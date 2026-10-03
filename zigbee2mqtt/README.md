# Zigbee2MQTT: Salu Workshop SWPLUG1x

External ESM converter `smart_plug_hlw8032.mjs` for this firmware: `modelID=SWPLUG1x`, `manufacturerName=Salu Workshop`, endpoint 1. It also recognizes the older `SMART_PLUG_BL0937` and `SMART_PLUG_HLW8032` models with manufacturer `TELINK`. It is not intended for the board's factory Tuya firmware.

The filename is retained for compatibility with existing installations. Replace the installed converter instead of adding a second copy. After changing the firmware's model identity, refresh the device interview in Zigbee2MQTT.

## Installation

1. Copy `smart_plug_hlw8032.mjs` into `external_converters` beside Zigbee2MQTT's `configuration.yaml`. In Docker this is usually `/app/data/external_converters` inside the container; use the corresponding mounted data directory on the host.
2. Add this setting to the existing `advanced` section:

   ```yaml
   advanced:
     enable_external_js: true
   ```

   Do not create a duplicate `advanced` section. Current Zigbee2MQTT does not require a separate YAML `external_converters` list.
3. Restart Zigbee2MQTT. Alternatively, add the file contents through Settings → Dev console → External converters.
4. Enable Permit join and hold the plug's button for ten seconds. This factory reset clears the network, accumulated energy and device settings. If the device is already joined with this firmware, try Reconfigure before resetting it.

## Exposed properties

| Property | Value |
|---|---|
| `state` | ON / OFF; SET also supports TOGGLE |
| `power_on_behavior` | off / on / toggle / previous |
| `voltage` | V, RMS averaged over the window |
| `current` | A, RMS averaged over the window |
| `power` | W, average active power with 1 W resolution |
| `apparent_power` | VA, average apparent power |
| `power_factor` | 0…1, ratio of average active to apparent power |
| `energy` | kWh, accumulated until factory reset |
| `telemetry_interval_ms` | Integer 100…3600000 ms; default 10000 |

Unavailable measurements are published as `null` rather than large numeric values. Scaling matches the current firmware: V/10, A/1000, W/1 and energy/1000000. Update the converter if C attribute scaling changes.

Energy is a cumulative counter and is not averaged. The interval defines the electrical-measurement aggregation window and the transmission period for both reports. At 100 ms, a window may contain no new HLW8032 frame.

## MQTT

Topic `zigbee2mqtt/PLUG_NAME/set`:

```json
{"state":"ON"}
```

```json
{"telemetry_interval_ms":10000}
```

```json
{"power_on_behavior":"previous"}
```

Topic `zigbee2mqtt/PLUG_NAME/get`:

```json
{"telemetry_interval_ms":"","energy":"","power":""}
```

The firmware persists the interval. Converter Configure reads it rather than replacing the user's setting with the default. Configure Reporting is used only for On/Off. Firmware measurement attributes are read-only and sent by its own scheduler to coordinator endpoint 1; standard Configure Reporting does not apply to them.

The converter enables `ota: true`. Upload this project's firmware as a custom file through Zigbee2MQTT; no public image server is configured. Building `.zigbee` images and uploading them is described in [OTA_UPDATE.md](../OTA_UPDATE.md), currently in Russian.

The converter was prepared against the firmware sources and Zigbee2MQTT API. Host checks passed; loading it into a real Zigbee2MQTT instance and end-to-end board communication were not established by those checks.

After updating the converter, restart Zigbee2MQTT and Reconfigure the device. Explicit `name` fields are defined for the custom cluster and attribute because herdsman uses them for routing and parsing Read Attributes Response. After SET, the converter returns the actual read-back interval.

The attribute explicitly declares `read: true` and `write: true`. Without `write: true`, herdsman rejects SET locally with NOT_AUTHORIZED before transmitting a Zigbee request. Invalid values converted by herdsman to NaN/null are handled alongside the firmware's raw numeric invalid markers.

References:

- [External converters](https://www.zigbee2mqtt.io/advanced/more/external_converters.html)
- [enable_external_js](https://www.zigbee2mqtt.io/guide/configuration/all-settings.html#enable_external_js)
