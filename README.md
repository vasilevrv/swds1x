# Salu Workshop SWPLUG1x firmware

Zigbee smart-plug firmware for **Telink TLSR8258 with 1 MiB flash**, adapted from the Telink SDK's `sampleLight` application. BL0937 is the default energy meter; HLW8032 is supported as a build-time alternative. Both configurations identify themselves in the Basic cluster as manufacturer `Salu Workshop`, model `SWPLUG1x`.

Thanks to Telink Semiconductor and the Telink Zigbee SDK contributors for the original application, drivers and protocol stack. Upstream copyright and license notices are retained. This repository contains the smart-plug application, its bootloader adaptation, build tools, host tests and Zigbee2MQTT converter; the SDK is obtained separately.

**Status:** host checks passed on 2026-10-03, including the current build 04. A successful TC32 build is required before flashing; no TC32 compiler is installed in the current development environment. Meter calibration and the PC5 ADC configuration still require board validation. Production readiness is not established. Host checks do not replace TC32 compilation/linking or hardware testing.

Detailed bootloader build, BIN merging and flashing instructions: [BUILD_AND_FLASH.md](BUILD_AND_FLASH.md). Energy-accounting limitations and review results: [REVIEW.md](REVIEW.md). These additional development notes remain in Russian.

## Meter selection

In `meter_config.h`:

```c
#define SMART_PLUG_METER_TYPE SMART_PLUG_METER_BL0937
/* For a future HLW8032 board: SMART_PLUG_METER_HLW8032 */
```

Selection happens at build time; there is no automatic detection. The BL0937 firmware requires a **physically installed BL0937** and does not decode HLW8032 UART data. Alternatively, pass `EXTRA_CPPFLAGS=-DSMART_PLUG_METER_TYPE=SMART_PLUG_METER_HLW8032`. Make rebuilds dependent objects when configuration headers change.

### BL0937: pulses, energy and calibration

| BL0937 pin | MCU GPIO | Function |
|---|---|---|
| 6 CF | PC3 | Active power and accumulated energy |
| 7 CF1 | PC2 | Current / voltage |
| 8 SEL | PB4 | MCU output: 0 = current, 1 = voltage |

CF and CF1 use separate falling-edge GPIO interrupts, RISC0/RISC1. Energy is counted from CF independently of SEL switching. Each pulse adds `BL0937_POWER_UW_PER_HZ / 3600` micro-watt-hours; fractional remainder is retained in RAM. A 32-bit pulse counter wrap is handled by modular subtraction. After restart, the software pulse counter starts again and accumulated energy is restored through the existing NV mechanism. Pulses during restart or prolonged interrupt masking cannot be recovered. Sudden power loss can lose energy accumulated since the last NV save.

After switching SEL, the driver waits one second and measures for two seconds. A complete voltage/current cycle takes approximately six seconds; initial readings appear after the first complete cycle. The latest readings are reused between updates and become invalid after nine seconds without an update. A 100 ms report interval does not provide fresh voltage/current measurements every 100 ms. Power is calculated from the CF period and becomes zero after ten seconds without pulses. Very small loads can produce less frequent pulses and unstable power readings.

The initial coefficients in `meter_config.h` were calculated for divider `(1991000 + 998) / 998` and an R001 shunt. Voltage was adjusted using the user's multimeter: the plug reported 294.9 V while the reference reported 222.8 V, changing the coefficient from 157895 to 119291 microV/Hz. This single-point correction still needs repeat verification.

Current was provisionally adjusted from 12870 to 10213 microA/Hz using three consistent reference/plug pairs: 2.004/2.52, 1.974/2.49 and 1.60/2.02 A. The 1.82/2.15 A pair differs and should be rechecked under a stable load. The active-power/energy coefficient remains nominal. Calibrate voltage, current and power separately:

```text
new coefficient = old coefficient × reference / reported value
```

The power coefficient also determines energy per pulse. Changing coefficients does not recalculate previously saved energy.

