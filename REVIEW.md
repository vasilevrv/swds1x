# Five-second battery sampling: 2026101006 / 0x110E4001

The device owner confirmed that ADC readings exactly matched a 3.3 V bench supply. No constant Vref correction is introduced: factory values of 1174 mV and -3 mV remain in use. Agreement with the bench supply does not establish the cause of the earlier discrepancy on a battery.

The pre-Zigbee startup measurement, PB0 comparison, all five diagnostic attributes and the bootAdc converter were removed. Sampling now starts after application initialization when the LED, stack and BDB are idle, and lasts at most five seconds. Up to ten bursts run at 500 ms intervals, each after 200 ms of quiet. The first ADC conversion is discarded; the next five produce a median. SAR ADC is powered down after each burst. At least five valid medians are required; the lowest and highest are discarded and the rest averaged. Results from previous windows are not blended in.

Review findings:

- The main loop continues handling events during sampling. LED/radio activity cannot extend the five-second window.
- Waiting to start the window does not block sleep.
- Errors and incomplete DMA samples cannot produce a false zero-percent reading.
- An invalid periodic window is skipped until the next 12-hour interval. An invalid initial result is retried no sooner than one minute later on an ordinary wake.
- Reports contain exactly two standard UINT8 attributes, use binding and retry without blocking sleep.
- Time arithmetic handles counter wrap. The sum of ten voltage readings cannot overflow.
- Factory flash is never written by the battery algorithm.

Checks passed: production runtime with ASan/UBSan; supply recovery and outlier handling; ADC shutdown; busy/invalid windows; startup and 12-hour reports; retry and clock wrap. The complete make check also passed for 179 application C files and 69 bootloader C files, real herdsman/converter integration and a standard-only voltage/percentage wire report.

TC32 is unavailable: make ota stops at missing tc32-elf-gcc. No new BIN/OTA was built in this environment. Accuracy on CR2032 and power consumption require device validation.

The sections below document earlier versions. Their superseded behavior is retained as implementation history.

---

# Reading live ADC calibration: 2026101005 / 0x110D4001

Device measurements were PC5 2552 mV and PB0 2543 mV, with flash bytes `FC 00 FF FF 11 AE 00` and active Vref/offset values of 1174 mV/-3 mV. BatteryVoltage 2600 mV correctly rounds 2552 mV to 100 mV units; battery 7.5% follows the selected curve. Live calibration matched the original dump, ruling out corruption of those seven bytes. The channels differed by 9 mV, suggesting a common cause but not proving a real supply dip or ADC accuracy. At this point, a stable external supply test was proposed before applying a constant correction.

An old dump cannot prove calibration survived subsequent flashing. This version read seven live bytes through flash_read(CFG_SAR_ADC_CALIBRATION) at startup and captured the active adc_vref/adc_vref_offset after ADC configuration. It did not restore coefficients or write flash. Sector 0x77000 could be affected by a full erase or a write reaching that range, but selecting a 1 MiB target does not imply transferring 1 MiB. A 217284-byte file written from address 0 does not reach this sector. Repeated readback halves do not establish behavior of writes beyond the chip boundary.

The first report contained seven attributes, including an OCTET_STR carrying seven calibration bytes, UINT16 Vref and INT8 offset. The converter exposed adc_calibration/adc_vref/adc_offset. Later 12-hour measurements neither reread calibration nor sent diagnostics.

Tests covered OCTET_STR length, signed offsets, erased/modified live bytes without replacement, actual active coefficients rather than backup-derived values, and a binary Report Attributes frame decoded by real zigbee-herdsman and converters. The complete check passed with ASan/UBSan, 179 application C files, 69 bootloader C files, BIN/OTA packaging and Z2M. The expanded wire integration test also passed separately. TC32 was unavailable; device verification of the diagnostic build remained necessary at that stage.

---

# ADC channel diagnostics: 2026101004 / 0x110C4001

The owner confirmed a 2500 mV reading on 2026101003 after removing and reinserting the battery. Correcting DFIFO completion handling had not resolved the discrepancy.

Startup measurement was moved before stack_init with the LED off. sensor_batteryBootSample read PC5 and a PB0 comparison channel. sensor_batteryInit reused the result after reporting state restoration instead of repeating ADC measurement. Neither channel is bonded out on ET32; PB0 is also used for VDD in ZigbeeTLc/src/board_zb_mc.h. This comparison did not prove PB0 accuracy or a radio-induced supply dip.

