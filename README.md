# swrelay2x

Two-channel Zigbee relay firmware for **Telink TLSR8258 with 1 MiB flash**. The device runs as a Zigbee router and exposes two independent relay endpoints, configurable local buttons, and button actions for automations.

- **Model:** `swrelay2x`
- **Manufacturer:** `Salu Workshop`
- **Hardware:** two relays, two control buttons, one service button, one status LED
- **Integration:** external Zigbee2MQTT converter included
- **Updates:** SWS programming for first installation; Zigbee OTA for subsequent application updates

This project uses the Telink Zigbee SDK. The power-meter drivers and telemetry from the original socket application have been removed.

## Hardware and pin configuration

Edit [relay_pins.h](relay_pins.h) to change GPIO assignments or active levels. This configuration is shared by the application and bootloader.

| Function | GPIO | Active level |
|---|---|---|
| Relay 1 / endpoint 1 | PC0 | High (`1`) turns the relay on |
| Relay 2 / endpoint 2 | PD4 | High (`1`) turns the relay on |
| Button 1 | PD3 | Low (`0`) means pressed |
| Button 2 | PD7 | Low (`0`) means pressed |
| Service/reset button | PC2 | Low (`0`) means pressed; external pull-up present |
| Status LED | PD2 | Low (`0`) turns the LED on |
| Supply-voltage ADC | PC5 | Must be an unused, floating ADC-capable GPIO |

The control buttons are configured as **active low**. Inputs receive an internal pull-up for active-low operation, or a pull-down for active-high operation. Outputs are initialized to their inactive level before their output drivers are enabled. Duplicate GPIO assignments and active levels other than `0`/`1` are rejected at compile time.

The SDK drives PC5 high when measuring supply voltage for flash-write protection, even though application-level voltage monitoring is disabled. Check that PC5 is physically unconnected. Check the MCU and flash capacity before programming: this build targets TLSR8258 / 1 MiB only.

Changing pin assignments requires rebuilding the firmware. For changes that must also apply during boot and OTA installation, rebuild and program the combined bootloader/application image.

## Build

### Requirements

- Telink Zigbee SDK, tested at commit `7cf1d6dbac6497505d649bd03342efe81f1a8212`
- Telink **TC32** toolchain providing `tc32-elf-gcc`, `tc32-elf-ld`, `tc32-elf-objcopy`, and `tc32-elf-size`
- GNU Make and Python 3
- For host checks: Clang with ASan/UBSan support and Node.js

ARM or RISC-V GCC cannot build this firmware. The SDK and toolchain are separate dependencies; they are not bundled in this repository.

### Checkout

The default SDK path is `../telink_zigbee_sdk/tl_zigbee_sdk`, relative to this repository:

```sh
git clone https://github.com/vasilevrv/swrelay2x.git
git clone https://github.com/telink-semi/telink_zigbee_sdk.git
git -C telink_zigbee_sdk checkout 7cf1d6dbac6497505d649bd03342efe81f1a8212
cd swrelay2x
```

For another SDK location, add `SDK_DIR=/absolute/path/to/tl_zigbee_sdk` to each build command. Point it at the inner SDK directory containing `apps`, `platform`, `proj`, and `stack`.

### Commands

If the TC32 tools are in `PATH`:

```sh
make -j4 combined    # Bootloader + application for first installation
make -j4 ota         # Application + Zigbee OTA container
```

Otherwise, supply the toolchain prefix, including its trailing hyphen:

```sh
make -j4 combined TOOLCHAIN_PREFIX=/path/to/bin/tc32-elf-
make -j4 ota TOOLCHAIN_PREFIX=/path/to/bin/tc32-elf-
```

Other targets:

```sh
make -j4             # Application only
make -j4 bootloader  # Bootloader only
make check           # Host tests and SDK syntax checks; no TC32 build or flashing
```

`BOOT_LOADER_MODE=1` is required for this target. Builds are incremental; changed headers and build settings trigger recompilation.

