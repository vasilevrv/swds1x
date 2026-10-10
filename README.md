# swds1x - Salu Workshop

Door/window sensor firmware for **TLSR8258F512ET32 with 512 KiB flash**. A Zigbee sleepy end-device powered by a CR2032, with Zigbee2MQTT support and OTA updates.

Current release: **2026101006**. OTA version: **0x110E4001**. [Release notes](RELEASE.md).

## Wiring

| Signal | GPIO | Level |
|---|---|---|
| LED | PB4 | LOW turns the LED on |
| Button | PD3 | LOW when pressed; internal pull-up |
| Reed switch | PD7 | HIGH when closed; internal pull-down |

Pin definitions: [sensor_pins.h](sensor_pins.h). This project targets the specified 512 KiB chip; images built for the previous 1 MiB layout are incompatible.

## Operation

- A new device automatically searches for a Zigbee network. Enable permit join on the coordinator; the LED blinks while searching.
- **One button press**, after 20 ms of debounce, clears network settings and starts pairing again. No hold is required. If the button is held during startup, release it first.
- Opening or closing the reed switch sends an IAS Zone event and starts a smooth, one-second LED pulse. A new transition immediately restarts the animation.
- The device sleeps with RAM retention when idle. The button and reed switch wake it immediately; parent polling runs every 5 seconds.
- Manufacturer: **Salu Workshop**. Model: **swds1x**. Endpoint: **1**. IAS Zone type: **Contact Switch**.

## Zigbee2MQTT

Install [swds1x.mjs](zigbee2mqtt/swds1x.mjs) as an external converter. [Setup instructions](zigbee2mqtt/README.md).

| Property | Meaning |
|---|---|
| `contact` | `true`: door closed; `false`: door open |
| `battery` | Estimated remaining charge, % |
| `voltage` | Supply voltage in mV, with 100 mV resolution |

After updating the converter, restart Zigbee2MQTT and run **Reconfigure** for an already paired device. ADC diagnostic properties were removed in this release.

## Battery

Measurement runs at startup and then **every 12 hours**, without a dedicated wake timer.

A five-second sampling window starts when the LED and Zigbee stack are idle. Every 500 ms, after 200 ms without activity, an ADC burst discards the first conversion and takes the median of the next five. The ADC is powered down between bursts. From up to ten valid medians, the lowest and highest are discarded and the rest are averaged. At least five valid bursts are required. The main loop continues servicing the button, reed switch and Zigbee throughout the window.

VDD is measured through GPIO PC5 driven HIGH, using the SDK's 1/8 input scaling and factory calibration. PC5 is not bonded out on ET32; no external connection is needed. Previous measurements are not blended into the new result.

The percentage is calculated before voltage rounding, with linear interpolation:

| Voltage | Charge |
|---|---|
| >=3.10 V | 100% |
| 3.00 V | 90% |
| 2.90 V | 70% |
| 2.80 V | 45% |
| 2.70 V | 20% |
| 2.60 V | 10% |
| 2.50 V | 5% |
| <=2.20 V | 0% |

This is an approximate CR2032 curve; available charge depends on load and temperature. An invalid periodic measurement preserves the last value until the next 12-hour interval. If the initial measurement fails, a retry is allowed after one minute on a normal device wake. Values remain unknown until the first valid result.

ADC readings matched a 3.3 V bench supply on the device. The new averaging algorithm still requires validation on a battery.

## Build

Requirements: **Telink TC32 GCC**, Python 3, GNU Make and the [Telink Zigbee SDK](https://github.com/telink-semi/telink_zigbee_sdk). Host tests additionally require Clang and Node.js.

Verified SDK revision: `7cf1d6dbac6497505d649bd03342efe81f1a8212`.

```sh
git clone https://github.com/vasilevrv/swds1x.git
git clone https://github.com/telink-semi/telink_zigbee_sdk.git
git -C telink_zigbee_sdk checkout 7cf1d6dbac6497505d649bd03342efe81f1a8212
cd swds1x
make -j4 combined TOOLCHAIN_PREFIX=/path/to/bin/tc32-elf-
make -j4 ota TOOLCHAIN_PREFIX=/path/to/bin/tc32-elf-
```

Omit `TOOLCHAIN_PREFIX` if TC32 is in PATH. Override the SDK path with `SDK_DIR=/absolute/path/to/tl_zigbee_sdk`. The build creates an adapted SDK copy in `build/sdk512`; the original SDK is unchanged.

| File | Purpose |
|---|---|
| `build/swds1x_factory.bin` | Bootloader and application; initial SWS programming at address 0 |
| `build/boot1/swds1x.bin` | Application at 0x10000; requires a compatible bootloader |
| `build/boot1/swds1x.zigbee` | Application update through Zigbee OTA |

[Initial flashing and flash layout](BUILD_AND_FLASH.md) - [OTA](OTA_UPDATE.md). Before initial programming, back up all 512 KiB and preserve the factory MAC and calibration. A full-chip erase is not required.

## Checks

```sh
make check
npm install --prefix /tmp/swds-node-check --ignore-scripts --no-audit --no-fund zigbee-herdsman-converters@26.111.0
SWDS_ZHC_DIR=/tmp/swds-node-check make check
```

Checks cover ASan/UBSan, GPIO/LED/sleep, debounce, button reset, IAS enrollment, event queues, join/rejoin, battery sampling, bootloader behavior, BIN/OTA CRC and size limits, 100,000 randomized ZCL packets and integration with the real Zigbee2MQTT converter library. Clang checks the complete source set against SDK headers: 179 application C files and 69 bootloader C files.

Host checks do not produce TC32 firmware. Final image size, sleep current and battery accuracy require a target build and device measurements. [Review and implementation history](REVIEW.md).

## Licensing

Telink-derived sources retain their original Apache-2.0 notices. The SDK is a separate dependency with its own licensing terms.
