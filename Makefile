# Standalone TLSR8258/TC32 build. Run: make -C smart_plug -j4
.DEFAULT_GOAL := all
APP_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
SDK_DIR ?= $(APP_DIR)/../telink_zigbee_sdk/tl_zigbee_sdk
SDK_DIR := $(abspath $(SDK_DIR))
TOOLCHAIN_PREFIX ?= tc32-elf-
TC32_CC := $(TOOLCHAIN_PREFIX)gcc
TC32_LD := $(TOOLCHAIN_PREFIX)ld
TC32_OBJCOPY := $(TOOLCHAIN_PREFIX)objcopy
TC32_SIZE := $(TOOLCHAIN_PREFIX)size
PYTHON ?= python3
IMAGE_KIND ?= application
ifeq ($(IMAGE_KIND),application)
MAX_FIRMWARE_SIZE ?= 0x6A000
CONFIG_DIR := $(APP_DIR)
IMAGE_NAME := smart_plug
else ifeq ($(IMAGE_KIND),bootloader)
override MAX_FIRMWARE_SIZE := 0x10000
CONFIG_DIR := $(APP_DIR)/bootloader
IMAGE_NAME := bootloader
else
$(error IMAGE_KIND must be application or bootloader)
endif
BOOT_LOADER_MODE ?= 1
ifneq ($(BOOT_LOADER_MODE),1)
$(error This SDK requires BOOT_LOADER_MODE=1 for the configured TLSR8258 1M target)
endif
ifeq ($(IMAGE_KIND),application)
BUILD_DIR ?= $(APP_DIR)/build/boot$(BOOT_LOADER_MODE)
else
BUILD_DIR ?= $(APP_DIR)/build/bootloader
endif
BUILD_DIR := $(abspath $(BUILD_DIR))
TARGET := $(BUILD_DIR)/$(IMAGE_NAME)

SMART_PLUG_DIR := $(APP_DIR)
include $(APP_DIR)/sources.mk

# Match the linked source directories of the SDK b85 Eclipse project.
recursive_c = $(wildcard $(1)/*.c) $(foreach d,$(patsubst %/,%,$(wildcard $(1)/*/)),$(call recursive_c,$(d)))
SDK_SOURCE_DIRS := apps/common apps/zigbee/sampleCommon proj platform/chip_8258 \
    stack/common stack/hci stack/ieee802154/mac stack/ieee802154/phy stack/zigbee
ifeq ($(IMAGE_KIND),bootloader)
SMART_PLUG_SOURCES := $(CONFIG_DIR)/bootloader.c
SDK_SOURCE_DIRS := apps/common apps/bootLoader proj platform/chip_8258
endif
SDK_C_SOURCES := $(sort $(foreach d,$(SDK_SOURCE_DIRS),$(call recursive_c,$(SDK_DIR)/$(d))))
ifeq ($(IMAGE_KIND),bootloader)
# Same exclusion as build/bootLoader/b85/.cproject.
SDK_C_SOURCES := $(filter-out $(SDK_DIR)/apps/common/module_test.c $(SDK_DIR)/apps/bootLoader/bootloader.c,$(SDK_C_SOURCES))
endif
SDK_C_SOURCES += $(SDK_DIR)/platform/services/b85m/irq_handler.c
ASM_SOURCES := $(SDK_DIR)/platform/boot/8258/cstartup_8258.S \
    $(SDK_DIR)/platform/boot/link_cfg.S $(SDK_DIR)/platform/tc32/div_mod.S
INCLUDE_DIRS := $(APP_DIR) $(addprefix $(SDK_DIR)/,apps/common apps/zigbee/sampleCommon \
    platform/chip_8258 platform/tc32 platform proj/common proj \
    stack/common/includes stack/ieee802154/mac/includes stack/ieee802154/phy \
    stack/zigbee/common/includes stack/zigbee/mac stack/zigbee/zbapi \
    stack/zigbee/bdb/includes stack/zigbee/gp stack/zigbee/zcl stack/zigbee/ota \
    stack/zigbee/wwah stack/hci stack/zigbee/zb_hci)
