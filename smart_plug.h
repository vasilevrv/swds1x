#pragma once

#include "energy_meter.h"

#define SMART_PLUG_ENDPOINT                    0x01
#define SMART_PLUG_CONFIG_CLUSTER_ID           0xFC00
#define SMART_PLUG_ATTR_TELEMETRY_INTERVAL_MS  0x0000

#define SMART_PLUG_TELEMETRY_INTERVAL_DEFAULT  10000UL
#define SMART_PLUG_TELEMETRY_INTERVAL_MIN      100UL
#define SMART_PLUG_TELEMETRY_INTERVAL_MAX      3600000UL

typedef struct {
    u32 measurementType;
    u16 rmsVoltage;
    u16 rmsCurrent;
    s16 activePower;
    u16 apparentPower;
    s8 powerFactor;
    u16 voltageMultiplier;
    u16 voltageDivisor;
    u16 currentMultiplier;
    u16 currentDivisor;
    u16 powerMultiplier;
    u16 powerDivisor;
} smartPlug_electricalAttrs_t;

typedef struct {
    u8 currentSummationDelivered[6];
    u8 unitOfMeasure;
    u8 multiplier[3];
    u8 divisor[3];
    u8 summationFormatting;
    u8 demandFormatting;
    u8 status;
    u8 meteringDeviceType;
    u8 instantaneousDemand[3];
} smartPlug_meteringAttrs_t;

extern smartPlug_electricalAttrs_t g_smartPlugElectricalAttrs;
extern smartPlug_meteringAttrs_t g_smartPlugMeteringAttrs;
extern u32 g_smartPlugTelemetryIntervalMs;

void smartPlug_init(void);
void smartPlug_task(void);
void smartPlug_meterSample(const energy_meter_sample_t *sample);
void smartPlug_addEnergy(u64 energyMicroWh);
void smartPlug_onRelayStateChanged(bool on);
void smartPlug_telemetryIntervalChanged(void);
void smartPlug_factoryReset(void);
