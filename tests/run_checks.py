#!/usr/bin/env python3
"""Host behavior tests and syntax checks against the real SDK; no TC32 firmware build."""
from pathlib import Path
import os
import subprocess
import tempfile
import shlex
import shutil

app = Path(__file__).resolve().parents[1]
tests = app / "tests"
compiler = os.environ.get("CC", "clang")

def run(args, **kwargs):
    subprocess.run(args, check=True, **kwargs)

with tempfile.TemporaryDirectory(prefix="relay-2ch-checks-") as directory:
    tmp = Path(directory)
    for name in ("tl_common.h", "zb_api.h", "zcl_include.h"):
        (tmp / name).write_text('#include "host_sdk.h"\n')
    boot = (app / "bootloader/bootloader.c").read_text()
    (tmp / "bootloader_sources.inc").write_text(boot[boot.index("static bool is_valid_fw_bootloader"):boot.index("s32 otaChkDelayCb")])
    network = (app / "zigbee_callbacks.c").read_text()
    (tmp / "network_sources.inc").write_text(network[network.index("static ev_timer_event_t *steerTimerEvt"):network.index("void zbdemo_bdbInitCb", network.index("void relay2_networkTask"))])
    guard = (app / "zcl_guard.c").read_text()
    (tmp / "guard_sources.inc").write_text(guard[guard.index("/* Validate wire data"):guard.index("void relay2_zclRxHandler")])
    sources = "\n".join((app / name).read_text().replace('#include "app_cfg.h"', '').replace('#include "bootloader/app_cfg.h"', '')
        for name in ("relay_2ch.c", "zcl_on_off.c", "relay_control.c", "app_ui.c", "zcl_scenes.c", "zcl_callbacks.c", "board_io.c"))
    (tmp / "relay_sources.inc").write_text(sources)
    for name in ("test_bootloader", "test_network", "test_zcl_guard", "test_buttons", "test_relay"):
        extra = [str(app / "button_engine.c")] if name in ("test_buttons", "test_relay") else []
        binary = tmp / name
        run([compiler, "-std=c99", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function", "-fsanitize=address,undefined",
             "-I", str(tmp), "-I", str(tests), "-I", str(app), str(tests / (name + ".c")), *extra, "-o", str(binary)])
        run([str(binary)])

# Use the production Makefile, including its actual SDK sources and flags.
# Syntax checks use the complete production GPIO configuration and produce no BIN.
for kind in ("application", "bootloader"):
    preview = subprocess.check_output(["make", "-C", str(app), "-n", "IMAGE_KIND=" + kind, "all"], text=True)
    commands = [shlex.split(line) for line in preview.splitlines() if " -c " in line]
    if kind == "application":
        startup = next(Path(command[command.index("-c") + 1]) for command in commands
                       if command[command.index("-c") + 1].endswith("/cstartup_8258.S"))
        import re
        assert re.search(r"\.org\s+0x0\s+tj\s+__reset\s+\.word\s+\(FILE_VERSION\)", startup.read_text()), \
            "SDK startup header changed: review the TLSR8258 OTA version offset"
    count = 0
    for command in commands:
        source = command[command.index("-c") + 1]
        if not source.endswith(".c"):
            continue
        flags = command[1:]
        for option in ("-c", "-o"):
            index = flags.index(option)
            del flags[index:index + 2]
        flags = [arg for arg in flags if arg not in ("-MMD", "-MP")]
        base = [compiler, "-target", "i386-unknown-none-elf", "-fsyntax-only", "-Wno-unknown-attributes",
                "-Wno-int-to-pointer-cast", "-Wno-pointer-to-int-cast", "-Wno-ignored-optimization-argument", *flags]
        if count == 0:
            # Without confirmed hardware pins, a normal firmware build must fail.
            result = subprocess.run(base + ["-DRELAY_BOARD_CONFIGURED=0", source], capture_output=True, text=True)
            assert result.returncode and "Relay GPIOs are not configured" in result.stderr
            metadata = '#include "app_cfg.h"\n#if !__PROJECT_TL_BOOT_LOADER__ && (FILE_VERSION != 0x11044001 || IMAGE_TYPE != 0x03D3)\n#error "Unexpected relay OTA identity"\n#endif\n'
            run(base + ["-x", "c", "-"], input=metadata, text=True)
        result = subprocess.run(base + [source], capture_output=True, text=True)
        if result.returncode:
            print(result.stderr)
            raise SystemExit(result.returncode)
        count += 1
    print(f"PASS: complete {kind} Makefile source set ({count} C files), real SDK headers, production GPIO configuration; unconfigured hardware build rejected")

run(["python3", str(tests / "test_firmware.py")])
if not shutil.which("node"):
    raise SystemExit("Node.js is required for converter checks")
run(["node", "--check", str(app / "zigbee2mqtt/relay_2ch.mjs")])
run(["node", str(tests / "test_converter.mjs")])
if os.environ.get("RELAY_ZHC_DIR"):
    run(["node", str(tests / "test_converter_integration.mjs"), os.environ["RELAY_ZHC_DIR"]])
print("All checks passed. No TC32 firmware was built or flashed; physical hardware/radio behavior remains to be verified.")