ifeq ($(IMAGE_KIND),bootloader)
INCLUDE_DIRS := $(CONFIG_DIR) $(addprefix $(SDK_DIR)/,apps/common apps/bootLoader \
    platform/chip_8258 platform/tc32 platform proj/common proj)
endif
DEFINES := -DMCU_CORE_8258=1 -DROUTER=1 -DZB_STACK_USED=1 \
    -D__PROJECT_TL_SMART_PLUG__=1 -DBOOT_LOADER_MODE=$(BOOT_LOADER_MODE)
ifeq ($(IMAGE_KIND),bootloader)
DEFINES := -DMCU_CORE_8258=1 -D__PROJECT_TL_BOOT_LOADER__=1 -DBOOT_LOADER_MODE=1
endif
CPPFLAGS := $(DEFINES) $(addprefix -I,$(INCLUDE_DIRS)) $(EXTRA_CPPFLAGS)
CFLAGS := -std=gnu99 -O2 -ffunction-sections -fdata-sections -fpack-struct \
    -fshort-enums -fshort-wchar -fms-extensions -finline-small-functions $(EXTRA_CFLAGS)
ASFLAGS := -DMCU_STARTUP_8258
LINK_SCRIPT := $(SDK_DIR)/platform/boot/8258/boot_8258.link
LIB_DIRS := $(SDK_DIR)/platform/tc32 $(SDK_DIR)/platform/lib $(SDK_DIR)/stack/zigbee/lib/tc32_gcc
LIBRARIES := -lfirmware_encrypt -lsoft-fp -ldrivers_b85 -lzb_router
LIB_FILES := $(SDK_DIR)/platform/tc32/libfirmware_encrypt.a \
    $(SDK_DIR)/platform/tc32/libsoft-fp.a $(SDK_DIR)/platform/lib/libdrivers_b85.a \
    $(SDK_DIR)/stack/zigbee/lib/tc32_gcc/libzb_router.a
ifeq ($(IMAGE_KIND),bootloader)
LIBRARIES := -ldrivers_b85
LIB_FILES := $(SDK_DIR)/platform/lib/libdrivers_b85.a
endif
APP_OBJS := $(patsubst $(APP_DIR)/%.c,$(BUILD_DIR)/app/%.o,$(SMART_PLUG_SOURCES))
SDK_OBJS := $(patsubst $(SDK_DIR)/%.c,$(BUILD_DIR)/sdk/%.o,$(SDK_C_SOURCES))
ASM_OBJS := $(patsubst $(SDK_DIR)/%.S,$(BUILD_DIR)/sdk/%.o,$(ASM_SOURCES))
OBJS := $(APP_OBJS) $(SDK_OBJS) $(ASM_OBJS)

.PHONY: all bootloader combined ota check-tools check sources help FORCE
all: $(TARGET).bin
ifeq ($(IMAGE_KIND),application)
ota: $(TARGET).zigbee
$(TARGET).zigbee: $(TARGET).bin $(TARGET).raw.bin $(APP_DIR)/tools/make_ota.py $(APP_DIR)/tools/prepare_firmware.py $(APP_DIR)/tools/merge_firmware.py
	$(PYTHON) "$(APP_DIR)/tools/make_ota.py" "$(TARGET).raw.bin" "$(TARGET).bin" "$@"
else
ota:
	$(error OTA packages must contain the application, not the bootloader)
endif
bootloader:
	$(MAKE) -f "$(APP_DIR)/Makefile" IMAGE_KIND=bootloader all
combined:
	$(MAKE) -f "$(APP_DIR)/Makefile" IMAGE_KIND=application BUILD_DIR="$(APP_DIR)/build/boot1" all
	$(MAKE) -f "$(APP_DIR)/Makefile" IMAGE_KIND=bootloader BUILD_DIR="$(APP_DIR)/build/bootloader" all
	$(PYTHON) "$(APP_DIR)/tools/merge_firmware.py" \
	    "$(APP_DIR)/build/bootloader/bootloader.bin" "$(APP_DIR)/build/boot1/smart_plug.bin" \
	    "$(APP_DIR)/build/smart_plug_factory.bin"

