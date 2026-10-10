# swds1x OTA

First install a compatible combined bootloader/application through SWS. Image type **0x02D4** distinguishes swds1x from relay firmware using 0x03D3; migration from a relay requires the combined image.

Current identity:

- Basic manufacturer: `Salu Workshop`; model: `swds1x`.
- OTA manufacturer code: 0x1141 (inherited Telink code).
- Image type: 0x02D4.
- File version: 0x110E4001; SW build ID: 2026101006.

Before a new release, increment APP_BUILD in `version_cfg.h` and update the SW build ID in `zcl_clusters.c` and the test assertions.

```sh
make -j4 ota TOOLCHAIN_PREFIX=/path/to/bin/tc32-elf-
```

Upload **`build/boot1/swds1x.zigbee`** as custom firmware in Zigbee2MQTT. A combined BIN, raw BIN or bootloader image cannot be used for this operation. The converter does not configure a public OTA provider; use the [Zigbee2MQTT custom firmware flow](https://www.zigbee2mqtt.io/guide/usage/ota_updates.html#using-custom-firmware-files-or-index).

Example MQTT request to `zigbee2mqtt/bridge/request/device/ota_update/update`:

```json
{"id":"DEVICE_IEEE_OR_FRIENDLY_NAME","url":"/app/data/ota/swds1x.zigbee"}
```

The file must be accessible to the Zigbee2MQTT process. The new version must be higher than the installed version. OTA preserves network settings and IAS enrollment; application OTA does not update the bootloader. Polling switches to 250 ms during download and returns to 5 seconds afterward. OTA operation has been confirmed on the device with earlier builds; each newly built image still requires validation.
