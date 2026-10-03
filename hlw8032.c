#include "app_cfg.h"

#if (__PROJECT_TL_SMART_PLUG__) && (SMART_PLUG_METER_TYPE == SMART_PLUG_METER_HLW8032)

#include "tl_common.h"
#include "hlw8032.h"
#include "smart_plug.h"

#define HLW8032_UART_BAUDRATE              4800
#define HLW8032_UART_RX_BUFFER_SIZE        64
#define HLW8032_STREAM_BUFFER_SIZE         128
#define HLW8032_OFFLINE_TIMEOUT_US         2000000UL
#define HLW8032_ENERGY_DENOMINATOR         3600000000ULL
#define HLW8032_UART_BYTE_US               ((11UL * 1000000UL + HLW8032_UART_BAUDRATE - 1) / HLW8032_UART_BAUDRATE)

#ifndef HLW8032_VOLTAGE_COEFFICIENT_MILLI
#define HLW8032_VOLTAGE_COEFFICIENT_MILLI  1880UL
#endif
#ifndef HLW8032_CURRENT_COEFFICIENT_MILLI
#define HLW8032_CURRENT_COEFFICIENT_MILLI  1000UL
#endif

typedef struct {
    u32 dataLen;
    u8 dataPayload[HLW8032_UART_RX_BUFFER_SIZE - 4];
} hlw8032_uartRxData_t;

static _attribute_data_retention_ __attribute__((aligned(4))) u8 hlwUartRxBuffer[HLW8032_UART_RX_BUFFER_SIZE];
static volatile u8 streamBuffer[HLW8032_STREAM_BUFFER_SIZE];
static volatile u32 streamReceivedTime[HLW8032_STREAM_BUFFER_SIZE];
static volatile u8 streamWrite;
static volatile u8 streamRead;
static volatile bool streamOverflow;
static u8 frameWindow[HLW8032_FRAME_SIZE];
static u8 frameWindowLen;
static bool meterOnline;
static u32 lastValidFrameTime;
static bool pfInitialized;
static u32 lastPfValue;
static u64 energyRemainder;
static u8 measurementValidMask;
static u32 latestVoltageMv;
static u32 latestCurrentMa;
static u32 latestActivePowerMw;
static u32 measurementUpdatedTime[3];
static u32 lastPowerParam;

static void updateMeasurement(u8 index, u64 value, u32 maximum, u32 receivedTime)
{
    measurementUpdatedTime[index] = receivedTime;
    if (value > maximum) {
        measurementValidMask &= ~BIT(index);
        return;
    }
    if (index == 0) latestVoltageMv = (u32)value;
    else if (index == 1) latestCurrentMa = (u32)value;
    else latestActivePowerMw = (u32)value;
    measurementValidMask |= BIT(index);
}

static u32 readBe24(const u8 *p)
{
    return ((u32)p[0] << 16) | ((u32)p[1] << 8) | p[2];
}

static bool frameChecksumValid(const u8 *frame)
{
    u8 checksum = 0;
    for (u8 i = 2; i < 23; i++) {
        checksum += frame[i];
    }
    return checksum == frame[23];
}

static bool frameHeaderValid(const u8 *frame)
{
    u8 state = frame[0];
    return (frame[1] == 0x5A) &&
           ((state == 0x55) || (state == 0xAA) || ((state & 0xF0) == 0xF0));
}

