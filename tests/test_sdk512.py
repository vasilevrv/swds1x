#!/usr/bin/env python3
"""Exercise the actual patched SDK flash-size check and NV address formulas."""
from pathlib import Path
import os
import re
import subprocess
import tempfile
sdk = Path(os.environ['SDK_DIR'])
hw = (sdk / 'proj/drivers/drv_hw.c').read_text()
check = hw[hw.index('static void internalFlashSizeCheck(void)'):hw.index('#if VOLTAGE_DETECT_ENABLE', hw.index('static void internalFlashSizeCheck(void)'))]
cfg = (sdk / 'proj/drivers/drv_nv_cfg.h').read_text()
# Compile production address expressions, rather than copying their values.
names = ('FLASH_ADDR_OF_MAC_ADDR_512K', 'FLASH_ADDR_OF_F_CFG_INFO_512K',
         'CFG_FACTORY_RST_CNT', 'NV_BASE_ADDRESS_ZB', 'NV_BASE_ADDRESS_BLE',
         'FLASH_OTA_IMAGE_MAX_SIZE', 'FLASH_ADDR_OF_OTA_IMAGE')
def definition(name):
    return re.findall(r'^#define\s+' + name + r'\s+(.+)$', cfg, re.M)[-1]
constants = '\n'.join('#define ' + n + ' ' + definition(n) for n in names)
constants += '\n#define FLASH_ADDR_OTA_IMAGE_END 0x5C000\n#define FLASH_ADDR_OF_APP_FW 0x10000\n'
for suffix, cap in (('1M',0x100000),('2M',0x200000),('4M',0x400000)):
    constants += f'\n#define FLASH_ADDR_OF_MAC_ADDR_{suffix} {cap-0x1000}\n#define FLASH_ADDR_OF_F_CFG_INFO_{suffix} {cap-0x2000}\n'
program = '''
#include <stdint.h>
#include <assert.h>
typedef uint32_t u32;
typedef uint8_t u8;
#define MCU_CORE_8258 1
#define FLASH_SIZE_512K 0x13
#define FLASH_SIZE_1M 0x14
#define FLASH_SIZE_2M 0x15
#define FLASH_SIZE_4M 0x16
static u32 g_u32MacFlashAddr, g_u32CfgFlashAddr;
#define FACTORY_CFG_BASE_ADD g_u32CfgFlashAddr
static u32 flash_read_mid(void) { return 0x136085; }
''' + constants + '\n' + check + '''
int main(void) {
    g_u32MacFlashAddr = FLASH_ADDR_OF_MAC_ADDR_512K;
    g_u32CfgFlashAddr = FLASH_ADDR_OF_F_CFG_INFO_512K;
    internalFlashSizeCheck();
    assert(g_u32MacFlashAddr == 0x76000 && g_u32CfgFlashAddr == 0x77000);
    assert(FLASH_OTA_IMAGE_MAX_SIZE == 0x26000);
    assert(FLASH_ADDR_OF_OTA_IMAGE == 0x36000);
    assert(FLASH_ADDR_OF_OTA_IMAGE + FLASH_OTA_IMAGE_MAX_SIZE == 0x5C000);
    assert(NV_BASE_ADDRESS_BLE == 0x5D000);
    assert(NV_BASE_ADDRESS_ZB == 0x5F000);
    /* Seven ordinary two-sector modules and an eight-sector keypair module. */
    assert(NV_BASE_ADDRESS_ZB + 14*4096 + 8*4096 == CFG_FACTORY_RST_CNT);
    assert(CFG_FACTORY_RST_CNT == 0x75000);
    assert(CFG_FACTORY_RST_CNT + 4096 <= g_u32MacFlashAddr);
    return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
    source = Path(directory) / 'flash.c'
    binary = Path(directory) / 'flash'
    source.write_text(program)
    subprocess.run([os.environ.get('CC','clang'), '-std=c99', '-Wall', '-Wextra', '-Werror', str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True, timeout=2)
assert definition('FLASH_ADDR_OF_MAC_ADDR_512K') == '0x76000'
assert '#define CFG_PRE_INSTALL_CODE 0x78000' in cfg
print('PASS: actual SDK JEDEC 512K startup check, aligned app/OTA, all NV bounds, MAC/calibration preservation')
