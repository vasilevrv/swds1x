#include "host_sdk.h"
#include "meter_sources.inc"

static void advance_us(u32 us) { now_ticks += us * 16U; }
static void be24(u8 *dst,u32 value) { dst[0]=value>>16; dst[1]=value>>8; dst[2]=value; }
static void make_frame(u8 frame[24],u32 count,u8 state,u8 update) {
    memset(frame,0,24); frame[0]=state; frame[1]=0x5A;
    be24(frame+2,100000); be24(frame+5,1000);
    be24(frame+8,1000); be24(frame+11,1000);
    be24(frame+14,5000000); be24(frame+17,10000);
    frame[20]=update | ((count&0x10000) ? 0x80 : 0);
    frame[21]=count>>8; frame[22]=count;
    for (u8 i=2;i<23;i++) frame[23]+=frame[i];
}
static void feed_bytes(const u8 *data,u32 len) {
    assert(len<=60);
    hlw8032_uartRxData_t *rx=(hlw8032_uartRxData_t *)hlwUartRxBuffer;
    rx->dataLen=len; memcpy(rx->dataPayload,data,len);
    uartReceiveCallback(); hlw8032_task();
}
static void frame(u32 count,u8 state,u8 update) {
    u8 data[24]; make_frame(data,count,state,update);
    advance_us(50000); feed_bytes(data,24);
}
static void fresh(void) {
    now_ticks=0; flash_present=false; flash_fail=false; uart_error=false; reports=nv_writes=0;
    g_smartPlugTelemetryIntervalMs=10000;
    smartPlug_init(); hlw8032_init();
}
static u64 energy(void) { return lifetimeEnergyMicroWh; }
static u64 pulse_energy(u32 n) { return (u64)n*5000000*
    HLW8032_VOLTAGE_COEFFICIENT_MILLI*HLW8032_CURRENT_COEFFICIENT_MILLI/3600000000ULL; }

