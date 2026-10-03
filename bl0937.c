#include "app_cfg.h"

#if (__PROJECT_TL_SMART_PLUG__) && (SMART_PLUG_METER_TYPE == SMART_PLUG_METER_BL0937)
#include "tl_common.h"
#include "smart_plug.h"

#define TICKS_PER_US CLOCK_16M_SYS_TIMER_CLK_1US
#define TICKS_PER_SECOND (TICKS_PER_US * 1000000UL)

/* One pin per IRQ group: the SDK dispatches only one callback per group. */
static volatile u32 cf_total, cf_last, cf_period;
static volatile bool cf_seen, cf_fault, cf_pending;
static volatile u32 cf1_count, cf1_first, cf1_last;
static volatile bool acquiring;
static bool initialized, voltage_phase, voltage_valid, current_valid;
static u32 phase_started, sample_started, voltage_time, current_time;
static u32 voltage_mv, current_ma, energy_baseline, energy_remainder;

static _attribute_ram_code_ void cf_irq(void)
{
    u32 now = clock_time();
    u32 period = now - cf_last;
    if (cf_seen && period < BL0937_MIN_EDGE_US * TICKS_PER_US) return;
    if (cf_seen) {
        cf_fault = period <= BL0937_FAULT_PERIOD_US * TICKS_PER_US;
        cf_period = cf_fault ? 0 : period;
        if (!cf_fault) {
            if (cf_pending) cf_total++;
            cf_total++;
        }
        cf_pending = FALSE;
    } else {
        /* A first edge alone cannot distinguish energy from the diagnostic
         * pulse train. Confirm it after the fault-period guard has elapsed. */
        cf_pending = TRUE;
    }
    cf_last = now;
    cf_seen = TRUE;
}

static _attribute_ram_code_ void cf1_irq(void)
{
    if (!acquiring) return;
    u32 now = clock_time();
    if (cf1_count && now - cf1_last < BL0937_MIN_EDGE_US * TICKS_PER_US) return;
    if (!cf1_count) cf1_first = now;
    cf1_last = now;
    cf1_count++;
}

void bl0937_reset_energy_baseline(void)
{
    u8 irq = irq_disable();
    energy_baseline = cf_total;
    cf_pending = FALSE;
    irq_restore(irq);
    energy_remainder = 0;
}

void bl0937_init(void)
{
    /* Also make explicit reinitialization safe: no measurements from the
     * previous session may survive, and no IRQ may race these resets. */
    drv_gpio_irq_risc0_dis(BL0937_CF_PIN);
    drv_gpio_irq_risc1_dis(BL0937_CF1_PIN);
    u8 irq = irq_disable();
    initialized = acquiring = voltage_valid = current_valid = FALSE;
    cf_seen = cf_fault = cf_pending = FALSE;
    cf_total = cf_last = cf_period = 0;
    cf1_count = cf1_first = cf1_last = 0;
    voltage_mv = current_ma = voltage_time = current_time = 0;
    reg_irq_src = FLD_IRQ_GPIO_RISC0_EN | FLD_IRQ_GPIO_RISC1_EN;
    irq_restore(irq);
    drv_gpio_func_set(BL0937_CF_PIN);
    drv_gpio_func_set(BL0937_CF1_PIN);
    drv_gpio_input_en(BL0937_CF_PIN, TRUE);
    drv_gpio_input_en(BL0937_CF1_PIN, TRUE);
    drv_gpio_output_en(BL0937_CF_PIN, FALSE);
    drv_gpio_output_en(BL0937_CF1_PIN, FALSE);
    drv_gpio_up_down_resistor(BL0937_CF_PIN, PM_PIN_UP_DOWN_FLOAT);
    drv_gpio_up_down_resistor(BL0937_CF1_PIN, PM_PIN_UP_DOWN_FLOAT);
    drv_gpio_func_set(BL0937_SEL_PIN);
    voltage_phase = TRUE;
    drv_gpio_write(BL0937_SEL_PIN, voltage_phase);
    drv_gpio_input_en(BL0937_SEL_PIN, FALSE);
    drv_gpio_output_en(BL0937_SEL_PIN, TRUE);
    int cf_result = drv_gpio_irq_config(GPIO_IRQ_RISC0_MODE, BL0937_CF_PIN,
                                       GPIO_FALLING_EDGE, cf_irq);
    int cf1_result = drv_gpio_irq_config(GPIO_IRQ_RISC1_MODE, BL0937_CF1_PIN,
                                        GPIO_FALLING_EDGE, cf1_irq);
    initialized = cf_result == 0 && cf1_result == 0;
    phase_started = sample_started = clock_time();
    bl0937_reset_energy_baseline();
    /* gpio_set_interrupt_risc* configures polarity/global masks only on 8258.
     * The per-pin masks require the separate enable calls. */
    if (initialized) {
        drv_gpio_irq_risc0_en(BL0937_CF_PIN);
        drv_gpio_irq_risc1_en(BL0937_CF1_PIN);
    }
}