| Output | Purpose | Flash address |
|---|---|---|
| `build/relay_2ch_factory.bin` | Combined bootloader and application | `0x00000` |
| `build/bootloader/bootloader.bin` | Bootloader only | `0x00000` |
| `build/boot1/relay_2ch.bin` | Application only; compatible bootloader required | `0x10000` |
| `build/boot1/relay_2ch.zigbee` | Zigbee OTA container | Upload through Zigbee2MQTT |
| `build/boot1/relay_2ch.raw.bin`, `.elf`, `.map` | Build/debug artifacts | Do not use as the OTA upload |

The application slot is 424 KiB (`0x6A000`). Packaging checks sizes, alignment, CRC, and image identity. The combined image leaves OTA/NV/factory-data regions outside the programmed image.

## First installation over SWS

Build `combined`, then program `build/relay_2ch_factory.bin` at address `0x00000` using a compatible SWS programmer. Preserve a complete backup before replacing the original firmware. Do not erase the entire flash: factory MAC/calibration data must be retained.

For [TlsrComSwireWriter](https://github.com/pvvx/TlsrComSwireWriter), run the following from its directory. Replace the serial port and image path for your machine. The examples use a reduced UART baud rate of 460800.

```sh
RELAY_PORT=/dev/tty.wchusbserial110
RELAY_IMAGE=/absolute/path/to/swrelay2x/build/relay_2ch_factory.bin

# Back up the full 1 MiB flash. Continue only after a successful complete read.
python3 TLSR825xComFlasher-macos.py -p "$RELAY_PORT" -b 460800 -t 70 rf 0x0 0x100000 original_flash.bin

# Remove the old OTA header so it cannot replace the new application on boot.
python3 TLSR825xComFlasher-macos.py -p "$RELAY_PORT" -b 460800 -t 70 es 0x7A000 0x1000

# wf erases the sectors it writes; a separate full-chip erase is unnecessary.
python3 TLSR825xComFlasher-macos.py -p "$RELAY_PORT" -b 460800 -t 70 wf 0x0 "$RELAY_IMAGE"

# Verify the programmed bytes.
RELAY_SIZE=$(wc -c < "$RELAY_IMAGE" | tr -d ' ')
python3 TLSR825xComFlasher-macos.py -p "$RELAY_PORT" -b 460800 -t 70 rf 0x0 "$RELAY_SIZE" relay_readback.bin
cmp "$RELAY_IMAGE" relay_readback.bin
```

After all commands and the comparison succeed, restart:

```sh
python3 TLSR825xComFlasher-macos.py -p "$RELAY_PORT" -b 460800 -t 70 -r
```

After migrating from another firmware, hold the service button for 10 seconds to clear the old network/settings, enable joining in Zigbee2MQTT, and pair the device. Later application updates normally preserve settings.

## Zigbee2MQTT

Install [zigbee2mqtt/relay_2ch.mjs](zigbee2mqtt/relay_2ch.mjs) as an external converter and restart Zigbee2MQTT. See the [external converter documentation](https://www.zigbee2mqtt.io/advanced/more/external_converters.html) for your installation. The converter matches model `swrelay2x` and manufacturer `Salu Workshop`.

| Endpoint | Relay/button | Exposed properties |
|---|---|---|
| 1 | Relay 1, button 1 | `state_l1`, `power_on_behavior_l1` |
| 2 | Relay 2, button 2 | `state_l2`, `power_on_behavior_l2` |

### Runtime settings

These settings can be changed in Zigbee2MQTT without rebuilding:

| Property | Default | Behavior |
|---|---|---|
| `button_1_linked` | `true` | A single click toggles relay 1 |
| `button_2_linked` | `true` | A single click toggles relay 2 |
| `button_1_double_click` | `false` | Enables double-click detection on button 1 |
| `button_2_double_click` | `false` | Enables double-click detection on button 2 |
| `power_on_behavior_l1` | `previous` | Relay 1 startup policy: `off`, `on`, `toggle`, or `previous` |
| `power_on_behavior_l2` | `previous` | Relay 2 startup policy: `off`, `on`, `toggle`, or `previous` |

Publish to `zigbee2mqtt/DEVICE/set`, replacing `DEVICE` with the friendly name:

```json
{"state_l1":"ON","state_l2":"OFF"}
```

```json
{"button_1_linked":false,"button_2_linked":true,"button_1_double_click":true,"button_2_double_click":false}
```

```json
{"power_on_behavior_l1":"off","power_on_behavior_l2":"previous"}
```

Use JSON Booleans (`true`/`false`) for button settings. The converter reads back changed settings before publishing their confirmed values. To request a setting, publish to `zigbee2mqtt/DEVICE/get`:

```json
{"button_1_linked":"","button_2_double_click":""}
```

### Button behavior

Button actions are published on `zigbee2mqtt/DEVICE`:

```text
button_1_single    button_1_double    button_1_hold
button_2_single    button_2_double    button_2_hold
```

- **Double click disabled:** a short press emits `single` immediately after debounced release, without waiting for another click.
- **Double click enabled:** the second press must begin within **400 ms** of the first release. The 400 ms boundary is included. `double` is emitted when the second short press is released; no `single` is emitted for that pair.
- **No second press:** `single` is emitted after the 400 ms window. A linked relay waits for this decision too.
- **Hold:** pressing for **strictly more than 1000 ms** emits one `hold` while the button is still pressed. Release produces no additional `single`. Exactly 1000 ms is a short press.
- **Short press followed by a hold:** the first `single` and the second `hold` are emitted when the hold is recognized; this is not a double click.

Only `single` toggles a linked relay. `double` and `hold` are logical actions. All actions are emitted whether the button is linked or detached. The buttons are independent and support simultaneous operation, 10 ms debounce, and suppression of buttons already held at startup until their first release.

Local linked-button control works without a coordinator. Offline actions are discarded. Online actions use APS acknowledgements, but the application does not queue/retry an action if the stack rejects its initial submission.

For Home Assistant, use [MQTT device triggers](https://www.zigbee2mqtt.io/guide/usage/integrations/home_assistant.html#via-mqtt-device-trigger-recommended); generate each action once to discover its trigger. Experimental event entities may split an action into `event_type` and a separate `button` attribute, depending on the Zigbee2MQTT version.

### Persistence and flash writes

The application restores saved attributes before the SDK applies standard Zigbee `StartUpOnOff` separately to each endpoint.

- `off` / `on`: ordinary relay switching stays in RAM and does not create application-NV writes. The relay starts off/on after power returns.
- `previous`: actual state changes are saved; the last saved state is restored after power returns.
- `toggle`: actual state changes are saved; the relay starts in the opposite state after power returns.
- Changing to `previous` / `toggle` immediately saves the current state together with the new policy.
- Changed button settings and startup policies are persisted. Repeating an unchanged value does not create another write.

Both channels and their settings share a versioned APP NV record. The SDK appends records and rotates flash sectors when full. Failed writes are retried after more than five seconds; power loss before a successful write can lose the latest change. First boot with no valid record starts with both relays off, linked buttons, double click disabled, and `previous` startup behavior.

## Service button and status LED

Hold the service button for **10 seconds** to switch both relays off, cancel timers/Identify, blink three times, clear network/application settings, and restart. Basic factory reset is also available on both endpoints.

The active-low LED shows successful joining, Identify, reset/leave, and OTA copying in the bootloader. It is normally off and does not mirror either relay. A new Identify request replaces the previous one because the device has one LED.

## OTA updates

First install the compatible bootloader over SWS. Then use application-only Zigbee OTA updates:

1. Increase `APP_BUILD` in [version_cfg.h](version_cfg.h) before releasing a new firmware version. Update `ZCL_BASIC_SW_BUILD_ID` in [zcl_clusters.c](zcl_clusters.c) too; its first byte is the string length.
2. Build with `make -j4 ota` (plus the toolchain/SDK options if necessary).
3. Upload **`build/boot1/relay_2ch.zigbee`** as a custom firmware file in Zigbee2MQTT.

Current identity: manufacturer code `0x1141`, image type `0x03D3`, file version **`0x11044001`** (application 1.1 build 4, stack 4.0 build 1), SW build ID `2026100404`.

Check the actual OTA header before uploading:

```sh
python3 - <<'PY'
import struct
from pathlib import Path
image = Path('build/boot1/relay_2ch.zigbee').read_bytes()
manufacturer, image_type, version = struct.unpack_from('<HHI', image, 10)
print(f'Manufacturer: 0x{manufacturer:04X}')
print(f'Image type: 0x{image_type:04X}')
print(f'Version: 0x{version:08X}')
PY
```

The package version must be newer than the installed version. This firmware is not in Zigbee2MQTT's public OTA index, so a normal update check does not locate it automatically. Supply the custom file. If using MQTT, publish to `zigbee2mqtt/bridge/request/device/ota_update/update`:

```json
{"id":"DEVICE_IEEE_OR_FRIENDLY_NAME","url":"/app/data/ota/relay_2ch.zigbee"}
```

The path must be accessible to the Zigbee2MQTT process; for Docker it refers to a path inside the container. See [custom OTA sources](https://www.zigbee2mqtt.io/guide/usage/ota_updates.html#using-custom-firmware-files-or-index).

The TLSR8258 BIN header stores its version at **offset `0x02`**, after a two-byte startup jump. If an old package reports `0x00001104`, rebuild it with the current packaging script. Never upload the combined factory BIN or raw BIN as a Zigbee OTA file.

Application OTA does not update the bootloader. Settings survive a normal OTA update; both relays are held off while the bootloader applies the image, then the startup policies take effect.

## Where to change things

| File | Settings/code |
|---|---|
| [relay_pins.h](relay_pins.h) | GPIO assignments, polarities, floating ADC pin |
| [button_engine.h](button_engine.h) | Debounce, double-click window, hold and service-reset thresholds |
| [relay_2ch.c](relay_2ch.c) | Default linked/double-click settings, startup defaults, persistence, button-event sending |
| [version_cfg.h](version_cfg.h) | Application/stack version, MCU/flash target, OTA identity |
| [zcl_clusters.c](zcl_clusters.c) | Model/manufacturer strings, SW build ID, endpoint descriptors and ZCL attributes |
| [app_cfg.h](app_cfg.h), [stack_cfg.h](stack_cfg.h) | Enabled features and Zigbee table capacities |
| [zigbee2mqtt/relay_2ch.mjs](zigbee2mqtt/relay_2ch.mjs) | Fingerprint, exposed settings/actions, endpoint mapping, custom-cluster definition |
| [Makefile](Makefile) | SDK/toolchain paths, build targets and image packaging |

Changes to firmware defaults apply when no valid saved settings exist. Existing runtime settings are restored from flash; change them through Zigbee or factory-reset the device to adopt new defaults. If changing model/manufacturer, update both firmware strings and the converter fingerprint. Update the version assertions in the tests when releasing a new version.

## Validation

```sh
make check
```

The checks cover button timing boundaries, concurrent channels, GPIO polarity handling, NV persistence/retry, flash-write suppression in fixed startup modes, reset/Identify/Scenes, timed On/Off, join/rejoin, malformed OTA bounds, successful bootloader copying, BIN/OTA packaging, and converter behavior. C tests run with ASan/UBSan; ZCL validation is exercised with 100,000 random packets. The production source set is syntax-checked against real SDK headers: 179 application/SDK C files and 69 bootloader/SDK C files.

Optional converter integration checks with real herdsman libraries:

```sh
npm install --prefix /tmp/relay-2ch-node-check --cache /tmp/relay-2ch-npm-cache \
    --ignore-scripts --no-audit --no-fund zigbee-herdsman-converters@26.108.1
RELAY_ZHC_DIR=/tmp/relay-2ch-node-check make check
```

Host checks do not compile/link TC32 firmware or verify physical GPIO/radio operation. Verify relay isolation, all button modes, persistence across power loss, reset/rejoin, and OTA on the target board after building and programming.

## Licensing

Telink-derived files retain their original copyright and Apache-2.0 notices. The SDK is an external dependency with its own licensing terms.
