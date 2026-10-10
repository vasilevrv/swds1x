# swds1x 2026101006

Source release of the **Salu Workshop swds1x** door/window sensor for **TLSR8258F512ET32**.

| Parameter | Value |
|---|---|
| Tag | `v2026101006` |
| SW build ID | `2026101006` |
| OTA file version | `0x110E4001` |
| OTA manufacturer code | `0x1141` |
| OTA image type | `0x02D4` |
| Flash | 512 KiB |
| Application / OTA slot | 152 KiB each |

## Features

- Sleepy Zigbee end-device using standard IAS Zone Contact Switch events.
- PB4 active-low LED; PD3 active-low button; PD7 active-high reed switch.
- Reset and pairing on one button press.
- Open/close events and a smooth, restartable one-second LED pulse.
- Zigbee2MQTT external converter exposing contact, battery and voltage.
- Zigbee OTA application updates, a separate bootloader and a factory image for SWS programming.

## Battery measurement changes

Battery measurement uses a five-second idle window at startup and then every 12 hours. Short ADC bursts produce medians; the lowest and highest medians are discarded and the rest are averaged. The ADC is powered down between bursts. Factory calibration is preserved. Startup diagnostic channels and attributes have been removed.

## Upgrade

For an existing swds1x with a compatible bootloader, build and install `build/boot1/swds1x.zigbee`, replace the converter, restart Zigbee2MQTT and run Reconfigure. Remove and reinsert the battery, then allow the first five-second window to complete and the report to arrive after network recovery.

To migrate from relay firmware, first program `build/swds1x_factory.bin` through SWS. Preserve factory MAC/calibration and reset network settings with the button.

[Build and SWS](BUILD_AND_FLASH.md) - [OTA](OTA_UPDATE.md) - [Zigbee2MQTT](zigbee2mqtt/README.md).

## Validation and release contents

The complete `make check` passed, including ASan/UBSan, syntax checks for 179 application C files and 69 bootloader C files, BIN/OTA packaging and integration with zigbee-herdsman-converters 26.111.0. [Review](REVIEW.md).

This release contains source code, the converter and documentation. Prebuilt BIN/OTA files are not included because the preparation environment has no TC32 compiler. Host packaging tests use synthetic images and do not establish final firmware size or successful target linking. The new battery algorithm and power consumption require device validation.