check-tools:
	@command -v "$(TC32_CC)" >/dev/null || { echo "Missing $(TC32_CC). Set TOOLCHAIN_PREFIX=/path/to/bin/tc32-elf-"; exit 1; }
	@command -v "$(TC32_LD)" >/dev/null
	@command -v "$(TC32_OBJCOPY)" >/dev/null
	@command -v "$(TC32_SIZE)" >/dev/null
	@command -v "$(PYTHON)" >/dev/null
	@test -f "$(LINK_SCRIPT)"
	@$(foreach lib,$(LIB_FILES),test -f "$(lib)" || { echo "Missing $(lib)"; exit 1; };)

# Recompile if command-line build settings change; separate objects for each boot mode.
$(BUILD_DIR)/settings.txt: FORCE | check-tools
	@mkdir -p "$(@D)"
	@printf '%s\n' '$(TC32_CC)' '$(TC32_LD)' '$(CPPFLAGS)' '$(CFLAGS)' '$(ASFLAGS)' '$(LIBRARIES)' '$(MAX_FIRMWARE_SIZE)' > "$@.tmp"
	@cmp -s "$@.tmp" "$@" || cp "$@.tmp" "$@"
	@rm -f "$@.tmp"
FORCE:

$(BUILD_DIR)/app/%.o: $(APP_DIR)/%.c $(BUILD_DIR)/settings.txt
	@mkdir -p "$(@D)"
	$(TC32_CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c "$<" -o "$@"
$(BUILD_DIR)/sdk/%.o: $(SDK_DIR)/%.c $(BUILD_DIR)/settings.txt
	@mkdir -p "$(@D)"
	$(TC32_CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c "$<" -o "$@"
$(BUILD_DIR)/sdk/%.o: $(SDK_DIR)/%.S $(BUILD_DIR)/settings.txt
	@mkdir -p "$(@D)"
	$(TC32_CC) $(CPPFLAGS) $(ASFLAGS) -MMD -MP -c "$<" -o "$@"

$(TARGET).elf: $(OBJS) $(LINK_SCRIPT) $(LIB_FILES) $(APP_DIR)/Makefile $(APP_DIR)/sources.mk
	$(TC32_LD) --gc-sections -T "$(LINK_SCRIPT)" -Map "$(TARGET).map" -o "$@" \
	    $(OBJS) $(addprefix -L,$(LIB_DIRS)) --start-group $(LIBRARIES) \
	    "$$($(TC32_CC) -print-libgcc-file-name)" --end-group
	$(TC32_SIZE) "$@"
$(TARGET).raw.bin: $(TARGET).elf
	$(TC32_OBJCOPY) -O binary "$<" "$@"
$(TARGET).bin: $(TARGET).raw.bin $(APP_DIR)/tools/prepare_firmware.py
	$(PYTHON) "$(APP_DIR)/tools/prepare_firmware.py" "$<" "$@" --max-size $(MAX_FIRMWARE_SIZE)

check:
	$(PYTHON) "$(APP_DIR)/tests/run_checks.py"
	$(PYTHON) "$(APP_DIR)/tests/test_firmware.py"
sources:
	@printf '%s\n' $(SMART_PLUG_SOURCES) $(SDK_C_SOURCES) $(ASM_SOURCES)
help:
	@printf '%s\n' 'make -C smart_plug -j4' \
	    'make -C smart_plug -j4 ota              # application Zigbee OTA container' \
	    'make -C smart_plug -j4 bootloader        # SDK bootloader, UART update disabled' \
	    'make -C smart_plug -j4 combined          # build both and merge at 0 / 0x10000' \
	    'make -C smart_plug -j4 TOOLCHAIN_PREFIX=/path/to/bin/tc32-elf-' \
	    'make -C smart_plug check                  # host checks; no TC32 needed' \
	    'Default output: smart_plug/build/boot1/smart_plug.bin (flash at 0x10000, bootloader required)'

-include $(OBJS:.o=.d)
