#pragma once
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
#define __PROJECT_TL_SWDS1X__ 1
#define CLOCK_16M_SYS_TIMER_CLK_1US 16
static u32 now_ticks;
static u32 clock_time(void) { return now_ticks; }
static bool clock_time_exceed(u32 start, u32 us) { return (u32)(now_ticks-start) > us*16U; }
static bool zb_isDeviceJoinedNwk(void) { return true; }
