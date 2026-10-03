#!/usr/bin/env python3
"""Combine prepared bootloader/application BINs for TLSR8258 1 MiB flash.

Output starts at flash offset 0. It ends after the application, before OTA/NV.
Each input retains its own header and CRC. Do not run prepare_firmware on output.
"""
import argparse
from pathlib import Path
import struct
import tempfile
import zlib

APP_OFFSET = 0x10000
APP_SLOT_SIZE = 0x6A000


def validate(image: bytes, maximum: int, name: str) -> None:
    if not 36 <= len(image) <= maximum:
        raise ValueError(f"{name}: size must be 36..{maximum} bytes")
    if image[8:12] != b"KNLT" or image[6:8] != b"\x5d\x02":
        raise ValueError(f"{name}: expected prepared Telink BIN with CRC marker")
    if struct.unpack_from("<I", image, 0x18)[0] != len(image):
        raise ValueError(f"{name}: header size mismatch")
    if (len(image) - 4) % 16:
        raise ValueError(f"{name}: invalid payload alignment")
    if struct.unpack_from("<I", image, len(image) - 4)[0] != (zlib.crc32(image[:-4]) ^ 0xFFFFFFFF):
        raise ValueError(f"{name}: CRC mismatch")


def merge(bootloader: bytes, application: bytes) -> bytes:
    validate(bootloader, APP_OFFSET, "bootloader")
    validate(application, APP_SLOT_SIZE, "application")
    # SDK bootloader reserves this much RAM before its own ram_code and copies
    # the application's header-declared RAM code there before rebooting.
    if struct.unpack_from("<H", application, 0x0C)[0] * 16 > 0x4000:
        raise ValueError("application: RAM code exceeds the bootloader's 16 KiB reservation")
    return bootloader + b"\xff" * (APP_OFFSET - len(bootloader)) + application


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bootloader", type=Path)
    parser.add_argument("application", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    temporary = None
    try:
        image = merge(args.bootloader.read_bytes(), args.application.read_bytes())
        with tempfile.NamedTemporaryFile(dir=args.output.parent, prefix=args.output.name + ".",
                                         suffix=".tmp", delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(image)
        temporary.replace(args.output)
    except (OSError, ValueError) as error:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
        parser.exit(1, f"Firmware merge failed: {error}\n")
    print(f"Combined: {args.output} ({len(image)} bytes), flash offset 0x00000")


if __name__ == "__main__":
    main()
