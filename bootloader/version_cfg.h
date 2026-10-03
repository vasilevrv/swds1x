#pragma once
#include "comm_cfg.h"

#define CHIP_TYPE                      TLSR_8258_1M
#define FLASH_CAP_SIZE_1M              1
#define MANUFACTURER_CODE              MANUFACTURER_CODE_TELINK
#define IMAGE_TYPE                     ((CHIP_TYPE << 8) | IMAGE_TYPE_BOOTLOADER)
#define FILE_VERSION                   0x10014001
#define IS_BOOT_LOADER_IMAGE           1
#define RESV_FOR_APP_RAM_CODE_SIZE     0x4000
#define IMAGE_OFFSET                   BOOT_LOADER_IMAGE_ADDR