bool bl0937_is_online(void)
{
    return initialized && voltage_valid && current_valid && !cf_fault &&
           !clock_time_exceed(voltage_time, BL0937_STALE_US) &&
           !clock_time_exceed(current_time, BL0937_STALE_US);
}

void bl0937_task(void)
{
    if (!initialized) return;
    u8 irq = irq_disable();
    /* Read time after masking IRQs: an edge newer than a prior timestamp
     * would otherwise look like a full timer rollover in subtraction. */
    u32 now = clock_time();
    if (cf_pending && now - cf_last > BL0937_FAULT_PERIOD_US * TICKS_PER_US) {
        cf_total++;
        cf_pending = FALSE;
    }
    u32 total = cf_total;
    if (cf_seen && now - cf_last > BL0937_POWER_TIMEOUT_US * TICKS_PER_US) {
        cf_seen = FALSE;
        cf_period = 0;
        cf_fault = FALSE;
    }
    u32 power_period = cf_period;
    irq_restore(irq);

    /* CF is independent of SEL and of whether V/I acquisition has completed.
     * Unsigned subtraction also handles the software pulse-counter rollover. */
    u32 pulses = total - energy_baseline;
    energy_baseline = total;
    u64 energy = (u64)pulses * BL0937_POWER_UW_PER_HZ + energy_remainder;
    energy_remainder = (u32)(energy % 3600UL);
    smartPlug_addEnergy(energy / 3600UL);

    if (!acquiring && clock_time_exceed(phase_started, BL0937_SETTLE_US)) {
        irq = irq_disable();
        cf1_count = 0;
        /* Drop a pending edge from the settling interval. Group is exclusive. */
        reg_irq_src = FLD_IRQ_GPIO_RISC1_EN;
        acquiring = TRUE;
        irq_restore(irq);
        phase_started = now;
    } else if (acquiring && clock_time_exceed(phase_started, BL0937_ACQUIRE_US)) {
        irq = irq_disable();
        acquiring = FALSE;
        u32 count = cf1_count, span = cf1_last - cf1_first;
        irq_restore(irq);
        /* Period averaging avoids the +/- one pulse gate error. For a single
         * pulse use the whole gate; no current pulses means zero load. */
        u32 coefficient = voltage_phase ? BL0937_VOLTAGE_UV_PER_HZ : BL0937_CURRENT_UA_PER_HZ;
        u32 elapsed = now - phase_started;
        u64 value = count > 1 && span ?
            (u64)coefficient * (count - 1) * TICKS_PER_SECOND / ((u64)span * 1000UL) :
            (u64)coefficient * count * TICKS_PER_SECOND / ((u64)elapsed * 1000UL);
        if (voltage_phase) {
            voltage_valid = count >= 2 && value > 0 && value <= 1000000UL;
            voltage_mv = voltage_valid ? (u32)value : 0;
            voltage_time = now;
        } else {
            current_valid = value <= 1000000UL;
            current_ma = current_valid ? (u32)value : 0;
            current_time = now;
        }
        voltage_phase = !voltage_phase;
        drv_gpio_write(BL0937_SEL_PIN, voltage_phase);
        phase_started = now;
    }

    if (clock_time_exceed(sample_started, BL0937_SAMPLE_US)) {
        sample_started = now;
        if (!bl0937_is_online()) return;
        energy_meter_sample_t sample = {0};
        sample.voltageMv = voltage_mv;
        sample.currentMa = current_ma;
        sample.activePowerMw = power_period ?
            (u32)((u64)BL0937_POWER_UW_PER_HZ * TICKS_PER_SECOND / ((u64)power_period * 1000UL)) : 0;
        sample.apparentPowerMva = (u32)((u64)voltage_mv * current_ma / 1000UL);
        u64 factor = sample.apparentPowerMva ? (u64)sample.activePowerMw * 100 / sample.apparentPowerMva : 0;
        sample.powerFactor = factor > 100 ? 100 : (u8)factor;
        smartPlug_meterSample(&sample);
    }
}
#endif
