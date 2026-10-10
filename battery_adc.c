#include "app_cfg.h"
#include "tl_common.h"
#include "battery.h"

/* Use the SDK's factory-calibrated conversion, but wait for DMA completion
 * instead of assuming that each DFIFO entry is ready after a fixed delay. */
extern unsigned short adc_vref, adc_code;
extern volatile signed char adc_vref_offset;
extern unsigned char adc_pre_scale;

uint16_t sensor_batteryReadMv(void)
{
    volatile unsigned int data[8] __attribute__((aligned(16)));
    unsigned short sorted[8];
    const unsigned int pending = 0x80000000U; /* Impossible 14-bit ADC result. */
    dfifo_disable_dfifo2();
    adc_reset_adc_module();
    for (unsigned i=0; i<8; ++i) data[i]=pending;
    adc_config_misc_channel_buf((unsigned short *)data, sizeof(data));
    unsigned int started=clock_time();
    dfifo_enable_dfifo2();
    while (data[7]==pending) {
        if (clock_time_exceed(started, 2000)) {
            dfifo_disable_dfifo2();
            return 0;
        }
    }
    dfifo_disable_dfifo2();
    for (unsigned i=0; i<8; ++i) {
        if (data[i]==pending) return 0;
        unsigned short value=(data[i] & BIT(13)) ? 0 : (data[i] & 0x1FFF);
        unsigned j=i;
        while (j && sorted[j-1]>value) {
            sorted[j]=sorted[j-1];
            --j;
        }
        sorted[j]=value;
    }
    unsigned int average=(sorted[2]+sorted[3]+sorted[4]+sorted[5])/4U;
    adc_code=average;
    if (!average) return 0;
    int millivolts=(int)((average*adc_pre_scale*adc_vref)>>13)+adc_vref_offset;
    return millivolts>0 && millivolts<=65535 ? millivolts : 0;
}
