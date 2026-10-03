#pragma once

#define SMART_PLUG_METER_BL0937  1
#define SMART_PLUG_METER_HLW8032 2

/* Select the chip actually installed on the board. Also overridable via -D. */
#ifndef SMART_PLUG_METER_TYPE
#define SMART_PLUG_METER_TYPE SMART_PLUG_METER_BL0937
#endif

#if SMART_PLUG_METER_TYPE != SMART_PLUG_METER_BL0937 && \
    SMART_PLUG_METER_TYPE != SMART_PLUG_METER_HLW8032
#error "Unsupported SMART_PLUG_METER_TYPE"
#endif

/* BL0937 conversion coefficients, in micro-units per Hz.
 * Divider=(1991000+998)/998, Rshunt=0.001 ohm, Vref=1.218 V.
 * Initial estimates from ESPHome's BL0937 formulas.
 * Voltage corrected against user's meter: 157895 * 222.8 / 294.9 = 119291.
 * Current corrected by least squares through zero using reference/reported A:
 * 2.004/2.52, 1.974/2.49, 1.60/2.02: 12870 * 0.793543344 = 10213.
 * 1.82/2.15 is inconsistent with these points; recheck under stable load.
 * Power still requires its own reference measurement on the board.
 * Calibration: new coefficient = old coefficient * reference / reported.
 */
#define BL0937_VOLTAGE_UV_PER_HZ 119291UL
#define BL0937_CURRENT_UA_PER_HZ 10213UL
#define BL0937_POWER_UW_PER_HZ   1720063UL

/* SEL=0: current, SEL=1: voltage. One complete V/I cycle takes 6 seconds. */
#define BL0937_SETTLE_US         1000000UL
#define BL0937_ACQUIRE_US        2000000UL
/* Faster than the minimum 100 ms reporting window, allowing loop jitter. */
#define BL0937_SAMPLE_US         50000UL
#define BL0937_STALE_US          9000000UL
#define BL0937_POWER_TIMEOUT_US  10000000UL
#define BL0937_MIN_EDGE_US       20UL
/* BL0937 signals overcurrent by ~6.78 kHz on CF. Do not count this as energy. */
#define BL0937_FAULT_PERIOD_US   180UL
