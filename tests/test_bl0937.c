#define SMART_PLUG_METER_TYPE 1
#include "host_sdk.h"
#define _attribute_ram_code_
#define BL0937_CF_PIN 3
#define BL0937_CF1_PIN 2
#define BL0937_SEL_PIN 4
#define PM_PIN_UP_DOWN_FLOAT 0
#define GPIO_IRQ_RISC0_MODE 2
#define GPIO_IRQ_RISC1_MODE 3
#define GPIO_FALLING_EDGE 1
#define FLD_IRQ_GPIO_RISC0_EN (1U << 21)
#define FLD_IRQ_GPIO_RISC1_EN (1U << 22)
static u32 reg_irq_src;
static bool cf_enabled, cf1_enabled, sel, fail_config;
static void (*cf_callback)(void), (*cf1_callback)(void);
static void drv_gpio_func_set(u32 pin) { (void)pin; }
static void drv_gpio_input_en(u32 pin, bool on) { (void)pin; (void)on; }
static void drv_gpio_output_en(u32 pin, bool on) { (void)pin; (void)on; }
static void drv_gpio_up_down_resistor(u32 pin, u8 pull) { (void)pin; (void)pull; }
static void drv_gpio_write(u32 pin, bool on) { assert(pin == BL0937_SEL_PIN); sel = on; }
static void drv_gpio_irq_risc0_dis(u32 pin) { assert(pin == BL0937_CF_PIN); cf_enabled = false; }
static void drv_gpio_irq_risc1_dis(u32 pin) { assert(pin == BL0937_CF1_PIN); cf1_enabled = false; }
static void drv_gpio_irq_risc0_en(u32 pin) { assert(pin == BL0937_CF_PIN); cf_enabled = true; }
static void drv_gpio_irq_risc1_en(u32 pin) { assert(pin == BL0937_CF1_PIN); cf1_enabled = true; }
/* Match SDK behavior: configuring a callback does NOT enable a pin. */
static int drv_gpio_irq_config(int mode, u32 pin, int edge, void (*callback)(void)) {
    assert(edge == GPIO_FALLING_EDGE);
    if (fail_config) return -1;
    if (mode == GPIO_IRQ_RISC0_MODE) { assert(pin == BL0937_CF_PIN); cf_callback = callback; }
    else { assert(mode == GPIO_IRQ_RISC1_MODE && pin == BL0937_CF1_PIN); cf1_callback = callback; }
    return 0;
}
#include "bl0937_sources.inc"

static u64 simulated_us;
static void cf_edge(u32 us) {
    now_ticks += us * 16U;
    if (cf_enabled) cf_callback();
}
static void fresh(void) {
    simulated_us = now_ticks = reports = nv_writes = 0;
    flash_present = flash_fail = fail_config = false;
    smartPlug_init(); bl0937_init();
    assert(sel && cf_enabled && cf1_enabled && !bl0937_is_online());
}
static void run_us(u32 duration, bool cf1_signal) {
    assert(duration % 100 == 0);
    for (u32 us = 0; us < duration; us += 100) {
        simulated_us += 100;
        now_ticks += 1600;
        if (cf_enabled && simulated_us % 10000 == 0) cf_callback();
        if (cf1_enabled && cf1_signal && simulated_us % (sel ? 700 : 10000) == 0) cf1_callback();
        bl0937_task(); smartPlug_task();
    }
}
static u64 pulse_energy(u32 count) { return (u64)count * BL0937_POWER_UW_PER_HZ / 3600; }