static void processFrame(const u8 *frame, u32 receivedTime)
{
    if (!frameHeaderValid(frame) || !frameChecksumValid(frame)) {
        return;
    }

    u8 state = frame[0];
    u8 update = frame[20];
    /* 0x55 is a special normal-state marker, not a set of error bits. */
    if ((state == 0xAA) || ((state != 0x55) && (state & BIT(0)))) {
        meterOnline = FALSE;
        measurementValidMask = 0;
        pfInitialized = FALSE;
        return;
    }
    if (meterOnline && (u32)(receivedTime - lastValidFrameTime) >
        HLW8032_OFFLINE_TIMEOUT_US * CLOCK_16M_SYS_TIMER_CLK_1US) {
        meterOnline = FALSE;
        measurementValidMask = 0;
        pfInitialized = FALSE;
    }
    u8 overflow = (state == 0x55) ? 0 : state;

    u32 voltageParam = readBe24(&frame[2]);
    u32 voltageReg = readBe24(&frame[5]);
    u32 currentParam = readBe24(&frame[8]);
    u32 currentReg = readBe24(&frame[11]);
    u32 powerParam = readBe24(&frame[14]);
    u32 powerReg = readBe24(&frame[17]);
    if (!voltageParam || !currentParam || !powerParam) {
        meterOnline = FALSE;
        measurementValidMask = 0;
        pfInitialized = FALSE;
        return;
    }

    if (overflow & BIT(3)) {
        updateMeasurement(0, 0, 1000000UL, receivedTime);
    } else if ((update & BIT(6)) && voltageReg) {
        updateMeasurement(0, ((u64)voltageParam * HLW8032_VOLTAGE_COEFFICIENT_MILLI) / voltageReg,
                          1000000UL, receivedTime);
    } else if ((update & BIT(6)) && !voltageReg) {
        measurementValidMask &= ~BIT(0);
    }
    if (overflow & BIT(2)) {
        updateMeasurement(1, 0, 1000000UL, receivedTime);
    } else if ((update & BIT(5)) && !currentReg) {
        measurementValidMask &= ~BIT(1);
    } else if ((update & BIT(5)) && currentReg) {
        updateMeasurement(1, ((u64)currentParam * HLW8032_CURRENT_COEFFICIENT_MILLI) / currentReg,
                          1000000UL, receivedTime);
    }
    if (overflow & BIT(1)) {
        updateMeasurement(2, 0, HLW8032_MAX_POWER_MW, receivedTime);
    } else if ((update & BIT(4)) && !powerReg) {
        measurementValidMask &= ~BIT(2);
    } else if ((update & BIT(4)) && powerReg) {
        updateMeasurement(2, ((u64)powerParam * HLW8032_VOLTAGE_COEFFICIENT_MILLI *
                                     HLW8032_CURRENT_COEFFICIENT_MILLI) /
                                    ((u64)powerReg * 1000ULL), HLW8032_MAX_POWER_MW, receivedTime);
    }
    for (u8 i = 0; i < 3; i++) {
        if ((u32)(receivedTime - measurementUpdatedTime[i]) >
            HLW8032_OFFLINE_TIMEOUT_US * CLOCK_16M_SYS_TIMER_CLK_1US) {
            measurementValidMask &= ~BIT(i);
        }
    }

    hlw8032_sample_t sample;
    memset((u8 *)&sample, 0, sizeof(sample));
    sample.voltageMv = latestVoltageMv;
    sample.currentMa = latestCurrentMa;
    sample.activePowerMw = latestActivePowerMw;
    sample.apparentPowerMva = (u32)min2(((u64)sample.voltageMv * sample.currentMa) / 1000ULL,
                                      0xFFFFFFFFULL);
    if (sample.apparentPowerMva) {
        u32 pf = (u32)(((u64)sample.activePowerMw * 100ULL) / sample.apparentPowerMva);
        sample.powerFactor = (pf > 100) ? 100 : (u8)pf;
    }

    /* UART exposes a 17-bit counter: carry toggle followed by PF REG. */
    u32 pfValue = ((update & BIT(7)) ? 0x10000UL : 0) |
                  ((u16)frame[21] << 8) | frame[22];
    if (pfInitialized && powerParam == lastPowerParam) {
        u32 pulseDelta = (pfValue - lastPfValue) & 0x1FFFFUL;
        u32 elapsedUs = (u32)(receivedTime - lastValidFrameTime) /
                        CLOCK_16M_SYS_TIMER_CLK_1US;
        u64 pulseEnergy = (u64)powerParam * HLW8032_VOLTAGE_COEFFICIENT_MILLI *
                         HLW8032_CURRENT_COEFFICIENT_MILLI;
        /* A chip reset can also change bit7. Reject physically impossible jumps.
         * Two pulses of slack allow batching and quantization at frame boundaries.
         * A reset indistinguishable from a plausible wrap cannot be detected by UART alone. */
        u64 maxPulses = pulseEnergy
            ? ((u64)HLW8032_MAX_POWER_MW * elapsedUs * 1000ULL) / pulseEnergy + 2
            : 0;
        if (pulseEnergy && (pulseDelta <= maxPulses)) {
            u64 energy = pulseDelta * pulseEnergy + energyRemainder;
            sample.energyDeltaMicroWh = energy / HLW8032_ENERGY_DENOMINATOR;
            energyRemainder = energy % HLW8032_ENERGY_DENOMINATOR;
        }
    }
    pfInitialized = TRUE;
    lastPfValue = pfValue;
    lastPowerParam = powerParam;

    meterOnline = TRUE;
    lastValidFrameTime = receivedTime;
    if (measurementValidMask == (BIT(0) | BIT(1) | BIT(2))) {
        smartPlug_meterSample(&sample);
    } else if (sample.energyDeltaMicroWh) {
        /* Energy counting must not depend on the RMS registers updating. */
        smartPlug_addEnergy(sample.energyDeltaMicroWh);
    }
}

