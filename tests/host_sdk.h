#pragma once

/* Hardware/stack substitutes for exercising the production parser and meter state. */
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
#define TRUE true
#define FALSE false
#define BIT(n) (1U << (n))
#define min2(a,b) ((a) < (b) ? (a) : (b))
#define max2(a,b) ((a) > (b) ? (a) : (b))
#define _attribute_packed_ __attribute__((packed))
#define _attribute_data_retention_
#define __PROJECT_TL_SMART_PLUG__ 1
/* Existing parser fixtures explicitly exercise the retained UART backend. */
#ifndef SMART_PLUG_METER_TYPE
#define SMART_PLUG_METER_TYPE 2
#endif
#include "../meter_config.h"
#define CLOCK_16M_SYS_TIMER_CLK_1US 16
#define HLW8032_VOLTAGE_COEFFICIENT_MILLI 1995UL
#define HLW8032_CURRENT_COEFFICIENT_MILLI 1000UL
#define HLW8032_MAX_POWER_MW 25000000UL
#define HLW8032_UART_TX_PIN 2
#define HLW8032_UART_RX_PIN 3
#define UART_CLOCK_SOURCE 48000000
#define PARITY_EVEN 1
#define STOP_BIT_ONE 1
#define NV_ENABLE 1
#define NV_MODULE_APP 6
#define NV_SUCC 0
#define APS_SHORT_DSTADDR_WITHEP 2
#define HA_PROFILE_ID 0x0104
#define ZCL_FRAME_SERVER_CLIENT_DIR 1
#define ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT 0x0B04
#define ZCL_CLUSTER_SE_METERING 0x0702
#define ZCL_ATTRID_RMS_VOLTAGE 0x0505
#define ZCL_ATTRID_RMS_CURRENT 0x0508
#define ZCL_ATTRID_ACTIVE_POWER 0x050B
#define ZCL_ATTRID_APPARENT_POWER 0x050F
#define ZCL_ATTRID_POWER_FACTOR 0x0510
#define ZCL_ATTRID_CURRENT_SUMMATION_DELIVERD 0
#define ZCL_ATTRID_INSTANTANEOUS_DEMAND 0x0400
#define ZCL_DATA_TYPE_UINT16 0x21
#define ZCL_DATA_TYPE_INT16 0x29
#define ZCL_DATA_TYPE_INT8 0x28
#define ZCL_DATA_TYPE_UINT48 0x25
#define ZCL_DATA_TYPE_INT24 0x2A
#define TL_SETSTRUCTCONTENT(s,v) memset(&(s),(v),sizeof(s))
typedef struct { u16 shortAddr; } addr_t;
typedef struct { addr_t dstAddr; u8 dstAddrMode, dstEp; u16 profileId; } epInfo_t;
typedef struct { u16 attrID; u8 dataType; u8 *attrData; } zclReport_t;
typedef struct { u8 numAttr; zclReport_t attrList[]; } zclReportCmd_t;
static u32 now_ticks, reports, nv_writes;
static bool flash_present, flash_fail, uart_error;
static u8 flash_state[64];
static u32 clock_time(void) { return now_ticks; }
static bool clock_time_exceed(u32 start, u32 us) { return (u32)(now_ticks-start) > us*16U; }
static u8 irq_disable(void) { return 1; }
static void irq_restore(u8 state) { (void)state; }
static void drv_uart_pin_set(u32 tx, u32 rx) { (void)tx; (void)rx; }
static u8 drv_uart_init(u32 baud, u8 *buf, u16 len, void (*cb)(void)) {
    (void)baud; (void)buf; (void)len; (void)cb; return 0;
}
static void uart_init_baudrate(u32 baud, u32 clock, u8 parity, u8 stop) {
    (void)baud; (void)clock; (void)parity; (void)stop;
}
static bool uart_is_parity_error(void) { return uart_error; }
static void uart_clear_parity_error(void) { uart_error = false; }
static u8 nv_flashReadNew(u8 fresh,u8 module,u8 item,u32 len,u8 *data) {
    (void)fresh; (void)module; (void)item;
    if (!flash_present) return 1;
    memcpy(data,flash_state,len); return NV_SUCC;
}
static u8 nv_flashWriteNew(u8 fresh,u8 module,u8 item,u32 len,u8 *data) {
    (void)fresh; (void)module; (void)item; nv_writes++;
    if (flash_fail) return 1;
    memcpy(flash_state,data,len); flash_present=true; return NV_SUCC;
}
static bool zb_isDeviceJoinedNwk(void) { return true; }
static void zcl_sendReportAttrsCmd(u8 ep,epInfo_t *dst,bool disable,u8 dir,u16 cluster,zclReportCmd_t *report) {
    (void)ep; (void)dst; (void)disable; (void)dir; (void)cluster;
    assert(report->numAttr==2 || report->numAttr==5); reports++;
}
#ifndef HOST_CONTROL_TEST
void statusLed_blinkStart(u16 times,u16 on,u16 off) { (void)times; (void)on; (void)off; }
#endif