int main(void) {
    fresh();
    run_us(990000, true); assert(!acquiring && !voltage_valid);
    run_us(2110000, true); assert(voltage_valid && !current_valid && !sel);
    run_us(3100000, true); assert(bl0937_is_online());
    assert(voltage_mv == BL0937_VOLTAGE_UV_PER_HZ * 1000ULL / 700);
    assert(current_ma == BL0937_CURRENT_UA_PER_HZ / 10);
    assert(lifetimeEnergyMicroWh == pulse_energy(620));
    finishAggregationWindow();
    assert(g_smartPlugElectricalAttrs.activePower == 172);
    assert(g_smartPlugElectricalAttrs.rmsVoltage == (voltage_mv + 50) / 100);
    assert(g_smartPlugElectricalAttrs.rmsCurrent == BL0937_CURRENT_UA_PER_HZ / 10);

    /* Minimum reporting interval cannot outrun the driver's sample cadence. */
    g_smartPlugTelemetryIntervalMs = 100; smartPlug_telemetryIntervalChanged();
    for (u32 i = 0; i < 1000; i++) {
        run_us(100000, true);
        assert(g_smartPlugElectricalAttrs.rmsVoltage != 0xffff);
    }
    assert(lifetimeEnergyMicroWh == pulse_energy(10620));
    u64 before = lifetimeEnergyMicroWh;
    smartPlug_onRelayStateChanged(false); assert(flash_present);
    smartPlug_init(); bl0937_init();
    assert(lifetimeEnergyMicroWh == before && !bl0937_is_online());
    cf_edge(10000); bl0937_task();
    now_ticks += (BL0937_FAULT_PERIOD_US + 1) * 16U; bl0937_task();
    assert(lifetimeEnergyMicroWh == before + pulse_energy(1));
    smartPlug_factoryReset(); assert(lifetimeEnergyMicroWh == 0 && energy_remainder == 0);
    cf_edge(10000); bl0937_task(); assert(lifetimeEnergyMicroWh == pulse_energy(1));

    fresh();
    for (u32 i = 0; i < 10000; i++) { cf_edge(10000); bl0937_task(); }
    assert(lifetimeEnergyMicroWh == pulse_energy(10000));
    /* Counter and hardware timer rollover. */
    cf_total = energy_baseline = 0xfffffffeU;
    cf_last = now_ticks = 0xffff0000U; cf_seen = true;
    before = lifetimeEnergyMicroWh;
    cf_edge(10000); cf_edge(10000); bl0937_task();
    assert(cf_total == 0 && cf_period == 160000 && lifetimeEnergyMicroWh == pulse_energy(10002));
    before = lifetimeEnergyMicroWh;
    cf_edge(1); bl0937_task(); assert(lifetimeEnergyMicroWh == before); /* glitch */
    cf_edge(147); bl0937_task(); assert(cf_fault && lifetimeEnergyMicroWh == before);
    cf_edge(148); bl0937_task(); assert(cf_fault && lifetimeEnergyMicroWh == before);
    cf_edge(10000); bl0937_task(); assert(!cf_fault && lifetimeEnergyMicroWh > before);
    now_ticks += (BL0937_POWER_TIMEOUT_US + 1) * 16U; bl0937_task();
    assert(!cf_seen && cf_period == 0);

    /* Put the timer wrap inside the voltage acquisition gate, not settling. */
    fresh(); now_ticks = 0U - 2000000U * 16U; smartPlug_init(); bl0937_init();
    run_us(6200000, true);
    assert(bl0937_is_online() && voltage_mv == BL0937_VOLTAGE_UV_PER_HZ * 1000ULL / 700);
    assert(current_ma == BL0937_CURRENT_UA_PER_HZ / 10);

    /* A diagnostic train already active at boot must contribute zero energy,
     * including its first edge; a lone low-load pulse must still be counted. */
    fresh(); cf_edge(10000); bl0937_task();
    assert(cf_pending && lifetimeEnergyMicroWh == 0);
    for (u32 i = 0; i < 100; i++) { cf_edge(148); bl0937_task(); }
    assert(cf_fault && lifetimeEnergyMicroWh == 0);
    fresh(); cf_edge(10000); bl0937_task();
    now_ticks += (BL0937_FAULT_PERIOD_US + 1) * 16U; bl0937_task();
    assert(!cf_pending && lifetimeEnergyMicroWh == pulse_energy(1));
    fresh(); cf_edge(10000); bl0937_task(); smartPlug_factoryReset();
    now_ticks += (BL0937_FAULT_PERIOD_US + 1) * 16U; bl0937_task();
    assert(lifetimeEnergyMicroWh == 0); /* pre-reset pending edge discarded */

    /* Very low load: gaps exceed the power timeout, and the run crosses a
     * complete timer rollover. Lifetime CF energy must remain exact. */
    fresh();
    for (u32 i = 0; i < 20; i++) {
        cf_edge(10000); bl0937_task();
        now_ticks += (BL0937_FAULT_PERIOD_US + 1) * 16U; bl0937_task();
        for (u32 second = 0; second < 15; second++) {
            now_ticks += 16000000U; bl0937_task();
        }
    }
    assert(lifetimeEnergyMicroWh == pulse_energy(20) && cf_total == 20);

    fresh(); run_us(6200000, true); assert(bl0937_is_online());
    run_us(10000000, false); assert(!bl0937_is_online());
    run_us(7000000, true); assert(bl0937_is_online());
    /* A current gate without pulses is a valid zero-load measurement. */
    voltage_phase = false; acquiring = true; cf1_count = 0; phase_started = now_ticks;
    now_ticks += (BL0937_ACQUIRE_US + 1) * 16U; bl0937_task();
    assert(current_valid && current_ma == 0);
    fail_config = true; bl0937_init();
    assert(!initialized && !cf_enabled && !cf1_enabled && !bl0937_is_online());
    before = lifetimeEnergyMicroWh; cf_edge(10000); bl0937_task();
    assert(lifetimeEnergyMicroWh == before);
    puts("PASS: BL0937 per-pin IRQ enable, SEL settling, V/I/power, CF energy, rounding, 100 ms reports, counter/timer wraps, glitches, fault at startup, pending reset, sparse pulses, timeout, NV restore/reset, IRQ allocation failure");
}
