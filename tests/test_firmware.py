#!/usr/bin/env python3
"""Check firmware packaging against the SDK CRC algorithm and utility format."""
import importlib.util
import ast
import re
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

app_dir = Path(__file__).resolve().parents[1]
clusters = (app_dir / "zcl_clusters.c").read_text()
def zcl_string(name):
    literal = re.search(r"^#define\s+" + name + r"\s+(\{[^\n]+\})", clusters, re.M).group(1)
    values = ast.literal_eval("[" + literal[1:-1] + "]")
    assert values[0] == len(values) - 1
    return "".join(values[1:])

assert zcl_string("ZCL_BASIC_MFG_NAME") == "Salu Workshop"
assert zcl_string("ZCL_BASIC_MODEL_ID") == "SWPLUG1x"
assert zcl_string("ZCL_BASIC_SW_BUILD_ID").endswith("04")
print("PASS: Salu Workshop / SWPLUG1x Zigbee string lengths and build identity")

spec = importlib.util.spec_from_file_location("prepare_firmware", Path(__file__).resolve().parents[1] / "tools/prepare_firmware.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def sdk_crc(data):
    # Independent bitwise equivalent of SDK utility.c:xcrc32(), init=FFFFFFFF.
    crc = 0xFFFFFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0xEDB88320 if crc & 1 else 0)
    return crc


for size in (32, 33, 47, 48, 49, 1001):
    raw = bytearray((i * 7) % 256 for i in range(size))
    raw[8:12] = b"KNLT"
    struct.pack_into("<I", raw, 0x18, size)
    image = module.prepare(bytes(raw))
    assert len(image) == ((size + 15) // 16) * 16 + 4
    assert image[6:8] == b"\x5d\x02"
    assert image[size:-4] == b"\xff" * ((-size) % 16)
    assert struct.unpack_from("<I", image, 0x18)[0] == len(image)
    assert struct.unpack_from("<I", image, len(image)-4)[0] == sdk_crc(image[:-4])
    for invalid in (b"", image):
        try:
            module.prepare(invalid)
        except ValueError:
            pass
        else:
            raise AssertionError("Invalid or already processed input accepted")
print("PASS: BIN header, FF alignment, CRC matching SDK algorithm, invalid input rejection")

with tempfile.TemporaryDirectory(prefix="smart-plug-slot-") as tmp:
    root = Path(tmp)
    source, output = root / "raw.bin", root / "final.bin"
    tool = Path(module.__file__)
    for size, accepted in ((0x69FF0, True), (0x69FFF, False), (0x6A000, False)):
        raw = bytearray(size); raw[8:12] = b"KNLT"
        struct.pack_into("<I", raw, 0x18, size); source.write_bytes(raw)
        result = subprocess.run([sys.executable, str(tool), str(source), str(output)], capture_output=True)
        assert (result.returncode == 0) == accepted
    result = subprocess.run([sys.executable, str(tool), str(source), str(output),
                             "--max-size", "0x70000"], capture_output=True)
    assert result.returncode != 0
    raw = bytearray(32); raw[8:12] = b"KNLT"; struct.pack_into("<I", raw, 0x18, 32)
    source.write_bytes(raw)
    result = subprocess.run([sys.executable, str(tool), str(source), str(root / "missing" / "out.bin")],
                            capture_output=True)
    assert result.returncode != 0 and b"Firmware preparation failed" in result.stderr
    assert not list(root.glob("*.tmp"))
print("PASS: real SDK 424 KiB slot, padding/CRC boundary, oversized limit rejection")

spec = importlib.util.spec_from_file_location("merge_firmware", tool.parent / "merge_firmware.py")
merger = importlib.util.module_from_spec(spec)
spec.loader.exec_module(merger)

def prepared(size):
    raw = bytearray(size)
    raw[8:12] = b"KNLT"
    struct.pack_into("<I", raw, 0x18, size)
    return module.prepare(bytes(raw))

boot, app = prepared(0xFFF0), prepared(0x69FF0)
combined = merger.merge(boot, app)
assert combined[:len(boot)] == boot
assert combined[len(boot):0x10000] == b"\xff" * (0x10000 - len(boot))
assert combined[0x10000:] == app and len(combined) < 0x7A000
damaged = bytearray(app); damaged[32] ^= 1
large_ram = bytearray(32); large_ram[8:12] = b"KNLT"
struct.pack_into("<I", large_ram, 0x18, 32)
struct.pack_into("<H", large_ram, 0x0c, 0x401)
for bad_boot, bad_app in ((prepared(0x10000), app), (boot, prepared(0x6A000)),
                          (boot, bytes(damaged)), (b"", app), (boot, app[:-1]),
                          (boot, module.prepare(bytes(large_ram)))):
    try:
        merger.merge(bad_boot, bad_app)
    except ValueError:
        pass
    else:
        raise AssertionError("Invalid merge input accepted")
print("PASS: merged offsets, FF gap, independent CRCs, slot/CRC/input rejection")
