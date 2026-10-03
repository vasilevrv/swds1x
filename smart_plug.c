#include "app_cfg.h"

#if (__PROJECT_TL_SMART_PLUG__)

#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "plug_control.h"
#include "smart_plug.h"

#define NV_ITEM_SMART_PLUG_STATE       0x70
#define SMART_PLUG_NV_MAGIC            0x504C5547UL
#define SMART_PLUG_NV_SAVE_INTERVAL_MS 300000UL
#define SMART_PLUG_ENERGY_MAX_MICRO_WH (0xFFFFFFFFFFFEULL * 1000ULL)

typedef struct _attribute_packed_ {
    u32 magic;
    u32 telemetryIntervalMs;
    u64 energyMicroWh;
} smartPlug_nvState_t;

typedef struct {
    u64 voltageSumMv;
    u64 currentSumMa;
    u64 activePowerSumMw;
    u64 apparentPowerSumMva;
    u32 samples;
} smartPlug_aggregate_t;

smartPlug_electricalAttrs_t g_smartPlugElectricalAttrs = {
    .measurementType = 0x0000000D, /* Active, apparent, single phase / phase A. */
    .rmsVoltage = 0xFFFF,
    .rmsCurrent = 0xFFFF,
    .activePower = (s16)0x8000,
    .apparentPower = 0xFFFF,
    .powerFactor = (s8)0x80,
    .voltageMultiplier = 1,
    .voltageDivisor = 10,
    .currentMultiplier = 1,
    .currentDivisor = 1000,
    .powerMultiplier = 1,
    .powerDivisor = 1,
};

smartPlug_meteringAttrs_t g_smartPlugMeteringAttrs = {
    .currentSummationDelivered = {0},
    .unitOfMeasure = 0x00,
    .multiplier = {1, 0, 0},
    .divisor = {0x40, 0x42, 0x0F}, /* 1,000,000: raw mWh to kWh */
    .summationFormatting = 0x23,
    .demandFormatting = 0x23,
    .status = BIT(0), /* Check meter until valid measurements are available. */
    .meteringDeviceType = 0x00,
    .instantaneousDemand = {0x00, 0x00, 0x80},
};

u32 g_smartPlugTelemetryIntervalMs = SMART_PLUG_TELEMETRY_INTERVAL_DEFAULT;

static smartPlug_aggregate_t aggregate;
static u64 lifetimeEnergyMicroWh;
static u32 telemetryElapsedMs;
static u32 nvElapsedMs;
static u32 tickStarted;
static u32 appliedTelemetryIntervalMs;
static bool nvDirty;
static bool nvRetryPending;
static u32 nvLastAttemptTime;

static void writeLe48(u8 dst[6], u64 value)
{
    for (u8 i = 0; i < 6; i++) {
        dst[i] = (u8)(value >> (i * 8));
    }
}

static void writeLeS24(u8 dst[3], s32 value)
{
    dst[0] = (u8)value;
    dst[1] = (u8)(value >> 8);
    dst[2] = (u8)(value >> 16);
}

static void invalidateMeasurements(void)
{
    g_smartPlugElectricalAttrs.rmsVoltage = 0xFFFF;
    g_smartPlugElectricalAttrs.rmsCurrent = 0xFFFF;
    g_smartPlugElectricalAttrs.activePower = (s16)0x8000;
    g_smartPlugElectricalAttrs.apparentPower = 0xFFFF;
    g_smartPlugElectricalAttrs.powerFactor = (s8)0x80;
    writeLeS24(g_smartPlugMeteringAttrs.instantaneousDemand, -8388608L);
}

static void saveState(void)
{
#if NV_ENABLE
    if (nvRetryPending && !clock_time_exceed(nvLastAttemptTime, 5000000UL)) {
        return;
    }
    nvLastAttemptTime = clock_time();
    smartPlug_nvState_t state;
    state.magic = SMART_PLUG_NV_MAGIC;
    state.telemetryIntervalMs = g_smartPlugTelemetryIntervalMs;
    state.energyMicroWh = lifetimeEnergyMicroWh;
    if (nv_flashWriteNew(1, NV_MODULE_APP, NV_ITEM_SMART_PLUG_STATE,
                         sizeof(state), (u8 *)&state) == NV_SUCC) {
        nvDirty = FALSE;
        nvRetryPending = FALSE;
        nvElapsedMs = 0;
    } else {
        nvRetryPending = TRUE;
        /* Avoid hammering a failing flash on every iteration of the main loop. */
        nvElapsedMs = SMART_PLUG_NV_SAVE_INTERVAL_MS - 5000UL;
    }
#endif
}

