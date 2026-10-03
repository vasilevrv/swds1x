#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t s32;
#define TRUE true
#define FALSE false
#define FLASH_TLNK_FLAG_OFFSET 8
#define FW_START_UP_FLAG_WHOLE 0x544c4e4b
#define FLASH_OTA_IMAGE_MAX_SIZE 0x6a000
#define FLASH_PROTECT_ENABLE 1
#define LED_PERMIT 0
#define SYSTEM_RESET() assert(!"unexpected reset")
#define REBOOT() assert(!"unexpected boot")

static u8 memory[0x100000];
static bool noAppFlg;
static u32 eraseCount, crcCount;
static void drv_disable_irq(void) {}
static void flash_unlock(void) {}
static void flash_lock(void) {}
static void gpio_toggle(int pin) { (void)pin; }
static void flash_read(u32 address, u32 size, u8 *data) {
    assert(address <= sizeof(memory) && size <= sizeof(memory) - address);
    memcpy(data, memory + address, size);
}
static void flash_write(u32 address, u32 size, u8 *data) {
    assert(address >= 0x7a000 && address + size <= 0xe4000);
    memcpy(memory + address, data, size);
}
static void flash_erase(u32 address) {
    assert(address >= 0x7a000 && address <= 0xe3000 && !(address & 4095));
    memset(memory + address, 0xff, 4096);
    eraseCount++;
}
static u32 xcrc32(u8 *data, u32 size, u32 crc) {
    (void)data; (void)size; (void)crc;
    crcCount++;
    return 0; /* Inputs use a mismatching stored CRC: never copy to application. */
}
#include "bootloader_sources.inc"

int main(void) {
    const u32 sizes[] = {0, 1, 3, 4, 35, 37, 0x6a001, 0xffffffff, 36, 0x69ff4};
    for (u32 i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        memset(memory, 0xff, sizeof(memory));
        memcpy(memory + 0x7a008, "KNLT", 4);
        memcpy(memory + 0x7a018, &sizes[i], 4);
        memory[0x7a006] = 0x5d; memory[0x7a007] = 0x02;
        eraseCount = crcCount = 0;
        bootloader_with_ota_check(0x10000, 0x7a000);
        bool valid = sizes[i] == 36 || sizes[i] == 0x69ff4;
        assert(eraseCount == (valid ? (sizes[i] + 4095) / 4096 : 1));
        assert((crcCount != 0) == valid);
        assert(noAppFlg);
    }
    puts("PASS: bootloader corrupt OTA sizes, CRC bounds, erase confined to OTA slot");
}