Read-only Power Configuration UINT16 attributes 0xF000/0xF001 and Z2M properties boot_voltage_pc5/boot_voltage_pb0 were added. The startup report had four attributes; periodic reports kept the two standard ones. Invalid primary measurements left standard values unknown while diagnostics were still reported. PB0 was measured only at boot, then ADC configuration returned to PC5. Calibration and the primary channel were unchanged; the higher reading was never selected automatically. No extra wake timer was added.

Review covered initialization order, no repeated ADC sample after early initialization or retention, independent channel values, unknown primary measurements without false 0%, and report types/addressing/copying. Integration tests decoded a real binary report containing standard and unknown vendor attributes. Full checks passed: ASan/UBSan, 179 application C files, 69 bootloader C files, BIN/OTA packaging and real Z2M imports/decoding. make ota could not build without tc32-elf-gcc. Device verification required OTA, the updated converter and a battery restart; comparing channels did not replace checking ADC accuracy under load.

---

# Twelve-hour battery interval: 2026101003 / 0x110B4001

At the owner's request, the five-second startup remeasurement, hourly measurements and minute retries of invalid/busy measurements were removed. Cold/deep startup measured once and sent voltage/percentage after network recovery and binding. Retention did not repeat initialization. The next measurement and report occurred after 43,200,000 ms, even with unchanged voltage. Scheduling used app_nowMs and app_task without an extra wake timer.

Periodic sampling waited for 200 ms of quiet for at most two seconds. Busy or invalid attempts were skipped until the next half-day interval, preserving previous data. Failed report enqueue remained pending and retried after one minute on a normal wake; report pending did not block sleep. Successful enqueue was not proof of delivery, although APS ACK was enabled.

Persisted SDK Power Configuration reporting entries were disabled in RAM without NV writes; the converter configured maxInterval 0xFFFF. This prevented duplicate reports and separate SDK reporting timers, including restored older settings. Attribute reads and manual standard reports through binding remained supported. Updating the converter and running Reconfigure were required.

Review checked attribute IDs/types/direction/addressing and synchronous SDK report copying. Tests covered startup without binding or with busy BDB, one-time reports, no measurements at 5 seconds/1 hour/before 12 hours, unchanged-value reports, invalid/busy samples without rapid retry, battery replacement, bounded quiet waits, clock wrap and enqueue retries without sleep blocking. Full checks passed with ASan/UBSan, 179 application C files, 69 bootloader C files, packaging and real Z2M imports. TC32 builds, device timing/current and ADC accuracy were not validated at that stage.

---

# Battery ADC completion handling: 2026101002 / 0x110A4001

The backup `scen_000000_0100000.bin` contained two byte-identical 512 KiB halves. Calibration bytes `FC 00 FF FF 11 AE 00` at 0x770C0 decoded to SDK Vref 1174 mV and offset -3 mV. These valid coefficients did not alone explain 2600 mV versus a 3012 mV unloaded multimeter measurement. The dump did not prove the chip's current contents after later writes.

Application measurement moved to battery_adc.c with 16-byte buffer alignment, waiting for the last DMA slot, checking every slot, a 2 ms timeout and DFIFO shutdown on every return path. Factory Vref/offset and a trimmed mean of the middle four of eight samples were preserved. Negative offset results could not wrap to large unsigned values. The original SDK and its flash-safe checks were unchanged.

This historical version remeasured five seconds after startup with a 200 ms quiet window; busy or invalid attempts retried after one minute. Z2M minimum reporting interval was reduced to 60 seconds, requiring Reconfigure. These intervals were superseded by 2026101003.

Review checked the SDK conversion formula, DMA buffer lifetime, bounded waits, DFIFO shutdown, SDK ADC compatibility, success/failure/busy intervals, retention and clock wrap. ASan/UBSan tests covered progressive DMA writes, zero/negative results, outliers, missing samples, timeout and startup 2600 mV followed by 3012 mV. Full checks passed for 179 application C files and 69 bootloader C files, packaging and real Z2M imports. TC32 was missing; the changes addressed identified sampling defects but did not establish the physical cause of the discrepancy.

---

# Initial CR2032 support: 2026101001 / 0x11094001

A Power Configuration server was added, giving the endpoint four input clusters. batteryVoltage and batteryPercentageRemaining became readable/reportable. The Z2M converter added battery/voltage, binding, reporting and reads. Older devices required a new interview/configuration.