static void uartReceiveCallback(void)
{
    hlw8032_uartRxData_t *rx = (hlw8032_uartRxData_t *)hlwUartRxBuffer;
    if (uart_is_parity_error()) {
        streamOverflow = TRUE;
        rx->dataLen = 0;
        return; /* The task clears the hardware error and resynchronizes. */
    }
    u32 len = rx->dataLen;
    if (len > sizeof(rx->dataPayload)) {
        len = sizeof(rx->dataPayload);
    }
    u32 receivedTime = clock_time();

    for (u32 i = 0; i < len; i++) {
        u8 next = (u8)(streamWrite + 1);
        if (next >= HLW8032_STREAM_BUFFER_SIZE) {
            next = 0;
        }
        if (next == streamRead) {
            streamOverflow = TRUE;
            break; /* Only the consumer is allowed to advance streamRead. */
        }
        streamBuffer[streamWrite] = rx->dataPayload[i];
        /* Backdate bytes within one DMA packet using 4800 8E1 wire time.
         * Separate packets retain their IRQ arrival time even if the task stalls. */
        streamReceivedTime[streamWrite] = receivedTime -
            (len - i - 1) * HLW8032_UART_BYTE_US * CLOCK_16M_SYS_TIMER_CLK_1US;
        streamWrite = next;
    }
    rx->dataLen = 0;
}

void hlw8032_init(void)
{
    streamWrite = 0;
    streamRead = 0;
    streamOverflow = FALSE;
    frameWindowLen = 0;
    meterOnline = FALSE;
    pfInitialized = FALSE;
    measurementValidMask = 0;
    energyRemainder = 0;
    latestVoltageMv = latestCurrentMa = latestActivePowerMw = 0;
    memset(measurementUpdatedTime, 0, sizeof(measurementUpdatedTime));
    memset(hlwUartRxBuffer, 0, sizeof(hlwUartRxBuffer));

    drv_uart_pin_set(HLW8032_UART_TX_PIN, HLW8032_UART_RX_PIN);
    drv_uart_init(HLW8032_UART_BAUDRATE, hlwUartRxBuffer, sizeof(hlwUartRxBuffer), uartReceiveCallback);
    uart_init_baudrate(HLW8032_UART_BAUDRATE, UART_CLOCK_SOURCE, PARITY_EVEN, STOP_BIT_ONE);
}

void hlw8032_task(void)
{
    u8 errorIrqState = irq_disable();
    if (uart_is_parity_error()) {
        uart_clear_parity_error(); /* Also clears the RX FIFO on TLSR8258. */
        streamOverflow = TRUE;
        ((hlw8032_uartRxData_t *)hlwUartRxBuffer)->dataLen = 0;
    }
    irq_restore(errorIrqState);

    for (;;) {
        u8 irqState = irq_disable();
        if (streamOverflow) {
            streamRead = streamWrite;
            streamOverflow = FALSE;
            frameWindowLen = 0;
        }
        if (streamRead == streamWrite) {
            irq_restore(irqState);
            break;
        }
        u8 byte = streamBuffer[streamRead];
        u32 receivedTime = streamReceivedTime[streamRead];
        streamRead++;
        if (streamRead >= HLW8032_STREAM_BUFFER_SIZE) {
            streamRead = 0;
        }
        irq_restore(irqState);

        if (frameWindowLen < HLW8032_FRAME_SIZE) {
            frameWindow[frameWindowLen++] = byte;
        }
        if (frameWindowLen == HLW8032_FRAME_SIZE) {
            if (frameHeaderValid(frameWindow) && frameChecksumValid(frameWindow)) {
                processFrame(frameWindow, receivedTime);
                frameWindowLen = 0;
            } else {
                memmove(frameWindow, frameWindow + 1, HLW8032_FRAME_SIZE - 1);
                frameWindowLen = HLW8032_FRAME_SIZE - 1;
            }
        }
    }

    if (meterOnline && clock_time_exceed(lastValidFrameTime, HLW8032_OFFLINE_TIMEOUT_US)) {
        meterOnline = FALSE;
        pfInitialized = FALSE;
        measurementValidMask = 0;
    }
}

bool hlw8032_isOnline(void)
{
    return meterOnline && (measurementValidMask == (BIT(0) | BIT(1) | BIT(2)));
}

void hlw8032_resetEnergyBaseline(void)
{
    pfInitialized = FALSE;
    energyRemainder = 0;
}

#endif
