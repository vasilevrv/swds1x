#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define BIT(n) (1U<<(n))
unsigned short adc_vref=1174, adc_code;
volatile signed char adc_vref_offset=-3;
unsigned char adc_pre_scale=8;
static volatile unsigned int *dma;
static unsigned int values[8], elapsed, now;
static bool enabled, stalled, missingEntry;
static void dfifo_disable_dfifo2(void) {enabled=false;}
static void adc_reset_adc_module(void) {assert(!enabled);elapsed=0;}
static void adc_config_misc_channel_buf(unsigned short *data, unsigned int size)
{
    assert(size==32 && ((uintptr_t)data%16)==0);
    dma=(volatile unsigned int *)data;
}
static unsigned int clock_time(void) {return now;}
static void dfifo_enable_dfifo2(void) {enabled=true;}
static bool clock_time_exceed(unsigned int start, unsigned int us)
{
    assert(enabled);
    now+=10;elapsed+=10;
    /* DMA produces one word per 50 us, including a legitimate zero value.
     * The last word arrives only after all earlier conversion slots. */
    if (!stalled && elapsed<=400 && elapsed%50==0) {
        unsigned i=elapsed/50-1;
        if (!(missingEntry && i==2)) dma[i]=values[i];
    }
    return (unsigned int)(now-start)>us;
}
#include "battery_adc_sources.inc"
int main(void)
{
    for (unsigned i=0;i<8;++i) values[i]=2630;
    assert(sensor_batteryReadMv()==3012 && adc_code==2630 && !enabled);
    assert(elapsed==400); /* Do not read incomplete slots after a fixed delay. */
    values[0]=0;values[1]=BIT(13);values[6]=7000;values[7]=8191;
    assert(sensor_batteryReadMv()==3012 && !enabled); /* Trim both extremes. */
    for (unsigned i=0;i<8;++i) values[i]=0;
    assert(sensor_batteryReadMv()==0 && elapsed==400 && !enabled);
    stalled=true;
    assert(sensor_batteryReadMv()==0 && elapsed<=2010 && !enabled);
    stalled=false;missingEntry=true;
    assert(sensor_batteryReadMv()==0 && !enabled);
    missingEntry=false;
    for (unsigned i=0;i<8;++i) values[i]=2630;
    now=UINT32_MAX-200;
    assert(sensor_batteryReadMv()==3012 && !enabled); /* Timeout clock wrap. */
    adc_vref_offset=-20;
    for (unsigned i=0;i<8;++i) values[i]=1;
    assert(sensor_batteryReadMv()==0); /* Signed offset cannot wrap positive. */
    puts("PASS: ADC DMA completion/alignment, real calibration, zero/negative/outliers, bounded timeout and clock wrap");
}