VDD used the SDK VBAT path through PC5, retaining ADC compatibility for flash-safe checks. This historical version sampled at cold startup and hourly. Five-reading median and IIR filtering preserved previous values on invalid samples and accepted large voltage changes immediately. Periodic sampling waited for 200 ms without LED/radio activity, bounded to two seconds, with a one-minute retry. Sleep was blocked during that wait so parent polls could not defer measurement indefinitely.

The project-specific nonlinear CR2032 curve estimated usable charge. Its 2.2 V zero-percent endpoint matched SDK flash safety. Percentage was computed from millivolts before voltage rounding and encoded in half-percent units.

Review covered no NV writes, retention state, unsigned clock-wrap arithmetic and unknown handling by the standard converter. Tests covered all 65536 input voltages, outliers/filter/invalid/replacement, hourly timing, quiet windows, bounded wait/retry, cold/deep initialization, sleep gating and real Z2M imports/decoding/report configuration. Full checks passed with ASan/UBSan, 178 application C files and 69 bootloader C files. TC32 was unavailable; final image size and ADC accuracy were unverified.

---

# Immediate reset and smooth LED: 2026100904 / 0x11084001

Target: TLSR8258F512ET32; image type 0x02D4.

Reset starts on a stable D3 press edge after 20 ms of debounce, regardless of pairing state. Three LED pulses confirm it; factory reset follows after 700 ms. A button held during boot cannot cause a reset loop: release and press again.

Each reed transition starts a one-second fade: 500 ms rising and 500 ms falling. PB4 supports PWM4; the active-low output uses 1 kHz PWM with duty updates every 10 ms. Another transition cancels the old timer and restarts from LED off. Both IAS events stay queued. Sleep is blocked during the fade; afterward PWM stops and PB4 returns to GPIO HIGH. Reset and Identify may cancel the fade.

Review checked duty limits of 0..1000, completion according to actual elapsed time after delayed callbacks, and LED shutdown with event preservation on timer allocation failure. Tests covered debounce/repeated presses/held boot, active-low PWM, rising/falling brightness, duration, animation restart, both IAS events, sleep gating, reset cancellation and timer exhaustion.

Full checks and git diff --check passed. No BIN was built without TC32. D3 hardware wake and PWM appearance required device validation at that point. The earlier ten-second hold and 50 ms pulse were superseded.

---

# Corrected hardware target: 2026100903 / 0x11074001

The owner identified the actual chip as having 512 KiB internal flash. The earlier 1 MiB target was incorrect: internalFlashSizeCheck stopped before user_init, explaining the absence of LED/button activity on the 512 KiB chip.

The target changed to TLSR_8258_512K with FLASH_CAP_SIZE_1M disabled. A separate SDK copy adapted the B85/512K restriction, JEDEC check and factory addresses. Application starts at 0x10000, OTA at 0x36000, each with a 0x26000-byte slot. Traditional factory sectors 0x76000/0x77000 are preserved. All eight Zigbee NV modules end before 0x75000; pre-install code moved to 0x78000 to avoid the MAC sector.

Image type changed to 0x02D4. OTA packaging rejects old 1 MiB images. Tests use a 512 KiB flash model, updated OTA boundaries, the actual SDK JEDEC check and checks against NV/factory overlap.

Full checks passed with ASan/UBSan, GPIO/input/retention, actual SDK JEDEC logic, syntax checks for 176 application C files and 69 bootloader C files, packaging and real converter imports. git diff --check passed. TC32 builds and device startup were not yet verified. This SDK originally rejected B85/512K; radio/OTA compatibility and actual factory data needed hardware validation.

The earlier 1 MiB layout and image identity below are historical and no longer apply.

---

# Initial implementation review: 2026-10-09

The review followed implementation and host checks. It covered production sources, SDK startup/PM/GPIO initialization, stack role, zigbee-herdsman IAS interview, NV layout, packaging and the external converter.

## Follow-up review

An earlier host stub modeled drv_pm_wakeupPinValid incorrectly and missed a real sleep defect. Its earlier conclusion was superseded by this review.

Corrections:

