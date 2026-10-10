# Building and initially flashing swds1x

Target: TLSR8258F512ET32, **512 KiB internal flash**, bootloader mode 1. Use Telink TC32 GCC; ARM and RISC-V GCC are incompatible.

```sh
make -j4 combined TOOLCHAIN_PREFIX=/path/to/bin/tc32-elf-
make -j4 ota TOOLCHAIN_PREFIX=/path/to/bin/tc32-elf-
```

The default SDK location is `../telink_zigbee_sdk/tl_zigbee_sdk`. Override it with `SDK_DIR=/absolute/path/to/tl_zigbee_sdk`. If TC32 is in PATH, omit `TOOLCHAIN_PREFIX`.

Outputs: `build/swds1x_factory.bin`, `build/bootloader/bootloader.bin`, `build/boot1/swds1x.bin` and `build/boot1/swds1x.zigbee`. The application slot is 0x26000 bytes at address 0x10000. Packaging checks size, headers, alignment and CRC. The application linker checks the TLSR8258 retention RAM boundary.

## TLSR8258F512 flash layout

- Bootloader: `0x00000..0x0FFFF`.
- Application: `0x10000..0x35FFF` (152 KiB).
- OTA: `0x36000..0x5BFFF` (152 KiB).
- Reserved/BLE NV: `0x5C000..0x5EFFF`.
- Zigbee NV: `0x5F000..0x74FFF`; factory-reset marker: `0x75000`.
- Factory MAC: `0x76000`; RF calibration: `0x77000`.
- Pre-install code: `0x78000`; higher sectors are unused.

The original SDK rejects B85/512K and stops a 1 MiB image on a 512 KiB chip. `tools/prepare_sdk512.py` creates a separate SDK copy with adapted checks and addresses. This is a project adaptation rather than an officially supported configuration of this SDK revision. Factory addresses follow the earlier Telink Zigbee 512K layout; verify them against the board's original dump.

Do not use the old 1 MiB BIN. Rebuild **both** images with `make combined`. The sensor uses a different OTA image type (`0x02D4`), so initial migration from a relay is performed through SWS.

## SWS programming

First back up the complete flash. Preserve factory MAC and RF calibration; a full-chip erase is not required.

For TlsrComSwireWriter, run the following from its directory with your own port and image paths:

```sh
SENSOR_PORT=/dev/tty.wchusbserial110
SENSOR_IMAGE=/absolute/path/to/swds1x/build/swds1x_factory.bin
python3 TLSR825xComFlasher-macos.py -p "$SENSOR_PORT" -b 460800 -t 70 rf 0x0 0x80000 original_flash.bin
```

Continue after successfully reading the complete backup:

```sh
python3 TLSR825xComFlasher-macos.py -p "$SENSOR_PORT" -b 460800 -t 70 es 0x36000 0x1000
python3 TLSR825xComFlasher-macos.py -p "$SENSOR_PORT" -b 460800 -t 70 wf 0x0 "$SENSOR_IMAGE"
SENSOR_SIZE=$(wc -c < "$SENSOR_IMAGE" | tr -d ' ')
python3 TLSR825xComFlasher-macos.py -p "$SENSOR_PORT" -b 460800 -t 70 rf 0x0 "$SENSOR_SIZE" sensor_readback.bin
cmp "$SENSOR_IMAGE" sensor_readback.bin
```

Erasing 0x36000 removes the OTA slot header so the bootloader cannot restore the previous application. After successful readback comparison, restart the device:

```sh
python3 TLSR825xComFlasher-macos.py -p "$SENSOR_PORT" -b 460800 -t 70 -r
```

Install the Zigbee2MQTT external converter, enable permit join, then press and release D3 to reset old network settings. Check D7 opening/closing, the B4 LED, button reset, network recovery after a coordinator restart and sleep current. PC5 is used by the SDK for supply measurements and is not bonded out on ET32.

`make check` runs host checks; it does not produce a TC32 BIN or program the device.
