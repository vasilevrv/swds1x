#!/usr/bin/env python3
"""Wrap this project's application BIN in an unencrypted Zigbee OTA container.

Read OTA identity from the matching raw BIN. TLSR8258's startup header begins
with a two-byte TJ instruction, followed by the four-byte version at 0x02.
Never wrap the combined factory BIN.
"""
import argparse
from pathlib import Path
import struct
import tempfile

from prepare_firmware import prepare
from merge_firmware import validate, APP_SLOT_SIZE


def package(raw: bytes, prepared: bytes) -> bytes:
    validate(prepared, APP_SLOT_SIZE, "application")
    if prepare(raw) != prepared:
        raise ValueError("Raw and prepared application BINs do not match")
    if struct.unpack_from("<H", prepared, 0x0C)[0] * 16 > 0x4000:
        raise ValueError("Application RAM code exceeds bootloader reservation")
    version = struct.unpack_from("<I", raw, 2)[0]
    manufacturer, image_type = struct.unpack_from("<HH", raw, 0x12)
    if manufacturer != 0x1141 or image_type != 0x03D3:
        raise ValueError("Expected TLSR8258 1M bootloader-mode application (1141/03D3)")
    total = 56 + 6 + len(prepared)
    # SDK checks the complete advertised OTA image size against this limit.
    if total > APP_SLOT_SIZE:
        raise ValueError("OTA container exceeds SDK OTA image size limit")
    header = struct.pack("<IHHHHHIH32sI", 0x0BEEF11E, 0x0100, 56, 0,
                         manufacturer, image_type, version, 2,
                         b"Salu Workshop swrelay2x".ljust(32, b"\0"), total)
    return header + struct.pack("<HI", 0, len(prepared)) + prepared


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("raw", type=Path)
    parser.add_argument("prepared", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    temporary = None
    try:
        raw, prepared = args.raw.read_bytes(), args.prepared.read_bytes()
        image = package(raw, prepared)
        with tempfile.NamedTemporaryFile(dir=args.output.parent, prefix=args.output.name + ".",
                                         suffix=".tmp", delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(image)
        temporary.replace(args.output)
    except (OSError, ValueError, struct.error) as error:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
        parser.exit(1, f"OTA packaging failed: {error}\n")
    manufacturer, image_type, version = struct.unpack_from("<HHI", image, 10)
    print(f"OTA: {args.output} ({len(image)} bytes), manufacturer=0x{manufacturer:04X}, "
          f"imageType=0x{image_type:04X}, fileVersion=0x{version:08X}")


if __name__ == "__main__":
    main()