static void loadState(void)
{
#if NV_ENABLE
    smartPlug_nvState_t state;
    if ((nv_flashReadNew(1, NV_MODULE_APP, NV_ITEM_SMART_PLUG_STATE,
                         sizeof(state), (u8 *)&state) == NV_SUCC) &&
        (state.magic == SMART_PLUG_NV_MAGIC) &&
        (state.energyMicroWh <= SMART_PLUG_ENERGY_MAX_MICRO_WH)) {
        lifetimeEnergyMicroWh = state.energyMicroWh;
        if ((state.telemetryIntervalMs >= SMART_PLUG_TELEMETRY_INTERVAL_MIN) &&
            (state.telemetryIntervalMs <= SMART_PLUG_TELEMETRY_INTERVAL_MAX)) {
            g_smartPlugTelemetryIntervalMs = state.telemetryIntervalMs;
        }
    }
#endif
    writeLe48(g_smartPlugMeteringAttrs.currentSummationDelivered,
              lifetimeEnergyMicroWh / 1000ULL);
}

static void sendReports(void)
{
    if (!zb_isDeviceJoinedNwk()) {
        return;
    }

    epInfo_t dst;
    TL_SETSTRUCTCONTENT(dst, 0);
    dst.dstAddrMode = APS_SHORT_DSTADDR_WITHEP;
    dst.dstAddr.shortAddr = 0x0000;
    dst.dstEp = SMART_PLUG_ENDPOINT;
    dst.profileId = HA_PROFILE_ID;

    struct {
        u8 numAttr;
        zclReport_t attr[5];
    } electricalReport;
    electricalReport.numAttr = 5;
    electricalReport.attr[0] = (zclReport_t){ZCL_ATTRID_RMS_VOLTAGE, ZCL_DATA_TYPE_UINT16, (u8 *)&g_smartPlugElectricalAttrs.rmsVoltage};
    electricalReport.attr[1] = (zclReport_t){ZCL_ATTRID_RMS_CURRENT, ZCL_DATA_TYPE_UINT16, (u8 *)&g_smartPlugElectricalAttrs.rmsCurrent};
    electricalReport.attr[2] = (zclReport_t){ZCL_ATTRID_ACTIVE_POWER, ZCL_DATA_TYPE_INT16, (u8 *)&g_smartPlugElectricalAttrs.activePower};
    electricalReport.attr[3] = (zclReport_t){ZCL_ATTRID_APPARENT_POWER, ZCL_DATA_TYPE_UINT16, (u8 *)&g_smartPlugElectricalAttrs.apparentPower};
    electricalReport.attr[4] = (zclReport_t){ZCL_ATTRID_POWER_FACTOR, ZCL_DATA_TYPE_INT8, (u8 *)&g_smartPlugElectricalAttrs.powerFactor};
    zcl_sendReportAttrsCmd(SMART_PLUG_ENDPOINT, &dst, TRUE, ZCL_FRAME_SERVER_CLIENT_DIR,
                           ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT, (zclReportCmd_t *)&electricalReport);

    struct {
        u8 numAttr;
        zclReport_t attr[2];
    } meteringReport;
    meteringReport.numAttr = 2;
    meteringReport.attr[0] = (zclReport_t){ZCL_ATTRID_CURRENT_SUMMATION_DELIVERD, ZCL_DATA_TYPE_UINT48,
                                          g_smartPlugMeteringAttrs.currentSummationDelivered};
    meteringReport.attr[1] = (zclReport_t){ZCL_ATTRID_INSTANTANEOUS_DEMAND, ZCL_DATA_TYPE_INT24,
                                          g_smartPlugMeteringAttrs.instantaneousDemand};
    zcl_sendReportAttrsCmd(SMART_PLUG_ENDPOINT, &dst, TRUE, ZCL_FRAME_SERVER_CLIENT_DIR,
                           ZCL_CLUSTER_SE_METERING, (zclReportCmd_t *)&meteringReport);
}

static void finishAggregationWindow(void)
{
    g_smartPlugMeteringAttrs.status = energy_meter_is_online() ? 0 : BIT(0);
    if (!energy_meter_is_online() || !aggregate.samples) {
        invalidateMeasurements();
    } else if (aggregate.samples) {
        u32 voltageMv = (u32)(aggregate.voltageSumMv / aggregate.samples);
        u32 currentMa = (u32)(aggregate.currentSumMa / aggregate.samples);
        u32 activePowerMw = (u32)(aggregate.activePowerSumMw / aggregate.samples);
        u32 apparentPowerMva = (u32)(aggregate.apparentPowerSumMva / aggregate.samples);

        g_smartPlugElectricalAttrs.rmsVoltage = (u16)min2((voltageMv + 50) / 100, 0xFFFE);
        g_smartPlugElectricalAttrs.rmsCurrent = currentMa <= 0xFFFE ? (u16)currentMa : 0xFFFF;
        g_smartPlugElectricalAttrs.activePower = (s16)min2((activePowerMw + 500) / 1000, 0x7FFF);
        u32 apparentPowerVa = (apparentPowerMva + 500) / 1000;
        g_smartPlugElectricalAttrs.apparentPower = apparentPowerVa <= 0xFFFE ? (u16)apparentPowerVa : 0xFFFF;
        g_smartPlugElectricalAttrs.powerFactor = apparentPowerMva
            ? (s8)min2(((u64)activePowerMw * 100ULL) / apparentPowerMva, 100)
            : 0;
        writeLeS24(g_smartPlugMeteringAttrs.instantaneousDemand,
                   activePowerMw <= 0x7FFFFFUL ? (s32)activePowerMw : -8388608L);
    }

    memset((u8 *)&aggregate, 0, sizeof(aggregate));
    sendReports();
}

