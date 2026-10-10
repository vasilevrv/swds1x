# Zigbee2MQTT: Salu Workshop swds1x

Version 2026101006 removes ADC diagnostics. Update the converter, install the OTA image and restart Zigbee2MQTT. Battery measurement uses a five-second idle window at startup and then every 12 hours.

The converter exposes `contact`, `battery` (%) and `voltage` (mV, with 100 mV resolution). Devices upgrading from a build without `genPowerCfg` need a new interview/configuration. If necessary, remove the old device entry, enable permit join and press/release D3 to pair again.

Firmware calculates the percentage using the approximate CR2032 curve described in the main README. The converter binds Power Configuration to the coordinator and reads both attributes. Firmware controls reporting: once after startup and network connection, then every 12 hours. The converter disables separate SDK reporting timers (`maximumReportInterval: 0xFFFF`) to avoid extra wakes and duplicate reports. After replacing the converter for an already paired device, run Reconfigure. Unknown 0xFF values are not shown as 0%.

## Setup

1. Add the contents of `swds1x.mjs` through **Settings > Dev console > External converters**, or place the file at `data/external_converters/swds1x.mjs` and restart Zigbee2MQTT.
2. Enable permit join.
3. Press and release D3. The LED indicates reset, then blinks while searching for the network.
4. Wait for interview/configuration to finish. Zigbee-herdsman handles IAS enrollment; the converter binds the `ssIasZone` cluster on endpoint 1 to the coordinator and reads the initial state.

See the [official external converter documentation](https://www.zigbee2mqtt.io/advanced/more/external_converters.html). A separate `external_converters` block in configuration.yaml is not needed for this installation method.

Example MQTT payload on `zigbee2mqtt/DEVICE`:

```json
{"contact":true}
```

`true` means the reed switch is closed and the door is closed; `false` means the reed switch is released and the door is open. Open and close the door to check that each stable transition produces one LED pulse and updates `contact`.

Model: `swds1x`. Manufacturer: `Salu Workshop`. If the device previously ran relay firmware, remove its old relay entry and pair again after installing sensor firmware.

Tamper is not exposed because no tamper input is implemented. Contact does not need separate attribute reporting configuration: transitions are sent through IAS Zone Status Change Notification, and `zoneStatus` can also be read.

The converter has been tested with real imports from zigbee-herdsman-converters 26.111.0. Reed switch events, the smooth LED pulse, button operation and OTA have been confirmed on the device. The new battery algorithm requires validation after updating.
