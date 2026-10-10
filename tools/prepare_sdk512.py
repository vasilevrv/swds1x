#!/usr/bin/env python3
"""Create a private SDK copy for TLSR8258F512; never modify the supplied SDK."""
from pathlib import Path
import hashlib
import shutil
import sys

source, output = map(lambda p: Path(p).resolve(), sys.argv[1:])
if source == output or source in output.parents:
    raise SystemExit('SDK copy must be outside its source tree')
paths = ('proj/drivers/drv_nv_cfg.h', 'proj/drivers/drv_hw.c')
texts = {p: (source / p).read_text() for p in paths}
fingerprint = hashlib.sha256(Path(__file__).read_bytes() + b''.join(
    str((p.relative_to(source), p.stat().st_size, p.stat().st_mtime_ns)).encode()
    for p in sorted(source.rglob('*')) if p.is_file())).hexdigest()
marker = output / '.swds512-fingerprint'
if not marker.exists() or marker.read_text() != fingerprint:
    cfg = texts[paths[0]]
    old = '#error "B85(512k) not support"'
    if cfg.count(old) != 1:
        raise SystemExit('Unsupported SDK flash configuration; review required')
    cfg = cfg.replace(old, '#if !BOOT_LOADER_MODE\n#error "swds1x 512K requires bootloader mode"\n#endif')
    cfg += '''
/* SWDS512: preserve traditional Telink 512K MAC/calibration sectors.
 * Keep all SDK NV modules below 0x75000; sector 0x76000 contains MAC,
 * so pre-install code must not use FACTORY_CFG_BASE_ADD - 0x1000. */
#define FLASH_ADDR_OF_MAC_ADDR_512K 0x76000
#define FLASH_ADDR_OF_F_CFG_INFO_512K 0x77000
#if CHIP_TYPE == TLSR_8258_512K
#undef CFG_PRE_INSTALL_CODE
#define CFG_PRE_INSTALL_CODE 0x78000
#undef FLASH_ADDR_OTA_IMAGE_END
#define FLASH_ADDR_OTA_IMAGE_END 0x5C000
#endif
'''
    hw = texts[paths[1]]
    old = 'switch (pMid[2]) {'
    if hw.count(old) != 1:
        raise SystemExit('Unsupported SDK flash detection; review required')
    hw = hw.replace(old, old + '''
    case FLASH_SIZE_512K:
        g_u32MacFlashAddr = FLASH_ADDR_OF_MAC_ADDR_512K;
        g_u32CfgFlashAddr = FLASH_ADDR_OF_F_CFG_INFO_512K;
        break;
''')
    if output.exists():
        shutil.rmtree(output)
    shutil.copytree(source, output)
    (output / paths[0]).write_text(cfg)
    (output / paths[1]).write_text(hw)
    marker.write_text(fingerprint)
print(output)