- **Sleep condition:** the SDK returns SUCCESS=0 for an active wake GPIO and FAILURE=1 for an inactive one. Logical negation inverted the intended check. Sleep now requires a result other than SUCCESS; active D3/D7 keep the device awake. The test executes the real SDK function.
- **Retention timing:** the 16 MHz hardware clock may restart. IAS/NV retries, queues and network timers now use a shared millisecond clock that includes sleep through the continuous 32 kHz clock and rebases the active tick. Tests cover hardware restart, 32 kHz wrap and avoiding double-counting.
- **Deep sleep without retention:** BDB repower uses drv_pm_deepSleep_flag_get instead of constant 1, matching the SDK contact example. Tests cover cold/deep startup and twenty retention wakes without stack/input reinitialization.
- **Incomplete enrollment after power loss:** a saved CIE without a cached short address triggers a ZDO NWK Address Request. Response length/status/CIE IEEE are validated before enrollment resumes.
- **Serialization:** runtime tests use the actual SDK IAS Zone Status Change Notification function and check wire payload and APS ACK.

Version became 0x11064001, SW build ID 2026100902, with historical image type 0x03D4. Full checks passed. Clang Static Analyzer checked nine application C files without diagnostics after excluding fixed-address dereference checks for normal SDK MMIO. TC32 remained unavailable.

## Other review corrections

- SDK gpio_init(TRUE) runs on retention wake. user_init now configures GPIO before PHY restoration, restores the LED and preserves logical state, timers, queues and IAS.
- Pending join/rejoin timer allocation retries block sleep so deep sleep cannot discard retry state.
- Reed wake polarity is opposite the last stable state. The active-level check prevents sleep over a new transition; tests change D7 between scanning and wake configuration.
- The Makefile selects END_DEVICE=1 and libzb_ed.a; PM_ENABLE=1 and ZB_MAC_RX_ON_WHEN_IDLE=0. Relay clusters are disabled and the sensor has one IAS endpoint. make check verifies these settings.
- APS ACK is enabled and enqueue success is checked. Failed sends keep the event queued. The converter binds IAS before reading initial state.
- IAS NV follows the SDK IEEE[8]/zoneId/zoneState layout. Read/write failures preserve configuration for retry; unchanged settings and reed events do not write flash.
- Historical reset behavior was one reset per hold at 10,000 ms; release at the threshold canceled it. Timer allocation failure did not prevent reset or leave the LED on. This was later replaced by immediate press reset.
- Historical sensor OTA image type was 0x03D4; packaging rejected relay/foreign types. OTA enables fast polling. The 512 KiB adaptation later changed the type to 0x02D4.
- A linker ASSERT rejects crossing the 0x848000 retention SRAM boundary.

## Checks performed

The complete check passed:

- ASan/UBSan for both-edge debounce, historical 9,999/10,000 ms thresholds, release, held boot, repeated holds and clock wrap.
- Production GPIO configuration: active-low B4, D3 pull-up, D7 pull-down, no relay outputs; historical 50 ms LED pulse, GPIO reset restoration, sleep gating and the sleep-entry race.
- Event queue ordering, failed initial sends, missing binding, offline coalescing, capacity limits and preservation of the latest state.
- IAS CIE writes, enrollment, sender IEEE validation, CIE changes, NV retry/restore/layout and suppression of redundant flash writes.
- Join/rejoin with failed timer allocation and busy commissioning.
- IAS/foundation/OTA payload bounds and 100,000 randomized packets.
- Bootloader malformed OTA sizes/CRC, erase limits, copy/readback and NV preservation.
- BIN/OTA CRC, offsets, slot bounds, identity and rejection of foreign images.
- Complete source set against real SDK headers: 176 application C files and 69 bootloader C files.
- Converter fingerprint, polarity, bind/read and real imports/exposes from zigbee-herdsman-converters 26.111.0.
- git diff --check.

## Limitations recorded at that stage

make combined stopped at Missing tc32-elf-gcc. No ELF/BIN was produced; actual linking, final size and the linker ASSERT were not verified by a target build.

GPIO, interview, parent polling, repeated wakes, sleep current, parent loss, reset/re-pairing and OTA required device checks. This historical configuration expected unconnected PC5 and 1 MiB flash; the later hardware correction superseded that flash assumption. The internal D7 pull-down draws current when the reed switch is closed, so consumption must be measured in both states.

The queue holds eight events; offline state retains only the latest position. The stack handles APS ACK, but the application does not receive final-delivery confirmation or resend a packet already accepted by the stack after its radio retries are exhausted. A network interruption can lose an individual event; recovery sends the current state.

No blocking application defects were found after these corrections. A binary firmware release still required TC32 builds and hardware validation.
