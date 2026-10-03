#!/usr/bin/env python3
"""Run production meter logic with host substitutes, then check all C files against the SDK."""
from pathlib import Path
import os
import subprocess
import tempfile
import shlex
import shutil

app = Path(__file__).resolve().parents[1]
tests = app / "tests"
sdk = app.parent / "telink_zigbee_sdk" / "tl_zigbee_sdk"
compiler = os.environ.get("CC", "clang")

with tempfile.TemporaryDirectory(prefix="smart-plug-checks-") as tmp:
    tmp = Path(tmp)
    boot = (app / "bootloader" / "bootloader.c").read_text()
    boot = boot[boot.index("static bool is_valid_fw_bootloader"):
                boot.index("s32 otaChkDelayCb")]
    (tmp / "bootloader_sources.inc").write_text(boot)
    binary = tmp / "test_bootloader"
    subprocess.run([compiler, "-std=c99", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-I", str(tmp),
                    str(tests / "test_bootloader.c"), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
    # Replace only SDK/hardware headers; compile the actual production functions.
    sources = "\n".join((app / name).read_text().replace('#include "app_cfg.h"', '')
                        for name in ("hlw8032.c", "smart_plug.c"))
    (tmp / "meter_sources.inc").write_text(sources)
    for name in ("tl_common.h", "zb_api.h", "zcl_include.h"):
        (tmp / name).write_text('#include "host_sdk.h"\n')
    binary = tmp / "test_meter"
    subprocess.run([compiler, "-std=c99", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-I", str(tmp), "-I", str(tests),
                    "-I", str(app), str(tests / "test_meter.c"), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

    sources = "\n".join((app / name).read_text().replace('#include "app_cfg.h"', '')
                        for name in ("bl0937.c", "smart_plug.c"))
    (tmp / "bl0937_sources.inc").write_text(sources)
    binary = tmp / "test_bl0937"
    subprocess.run([compiler, "-std=c99", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function",
                    "-fsanitize=address,undefined", "-I", str(tmp), "-I", str(tests),
                    "-I", str(app), str(tests / "test_bl0937.c"), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

    network = (app / "zigbee_callbacks.c").read_text()
    network = network[network.index("static ev_timer_event_t *steerTimerEvt"):
                      network.index("void zbdemo_bdbInitCb", network.index("void smartPlug_networkTask"))]
    (tmp / "network_sources.inc").write_text(network)
    binary = tmp / "test_network"
    subprocess.run([compiler, "-std=c99", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function",
                    "-fsanitize=address,undefined", "-I", str(tmp), "-I", str(tests),
                    str(tests / "test_network.c"), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

    guard = (app / "zcl_guard.c").read_text()
    guard = guard[guard.index("/* Validate wire data"):guard.index("void smartPlug_zclRxHandler")]
    (tmp / "guard_sources.inc").write_text(guard)
    binary = tmp / "test_zcl_guard"
    subprocess.run([compiler, "-std=c99", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function",
                    "-fsanitize=address,undefined", "-I", str(tmp), "-I", str(tests),
                    str(tests / "test_zcl_guard.c"), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

    ui_sources = "\n".join((app / name).read_text()
        .replace('#include "app_cfg.h"', '').replace('#include "app_main.h"', '')
        for name in ("app_ui.c", "plug_control.c", "zcl_scenes.c"))
    clusters = (app / "zcl_clusters.c").read_text()
    ui_sources += "\n" + clusters[clusters.index("static status_t smartPlugConfigCommand"):
                                  clusters.index("#define ATTR_COUNT")]
    (tmp / "ui_sources.inc").write_text(ui_sources)
    binary = tmp / "test_ui"
    subprocess.run([compiler, "-std=c99", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function",
                    "-fsanitize=address,undefined", "-I", str(tmp), "-I", str(tests),
                    "-I", str(app), str(tests / "test_ui.c"), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
    (tmp / "control_sources.inc").write_text((app / "zcl_on_off.c").read_text()
        .replace('#include "app_cfg.h"', '').replace('#include "app_main.h"', '')
        .replace('#include "plug_control.h"', ''))
    binary = tmp / "test_control"
    subprocess.run([compiler, "-std=c99", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function",
                    "-fsanitize=address,undefined", "-I", str(tmp), "-I", str(tests),
                    "-I", str(app), str(tests / "test_control.c"), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

# Use the production Makefile flags; avoid a separate IDE include/configuration list.
preview = subprocess.check_output(["make", "-C", str(app), "-n", "all"], text=True)
commands = [shlex.split(line) for line in preview.splitlines() if " -c " in line]
command = next(parts for parts in commands if parts[parts.index("-c") + 1].endswith(".c"))
flags = command[1:]
for option in ("-c", "-o"):
    index = flags.index(option)
    del flags[index:index + 2]
flags = [arg for arg in flags if arg not in ("-MMD", "-MP", "-D__PROJECT_TL_SMART_PLUG__=1")]
base = [compiler, "-target", "i386-unknown-none-elf", "-fsyntax-only",
        "-Wno-unknown-attributes", "-Wno-int-to-pointer-cast", "-Wno-pointer-to-int-cast",
        "-Wno-ignored-optimization-argument"] + flags
metadata_check = '''#include "app_cfg.h"
#include "version_cfg.h"
#if FILE_VERSION != 0x10044001
#error "Unexpected application OTA version"
#endif
'''
subprocess.run(base + ["-D__PROJECT_TL_SMART_PLUG__=1", "-x", "c", "-"],
               input=metadata_check, text=True, check=True)
print("PASS: application OTA version 0x10044001 using production headers")
for meter in (1, 2):
    for symbol in ("__PROJECT_TL_SMART_PLUG__", "__PROJECT_TL_DIMMABLE_LIGHT__"):
        for source in sorted(app.glob("*.c")):
            subprocess.run(base + [f"-D{symbol}=1", f"-DSMART_PLUG_METER_TYPE={meter}", str(source)], check=True)
        print(f"PASS: all {len(list(app.glob('*.c')))} application C files against SDK headers ({symbol}, meter={meter})")
print("These checks do not link TC32 firmware or exercise physical hardware.")

# Check the complete source set from the standalone Makefile, not just app code.
sources = subprocess.check_output(["make", "-s", "-C", str(app), "sources"], text=True).splitlines()
for meter in (1, 2):
    flags = [arg for arg in base if not arg.startswith("-DBOOT_LOADER_MODE=")]
    flags += ["-D__PROJECT_TL_SMART_PLUG__=1", "-DBOOT_LOADER_MODE=1", f"-DSMART_PLUG_METER_TYPE={meter}"]
    for source in sources:
        if source.endswith(".c"):
            result = subprocess.run(flags + [source], stderr=subprocess.PIPE, text=True)
            if result.returncode:
                print(result.stderr)
                raise SystemExit(result.returncode)
    print(f"PASS: complete Makefile C source set ({sum(s.endswith('.c') for s in sources)} files, boot mode 1, meter={meter})")

# Check the separate SDK bootloader configuration too; still no TC32 link.
preview = subprocess.check_output(["make", "-C", str(app), "-n", "IMAGE_KIND=bootloader", "all"], text=True)
commands = [shlex.split(line) for line in preview.splitlines() if " -c " in line]
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
    result = subprocess.run([compiler, "-target", "i386-unknown-none-elf", "-fsyntax-only",
        "-Wno-unknown-attributes", "-Wno-int-to-pointer-cast", "-Wno-pointer-to-int-cast",
        "-Wno-ignored-optimization-argument"] + flags + [source], capture_output=True, text=True)
    if result.returncode:
        print(result.stderr)
        raise SystemExit(result.returncode)
    count += 1
print(f"PASS: SDK bootloader source set ({count} C files, local board, UART update disabled)")

if shutil.which("node"):
    subprocess.run(["node", "--check", str(app / "zigbee2mqtt/smart_plug_hlw8032.mjs")], check=True)
    subprocess.run(["node", str(tests / "test_converter.mjs")], check=True)
else:
    print("SKIP: converter checks require Node.js")