int main(void) {
    fresh(); frame(500,0x55,0x70);
    assert(hlw8032_isOnline() && energy()==0);
    assert(latestVoltageMv==199500 && latestCurrentMa==1000 && latestActivePowerMw==997500);
    frame(501,0x55,0x70); assert(energy()==pulse_energy(1));
    for (u32 n=502;n<=600;n++) frame(n,0x55,0x70);
    assert(energy()==pulse_energy(100)); /* fractional rounding does not accumulate */

    fresh(); frame(65535,0x55,0x70); frame(65536,0x55,0x70);
    assert(energy()==pulse_energy(1));
    frame(65537,0x55,0x70); assert(energy()==pulse_energy(2));
    fresh(); frame(131071,0x55,0x70); frame(0,0x55,0x70);
    assert(energy()==pulse_energy(1)); /* carry bit toggles back to zero */

    fresh(); frame(1000,0x55,0x70); frame(1001,0x55,0x70);
    u64 before=energy(); frame(0,0x55,0x70); assert(energy()==before);
    frame(1,0x55,0x70); assert(energy()==pulse_energy(2));
    fresh(); frame(70000,0x55,0x70); frame(0,0x55,0x70);
    assert(energy()==0); frame(1,0x55,0x70); assert(energy()==pulse_energy(1));

    fresh(); frame(40,0x55,0x70); advance_us(2100000); hlw8032_task();
    assert(!hlw8032_isOnline()); frame(0,0x55,0x70); assert(energy()==0);
    frame(1,0x55,0x70); assert(energy()==pulse_energy(1));
    frame(2,0xAA,0x70); assert(!hlw8032_isOnline());
    frame(0,0x55,0x70); assert(energy()==pulse_energy(1));

    fresh(); frame(0,0x55,0); frame(1,0x55,0);
    assert(!hlw8032_isOnline() && energy()==pulse_energy(1)); /* independent of RMS */
    frame(2,0xF8,0x30); assert(hlw8032_isOnline() && latestVoltageMv==0);

    fresh(); u8 data[24]; make_frame(data,123,0x55,0x70);
    data[23]++; feed_bytes(data,24); assert(!hlw8032_isOnline());
    make_frame(data,123,0x55,0x70); feed_bytes(data,7); feed_bytes(data+7,17);
    assert(hlw8032_isOnline() && energy()==0);
    make_frame(data,123,0x55,0x70); be24(data+5,0); data[23]=0;
    for (u8 i=2;i<23;i++) data[23]+=data[i];
    feed_bytes(data,24); assert(!hlw8032_isOnline());
    make_frame(data,123,0x55,0x70); feed_bytes(data,24); assert(hlw8032_isOnline());
    u8 noise[60]; memset(noise,0x11,sizeof(noise));
    hlw8032_uartRxData_t *rx=(hlw8032_uartRxData_t *)hlwUartRxBuffer;
    for (u8 i=0;i<3;i++) { rx->dataLen=60; memcpy(rx->dataPayload,noise,60); uartReceiveCallback(); }
    assert(streamOverflow); hlw8032_task(); assert(!streamOverflow && !frameWindowLen);
    frame(124,0x55,0x70); assert(energy()==pulse_energy(1));

    fresh(); frame(100,0x55,0x70);
    uart_error=true; frame(101,0x55,0x70);
    assert(!uart_error && energy()==0 && !frameWindowLen);
    frame(102,0x55,0x70); assert(energy()==pulse_energy(2));
    uart_error=true; hlw8032_task(); assert(!uart_error);

    fresh(); frame(0,0x55,0x70);
    for (u32 count=4;count<=12;count+=4) {
        make_frame(data,count,0x55,0x70); advance_us(100000);
        rx->dataLen=24; memcpy(rx->dataPayload,data,24); uartReceiveCallback();
    }
    hlw8032_task(); assert(energy()==pulse_energy(12)); /* delayed consumer */
    fresh(); frame(0,0x55,0x70);
    u8 batch[48]; make_frame(batch,4,0x55,0x70); make_frame(batch+24,8,0x55,0x70);
    advance_us(200000); feed_bytes(batch,48);
    assert(energy()==pulse_energy(8)); /* two frames in one continuous DMA packet */

    fresh(); frame(0,0x55,0x70);
    for (u32 n=1;n<=42;n++) frame(n,0x55,0);
    assert(!hlw8032_isOnline() && energy()==pulse_energy(42)); /* stale RMS, live energy */
    make_frame(data,43,0x55,0x70); be24(data+5,1); data[23]=0;
    for (u8 i=2;i<23;i++) data[23]+=data[i];
    advance_us(100000); feed_bytes(data,24); assert(!hlw8032_isOnline());
    frame(44,0x55,0x70); assert(hlw8032_isOnline());
    before=energy(); make_frame(data,45,0x55,0x70); be24(data+14,5000001); data[23]=0;
    for (u8 i=2;i<23;i++) data[23]+=data[i];
    advance_us(100000); feed_bytes(data,24); assert(energy()==before);

    fresh(); frame(10,0x55,0x70); frame(11,0x55,0x70);
    smartPlug_onRelayStateChanged(false); before=energy(); assert(flash_present);
    smartPlug_init(); hlw8032_init(); assert(energy()==before);
    frame(0,0x55,0x70); assert(energy()==before);
    frame(1,0x55,0x70); assert(energy()==before+pulse_energy(1));
    smartPlug_factoryReset(); assert(energy()==0 && energyRemainder==0);
    frame(2,0x55,0x70); assert(energy()==0);
    frame(3,0x55,0x70); assert(energy()==pulse_energy(1));

    fresh(); frame(0,0x55,0x70); g_smartPlugTelemetryIntervalMs=125;
    smartPlug_telemetryIntervalChanged(); advance_us(124000); smartPlug_task(); assert(reports==0);
    advance_us(1000); smartPlug_task(); assert(reports==2);
    assert(g_smartPlugElectricalAttrs.rmsVoltage==0xFFFF); /* empty window is invalid */
    g_smartPlugTelemetryIntervalMs=1; smartPlug_telemetryIntervalChanged();
    assert(g_smartPlugTelemetryIntervalMs==100);
    g_smartPlugTelemetryIntervalMs=4000000; smartPlug_telemetryIntervalChanged();
    assert(g_smartPlugTelemetryIntervalMs==3600000);
    u32 unchanged_writes=nv_writes; smartPlug_telemetryIntervalChanged();
    assert(nv_writes==unchanged_writes);
    flash_fail=true; smartPlug_addEnergy(1); advance_us(60000000); smartPlug_task();
    advance_us(60000000); smartPlug_task(); advance_us(60000000); smartPlug_task();
    advance_us(60000000); smartPlug_task(); advance_us(60000000); smartPlug_task();
    u32 writes=nv_writes; advance_us(1000); smartPlug_task(); assert(nv_writes==writes);
    smartPlug_onRelayStateChanged(false); assert(nv_writes==writes);
    g_smartPlugTelemetryIntervalMs=1000; smartPlug_telemetryIntervalChanged();
    assert(nv_writes==writes); /* failure backoff also covers forced saves */
    advance_us(5000000); smartPlug_task(); assert(nv_writes==writes+1);
    fresh(); now_ticks=0xFFF00000U; smartPlug_init();
    g_smartPlugTelemetryIntervalMs=125; smartPlug_telemetryIntervalChanged();
    frame(0,0x55,0x70); advance_us(75000); smartPlug_task();
    assert(reports==2); /* subtraction works across the hardware timer wrap */
    fresh(); smartPlug_addEnergy(~(u64)0);
    assert(energy()==SMART_PLUG_ENERGY_MAX_MICRO_WH);
    assert(g_smartPlugMeteringAttrs.currentSummationDelivered[0]==0xFE);
    smartPlug_addEnergy(1); assert(energy()==SMART_PLUG_ENERGY_MAX_MICRO_WH);
    fresh(); frame(0,0x55,0x70);
    memset(&aggregate,0,sizeof(aggregate));
    hlw8032_sample_t big={230000,70000,9000000,70000000,13,0};
    smartPlug_meterSample(&big); finishAggregationWindow();
    assert(g_smartPlugElectricalAttrs.rmsCurrent==0xFFFF);
    assert(g_smartPlugElectricalAttrs.apparentPower==0xFFFF);
    assert(g_smartPlugElectricalAttrs.activePower==9000);
    assert(g_smartPlugMeteringAttrs.instantaneousDemand[2]==0x80);
    advance_us(2100000); hlw8032_task(); smartPlug_task();
    assert(g_smartPlugMeteringAttrs.status==BIT(0) && g_smartPlugElectricalAttrs.rmsVoltage==0xFFFF);
    fresh(); frame(0,0x55,0x70);
    for (u32 count=1;count<=140000;count++) {
        frame(count & 0x1FFFF,0x55,0x70); smartPlug_task();
    }
    assert(energy()==pulse_energy(140000) && nv_writes>=23);
    puts("PASS: 140000 meter frames, multiple PF/system-clock wraps, 116-minute NV/reporting run");
    puts("PASS: meter frames, both PF wraps, chip resets, reconnect, checksum, split UART, ring overflow, rounding, NV restore/reset, reporting, flash retry");
    return 0;
}