void smartPlug_init(void)
{
    memset((u8 *)&aggregate, 0, sizeof(aggregate));
    lifetimeEnergyMicroWh = 0;
    telemetryElapsedMs = 0;
    nvElapsedMs = 0;
    nvDirty = FALSE;
    nvRetryPending = FALSE;
    invalidateMeasurements();
    g_smartPlugMeteringAttrs.status = BIT(0);
    g_smartPlugTelemetryIntervalMs = SMART_PLUG_TELEMETRY_INTERVAL_DEFAULT;
    loadState();
    appliedTelemetryIntervalMs = g_smartPlugTelemetryIntervalMs;
    tickStarted = clock_time();
}

void smartPlug_meterSample(const energy_meter_sample_t *sample)
{
    aggregate.voltageSumMv += sample->voltageMv;
    aggregate.currentSumMa += sample->currentMa;
    aggregate.activePowerSumMw += sample->activePowerMw;
    aggregate.apparentPowerSumMva += sample->apparentPowerMva;
    aggregate.samples++;

    smartPlug_addEnergy(sample->energyDeltaMicroWh);
}

void smartPlug_addEnergy(u64 energyMicroWh)
{
    if (energyMicroWh) {
        lifetimeEnergyMicroWh += min2(energyMicroWh,
            SMART_PLUG_ENERGY_MAX_MICRO_WH - lifetimeEnergyMicroWh);
        writeLe48(g_smartPlugMeteringAttrs.currentSummationDelivered,
                  lifetimeEnergyMicroWh / 1000ULL);
        nvDirty = TRUE;
    }
}

void smartPlug_task(void)
{
    g_smartPlugMeteringAttrs.status = energy_meter_is_online() ? 0 : BIT(0);
    if (!energy_meter_is_online()) invalidateMeasurements();
    u32 elapsedMs = (u32)(clock_time() - tickStarted) /
                    (CLOCK_16M_SYS_TIMER_CLK_1US * 1000UL);
    if (elapsedMs) {
        tickStarted += elapsedMs * CLOCK_16M_SYS_TIMER_CLK_1US * 1000UL;
        telemetryElapsedMs += elapsedMs;
        nvElapsedMs = min2(nvElapsedMs + elapsedMs, SMART_PLUG_NV_SAVE_INTERVAL_MS);

        if (telemetryElapsedMs >= g_smartPlugTelemetryIntervalMs) {
            telemetryElapsedMs %= g_smartPlugTelemetryIntervalMs;
            finishAggregationWindow();
        }
        if (nvDirty && (nvElapsedMs >= SMART_PLUG_NV_SAVE_INTERVAL_MS)) {
            saveState();
        }
    }
}

void smartPlug_onRelayStateChanged(bool on)
{
    statusLed_blinkStart(1, 150, 1);
    if (!on && nvDirty) {
        saveState();
    }
}

void smartPlug_telemetryIntervalChanged(void)
{
    if (g_smartPlugTelemetryIntervalMs < SMART_PLUG_TELEMETRY_INTERVAL_MIN) {
        g_smartPlugTelemetryIntervalMs = SMART_PLUG_TELEMETRY_INTERVAL_MIN;
    } else if (g_smartPlugTelemetryIntervalMs > SMART_PLUG_TELEMETRY_INTERVAL_MAX) {
        g_smartPlugTelemetryIntervalMs = SMART_PLUG_TELEMETRY_INTERVAL_MAX;
    }
    if (appliedTelemetryIntervalMs == g_smartPlugTelemetryIntervalMs) {
        return;
    }
    appliedTelemetryIntervalMs = g_smartPlugTelemetryIntervalMs;
    telemetryElapsedMs = 0;
    tickStarted = clock_time();
    memset((u8 *)&aggregate, 0, sizeof(aggregate));
    nvDirty = TRUE;
    saveState();
}

void smartPlug_factoryReset(void)
{
    energy_meter_reset_baseline();
    memset((u8 *)&aggregate, 0, sizeof(aggregate));
    telemetryElapsedMs = 0;
    lifetimeEnergyMicroWh = 0;
    g_smartPlugTelemetryIntervalMs = SMART_PLUG_TELEMETRY_INTERVAL_DEFAULT;
    appliedTelemetryIntervalMs = g_smartPlugTelemetryIntervalMs;
    writeLe48(g_smartPlugMeteringAttrs.currentSummationDelivered, 0);
    nvDirty = TRUE;
    nvRetryPending = FALSE; /* A reset must attempt to persist zero immediately. */
    saveState();
}

#endif
