#pragma once
#include <stdint.h>
#include <stdbool.h>
#define SENSOR_DEBOUNCE_MS 20U
typedef struct {
    bool stable, raw;
    uint32_t changed;
} sensor_input_t;
typedef struct {
    sensor_input_t reed, button;
} sensor_engine_t;
void sensor_engine_init(sensor_engine_t *, bool closed, bool pressed, uint32_t now);
/* Bit 0: contact changed; bit 1: debounced button press/reset. */
unsigned sensor_engine_poll(sensor_engine_t *, bool closed, bool pressed, uint32_t now);
bool sensor_engine_busy(const sensor_engine_t *);
