#!/usr/bin/env python3
"""Prepare a TC32 BIN using the format of SDK tools/tl_check_fw2.out.

The SDK utility pads the raw image to 16 bytes with FF, sets CRC markers at
offsets 6/7, updates size at 0x18, and appends reflected CRC32 (no final XOR).
The same CRC is checked by SDK ota_newImageValid() and xcrc32().
"""
import argparse
from pathlib import Path
import struct
import zlib
import tempfile

FLASH_SLOT_SIZE = 0x26000  # SDK TLSR8258F512: (0x5C000 - 0x10000) / 2.


def prepare(raw: bytes) -> bytes:
    if len(raw) < 32 or raw[8:12] != b"KNLT":
        raise ValueError("Input is not a raw Telink firmware image (missing TLNK header)")
    if raw[6:8] == b"\x5d\x02":
        raise ValueError("Input already has a CRC marker; use the .raw.bin from objcopy")
    # Catch a damaged input/link layout rather than silently generating a CRC for it.
    declared_size = struct.unpack_from("<I", raw, 0x18)[0]
    if declared_size != len(raw):
        raise ValueError(f"Raw image length {len(raw)} differs from linker header {declared_size}")
    image = bytearray(raw)
    image += b"\xff" * ((-len(image)) % 16)
    image[6:8] = b"\x5d\x02"
    struct.pack_into("<I", image, 0x18, len(image) + 4)
    crc = zlib.crc32(image) ^ 0xFFFFFFFF
    image += struct.pack("<I", crc)
    return bytes(image)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--max-size", type=lambda value: int(value, 0), default=FLASH_SLOT_SIZE,
                        help="Maximum final size in bytes (default: 0x26000 / 152 KiB)")
    args = parser.parse_args()
    temporary = None
    try:
        if not 0 < args.max_size <= FLASH_SLOT_SIZE:
            raise ValueError("Size limit must be within the TLSR8258F512 slot: 1..0x26000")
        image = prepare(args.input.read_bytes())
        if len(image) > args.max_size:
            raise ValueError(f"Image {len(image)} exceeds flash slot limit {args.max_size}")
        with tempfile.NamedTemporaryFile(dir=args.output.parent, prefix=args.output.name + ".",
                                         suffix=".tmp", delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(image)
        temporary.replace(args.output)
    except (OSError, ValueError) as error:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
        parser.exit(1, f"Firmware preparation failed: {error}\n")
    print(f"Firmware: {args.output} ({len(image)} bytes, CRC32={image[-4:].hex()})")


if __name__ == "__main__":
    main()