A CF period of 180 microseconds or less marks measurements invalid; subsequent pulses of that signal are not counted as energy. The first CF pulse after startup or timeout is confirmed after 180 microseconds so an overload signal at startup does not add energy. This filters the diagnostic overload signal; it is not hardware protection or automatic relay shutoff.

References: [Belling BL0937 application note](https://www.belling.com.cn/media/file_object/bel_product/BL0937/guide/BL0937%20APP%20Note_V1.0_en.pdf) and [ESPHome BL0937 formulas](https://github.com/esphome/esphome/blob/dev/esphome/components/hlw8012/hlw8012.cpp).

## Board pinout

Pins and active levels from the user's latest continuity checks are defined in `board_smart_plug_8258.h`:

| Signal | MCU GPIO / IC pin reported by the user | Active level / mode |
|---|---|---|
| Relay | PB5 / 15 | Active high |
| LED | PD2 / 31 | Active low |
| Button | PC4 / 24 | Active low, 10 kOhm pull-up |
| BL0937 CF / HLW8032 TX | PC3 / 23 | GPIO interrupt / hardware UART RX |
| BL0937 SEL / HLW8032 RX | PB4 / 14 | Output 0/1 / high impedance |
| BL0937 CF1 / HLW8032 PF | PC2 / 22 | GPIO interrupt / reserved input |
| SDK flash-protection ADC | PC5 | Provisional; requires an unused floating pin |

After checking a board's wiring, update this header. It also contains HLW8032 voltage and current coefficients derived from board resistors.

For HLW8032, PB4 does not support TLSR8258 hardware UART TX. Reading the meter does not require transmission: its RX pin is reserved. MCU TX is disabled with `UART_TX_NONE_PIN`, so PC2 carrying PF is not configured as UART TX.

Measured resistors: IP/IN are 0.998 kOhm each, nominally 1 kOhm; the VP resistor chain is 1.991 MOhm, comprising two nominal 1 MOhm resistors. The IP/IN resistors are filters, not the power shunt. The reported shunt marking is R001 = 0.001 Ohm, giving nominal current coefficient 1.000 (`HLW8032_CURRENT_COEFFICIENT_MILLI = 1000`).

VP–GND is treated as 0.998 kOhm = 998 Ohm based on the user's measurements. The voltage coefficient is `1991000 / (998 × 1000) ≈ 1.99498998`, represented as `HLW8032_VOLTAGE_COEFFICIENT_MILLI = 1995`. These coefficients apply to voltage, current, power and PF energy increments. Reference calibration is still required; in-circuit resistor measurements do not replace it. Changing coefficients does not recalculate saved energy. Factory reset provides a fresh accounting start but also clears the Zigbee network and settings.

## Controls

- A short button press toggles the relay.
- Each actual relay-state change flashes the LED for 150 ms.
- Holding the button for ten seconds turns the relay off, flashes the LED three times, clears the Zigbee network and accumulated energy, then restarts and searches for a network.
- Relay state is restored after restart through the On/Off attribute.

## Zigbee endpoint 1

Device ID: `0x0051` (Smart Plug).

Server clusters:

| Cluster | ID |
|---|---|
| Basic | `0x0000` |
| Identify | `0x0003` |
| Groups | `0x0004` |
| Scenes | `0x0005` |
| On/Off | `0x0006` |
| Metering | `0x0702` |
| Electrical Measurement | `0x0B04` |
| Smart Plug Configuration | `0xFC00` |

Client cluster: OTA Upgrade `0x0019`.

### Telemetry interval

Cluster `0xFC00`, attribute `0x0000`, type `uint32`, read/write. The value is in milliseconds, range `100..3600000`, default `10000`. Out-of-range values are clamped to the nearest bound.

During the interval, readings from the selected driver are accumulated and averaged. At the end of the window, Electrical Measurement and Metering Report Attributes are sent to coordinator address `0x0000`, endpoint 1.

This setting exclusively controls periodic measurement reports. Metering and Electrical Measurement attributes are readable and included in outgoing reports, but do not support standard Configure Reporting: otherwise the SDK would generate additional reports outside the selected interval. Standard Configure Reporting is supported for On/Off. Coordinator integrations must account for this distinction.

## Attribute units

| Attribute | Representation |
|---|---|
| RMSVoltage | 0.1 V |
| RMSCurrent | 1 mA |
| ActivePower | 1 W |
| ApparentPower | 1 VA |
| PowerFactor | Percent |
| CurrentSummationDelivered | Raw mWh; multiplier 1, divisor 1000000, yielding kWh |
| InstantaneousDemand | Raw mW with the same divisor, yielding kW |

HLW8032 uses UART `4800 8E1` with 24-byte frames. Sending a frame takes approximately 55 ms; the actual frame interval needs board verification. Accumulated energy and the telemetry interval are saved to flash every five minutes, when the relay turns off and when the interval changes. Sudden power loss can lose increments since the last checkpoint, normally up to five minutes with healthy flash. The checkpoint interval limits flash wear.

## HLW8032: energy accounting through PF REG

Energy is calculated from increments of the UART pulse counter without a separate PF GPIO connection. The 16-bit PF REG and bit 7 of Data Update REG form a 17-bit counter. Differences are calculated modulo 131072, covering both carry transitions, `0 → 1` and `1 → 0`. Fractional pulse-to-micro-watt-hour remainder remains in RAM rather than being lost on every frame.

After MCU startup, the total is restored from flash and the first valid HLW8032 frame establishes a new PF baseline. A previous session's PF value is not subtracted from the new counter. A gap longer than two seconds without valid frames also resets the baseline because energy during the gap is unknown. The first absolute PF value is not added to the total: the MCU may restart independently of the meter.

A rapid HLW8032 reset is filtered using the maximum plausible pulse increment for the elapsed time. `HLW8032_MAX_POWER_MW` is a provisional 25000 W plausibility limit with a two-pulse margin, not overload protection. A reset close to the wrap boundary can look like a valid wrap; UART provides no separate startup identifier. Guaranteed detection requires a separate power/reset signal.

Reception timestamps are stored alongside UART bytes, preventing main-loop delay from turning queued frames into simultaneous measurements. Multiple frames in one DMA packet are timestamped approximately using the UART rate; IRQ service latency remains a hardware limitation. Parity errors clear the RX FIFO, after which the parser searches for the next complete frame.

## Build

Requirements: GNU Make, Python 3 and a host-compatible **TC32** toolchain providing `tc32-elf-gcc`, `tc32-elf-ld`, `tc32-elf-objcopy` and `tc32-elf-size`. No IDE is required. Clang and ARM GCC cannot build TC32 instructions.

The SDK is not bundled. To reproduce the development layout:

```sh
mkdir telink-plug
cd telink-plug
git clone https://github.com/vasilevrv/tlsr8258-socket-pwmeter.git smart_plug
git clone https://github.com/telink-semi/telink_zigbee_sdk.git telink_zigbee_sdk
git -C telink_zigbee_sdk checkout 7cf1d6dbac6497505d649bd03342efe81f1a8212
make -C smart_plug -j4 TOOLCHAIN_PREFIX=/absolute/path/to/bin/tc32-elf-
```

If the tools are in `PATH`, omit `TOOLCHAIN_PREFIX`. The prefix includes the complete tool-name prefix, ending in `tc32-elf-`. For a different SDK location, pass `SDK_DIR=/absolute/path/to/tl_zigbee_sdk`.

Build OTA with the same toolchain settings:

```sh
make -C smart_plug -j4 ota TOOLCHAIN_PREFIX=/absolute/path/to/bin/tc32-elf-
```

Output: `smart_plug/build/boot1/smart_plug.zigbee`. OTA upload procedure: [OTA_UPDATE.md](OTA_UPDATE.md).

Bootloader mode is required, matching the SDK b85 project:

```text
smart_plug/build/boot1/smart_plug.elf
smart_plug/build/boot1/smart_plug.map
smart_plug/build/boot1/smart_plug.raw.bin
smart_plug/build/boot1/smart_plug.bin
```

Flash `smart_plug.bin` at address `0x10000`, with a compatible bootloader installed separately at address `0`. A factory bootloader on a purchased plug is not automatically compatible. The configured target is TLSR8258 with 1 MiB flash; this SDK explicitly requires bootloader mode for that target, so Make rejects `BOOT_LOADER_MODE=0`.

Build a bootloader or a combined factory image:

```sh
make -C smart_plug -j4 bootloader TOOLCHAIN_PREFIX=/absolute/path/to/bin/tc32-elf-
make -C smart_plug -j4 combined TOOLCHAIN_PREFIX=/absolute/path/to/bin/tc32-elf-
```

The combined output is `smart_plug/build/smart_plug_factory.bin`. See [BUILD_AND_FLASH.md](BUILD_AND_FLASH.md) before flashing.

The Makefile includes startup, linker script, SDK sources and b85 Router libraries. The portable `tools/prepare_firmware.py` replaces the SDK's platform-specific utility: it pads the BIN with FF to a 16-byte boundary, installs CRC marker `5D 02`, writes image size into the header and appends CRC32 without a final XOR. Its format was checked against `tl_check_fw2.out` and `ota_newImageValid()`.

Application size is limited to a 424 KiB slot (`0x6A000`); OTA storage starts at `0x7A000`. The limit includes padding and CRC. `.raw.bin` is not ready to flash because it lacks this processing. A full TC32 ELF/BIN has not yet been built in the current environment; Make rules, source syntax and image processing have been checked with host tools.

### Integration into an existing SDK project

Application source files are listed in `sources.mk`. Include it in an SDK Make project and add `SMART_PLUG_SOURCES` to the source list. Exclude the original `apps/zigbee/sampleLight` application and add `smart_plug` to include paths. Do not include `tests` in the firmware build.

Target definitions: `MCU_CORE_8258=1`, `ROUTER=1`, `ZB_STACK_USED=1`, `__PROJECT_TL_SMART_PLUG__=1`. The old SDK definition `__PROJECT_TL_DIMMABLE_LIGHT__=1` remains supported for migration. Use the SDK's 8258 bootloader settings, linker script and TC32 libraries. `sources.mk` is not a standalone firmware Makefile.

## Incoming ZCL validation

`zcl_guard.c` validates headers, value/record lengths, control-command direction, Groups/Scenes fields and OTA responses before forwarding packets to the SDK. Incomplete messages, invalid StartUpOnOff/boolean values, unsupported commands and compound types are dropped without a ZCL response. This device does not use compound attribute types. A misconfigured coordinator may observe a timeout when a packet is rejected.

Out-of-range or stale measurements are marked invalid; energy accounting is independent of RMS validity. The common stale timeout is two seconds, while BL0937 voltage/current has its driver-specific nine-second window. Metering Status includes Check Meter when valid measurements are unavailable. Values that do not fit RMSCurrent, ApparentPower or InstantaneousDemand are reported as invalid rather than truncated. With the current scaling, InstantaneousDemand is limited to 8388.607 W; Electrical Measurement can represent higher power up to the hardware plausibility limit.

## Zigbee2MQTT

Use [zigbee2mqtt/smart_plug_hlw8032.mjs](zigbee2mqtt/smart_plug_hlw8032.mjs). It supports the current `SWPLUG1x` identity, both meter variants, relay control, startup behavior, electrical readings, energy and the custom telemetry interval. Installation and MQTT examples: [zigbee2mqtt/README.md](zigbee2mqtt/README.md).

## Checks

From the `telink-plug` workspace:

```sh
make -C smart_plug check
make -C smart_plug sources
```

Or run `python3 smart_plug/tests/run_checks.py`. Python 3 and Clang are required. Checks compile actual driver, energy and On/Off logic with hardware API substitutes, use AddressSanitizer/UndefinedBehaviorSanitizer, and check C syntax against real SDK headers for a 32-bit Clang target. The current suite also covers bootloader validation, ZCL fuzzing, controls, converter behavior, firmware packaging and OTA version `0x10044001`. These checks passed on 2026-10-03. They do not link TC32 firmware or exercise physical hardware. Further review notes: [REVIEW.md](REVIEW.md).
