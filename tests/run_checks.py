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

with tempfile.TemporaryDirectory(prefix="swds1x-checks-") as directory:
    tmp = Path(directory)
    for name in ("tl_common.h", "zb_api.h", "zcl_include.h"):
        (tmp / name).write_text('#include "host_sdk.h"\n')
    boot = (app / "bootloader/bootloader.c").read_text()
    (tmp / "bootloader_sources.inc").write_text(boot[boot.index("static bool is_valid_fw_bootloader"):boot.index("s32 otaChkDelayCb")])
    network = (app / "zigbee_callbacks.c").read_text()
    (tmp / "network_sources.inc").write_text(network[network.index("static ev_timer_event_t *steerTimerEvt"):network.index("void zbdemo_bdbInitCb", network.index("void sensor_networkTask"))])
    guard = (app / "zcl_guard.c").read_text()
    (tmp / "guard_sources.inc").write_text(guard[guard.index("/* Validate wire data"):guard.index("void sensor_zclRxHandler")])
    sdk = Path(os.environ.get("SDK_DIR", app / "../telink_zigbee_sdk/tl_zigbee_sdk")).resolve()
    pm = (sdk / "proj/drivers/drv_pm.c").read_text()
    (tmp / "wakeup_sources.inc").write_text(pm[pm.index("u8 drv_pm_wakeupPinValid"):pm.index("void drv_pm_wakeupPinLevelChange")])
    zone = (sdk / "stack/zigbee/zcl/security_safety/zcl_ias_zone.c").read_text()
    (tmp / "zone_wire_sources.inc").write_text(zone[zone.index("_CODE_ZCL_ status_t zcl_iasZone_statusChangeNotification"):zone.index("_CODE_ZCL_ static status_t zcl_enrollRspPrc")])
    main = (app / "app_main.c").read_text()
    (tmp / "startup_sources.inc").write_text(main[main.index("void user_init(bool isRetention)"):main.rindex("#endif")])
    (tmp / "battery_sources.inc").write_text("\n".join(line for line in (app / "battery.c").read_text().splitlines() if not line.startswith("#include")))
    (tmp / "battery_adc_sources.inc").write_text("\n".join(line for line in (app / "battery_adc.c").read_text().splitlines() if not line.startswith("#include")))
    runtime = "\n".join("\n".join(line for line in (app / name).read_text().splitlines() if not line.startswith("#include")) for name in ("board_io.c", "sensor_control.c", "app_ui.c"))
    (tmp / "runtime_sources.inc").write_text(runtime)
    clusters = (app / "zcl_clusters.c").read_text()
    ias = clusters[clusters.index("typedef struct {"):clusters.index("void sensor_attrsInit")]
    callback = (app / "zcl_callbacks.c").read_text()
    ias += callback[callback.index("static bool iasDirty"):callback.rindex("#endif")]
    (tmp / "ias_sources.inc").write_text(ias)
    for name in ("test_bootloader", "test_network", "test_zcl_guard", "test_sensor", "test_runtime", "test_ias", "test_startup", "test_battery", "test_battery_adc"):
        extra = [str(app / "sensor_engine.c")] if name in ("test_sensor", "test_runtime") else []
        if name == "test_battery": extra = [str(app / "battery_model.c")]
        binary = tmp / name
        run([compiler, "-std=c99", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function", "-fsanitize=address,undefined",
             "-I", str(tmp), "-I", str(tests), "-I", str(app), str(tests / (name + ".c")), *extra, "-o", str(binary)])
        run([str(binary)])

# Use the production Makefile, including its actual SDK sources and flags.
# Syntax checks use the complete production GPIO configuration and produce no BIN.
for kind in ("application", "bootloader"):
    preview = subprocess.check_output(["make", "-C", str(app), "-n", "SDK_DIR=" + os.environ.get("SDK_SOURCE_DIR", str(app / "../telink_zigbee_sdk/tl_zigbee_sdk")), "IMAGE_KIND=" + kind, "all"], text=True)
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
        if count == 0 and kind == "application":
            metadata = '#include "app_cfg.h"\n#if !END_DEVICE || ROUTER || !PM_ENABLE || ZB_MAC_RX_ON_WHEN_IDLE || ZCL_ON_OFF_SUPPORT || ZCL_GROUP_SUPPORT || ZCL_SCENE_SUPPORT || !ZCL_POWER_CFG_SUPPORT || !ZCL_IAS_ZONE_SUPPORT || (FILE_VERSION != 0x110E4001) || (IMAGE_TYPE != 0x02D4) || (CHIP_TYPE != TLSR_8258_512K) || FLASH_CAP_SIZE_1M || (APP_IMAGE_ADDR != 0x10000)\n#error "Incorrect sensor role/features/identity"\n#endif\n'
            run(base + ["-x", "c", "-"], input=metadata, text=True)
        result = subprocess.run(base + [source], capture_output=True, text=True)
        if result.returncode:
            print(result.stderr)
            raise SystemExit(result.returncode)
        count += 1
    print(f"PASS: complete {kind} Makefile source set ({count} C files), real SDK headers, production GPIO configuration; sleepy ED configuration")

run(["python3", str(tests / "test_sdk512.py")])
run(["python3", str(tests / "test_firmware.py")])
if not shutil.which("node"):
    raise SystemExit("Node.js is required for converter checks")
run(["node", "--check", str(app / "zigbee2mqtt/swds1x.mjs")])
run(["node", str(tests / "test_swds_converter.mjs")])
if os.environ.get("SWDS_ZHC_DIR"):
    run(["node", str(tests / "test_swds_integration.mjs"), os.environ["SWDS_ZHC_DIR"]])
print("All checks passed. Physical GPIO, power consumption and radio require target-board validation.")
