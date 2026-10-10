#include "sensor_engine.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
int main(void)
{
    sensor_engine_t s;
    sensor_engine_init(&s, true, false, 0);
    assert(!sensor_engine_poll(&s, false, false, 1));
    assert(sensor_engine_busy(&s));
    assert(!sensor_engine_poll(&s, true, false, 10));
    assert(!sensor_engine_poll(&s, true, false, 30));
    assert(!sensor_engine_busy(&s));
    assert(!sensor_engine_poll(&s, false, false, 40));
    assert(!sensor_engine_poll(&s, false, false, 59));
    assert(sensor_engine_poll(&s, false, false, 60) == 1);
    assert(!s.reed.stable && !sensor_engine_busy(&s));
    assert(!sensor_engine_poll(&s, true, false, 100));
    assert(sensor_engine_poll(&s, true, false, 120) == 1 && s.reed.stable);
    // Press triggers after debounce, once per press; no hold deadline.
    assert(!sensor_engine_poll(&s, true, true, 200));
    assert(!sensor_engine_poll(&s, true, true, 219));
    assert(sensor_engine_poll(&s, true, true, 220) == 2);
    assert(!sensor_engine_poll(&s, true, true, 10220));
    assert(!sensor_engine_poll(&s, true, false, 10221));
    assert(!sensor_engine_poll(&s, true, false, 10241));
    assert(!sensor_engine_busy(&s));
    sensor_engine_poll(&s, true, true, 11000);
    assert(sensor_engine_poll(&s, true, true, 11020)==2);
    // Brief low glitch must not reset.
    sensor_engine_init(&s, false, false, 0);
    sensor_engine_poll(&s, false, true, 1);
    assert(!sensor_engine_poll(&s, false, false, 19));
    assert(!sensor_engine_poll(&s, false, false, 39));
    // A held button across reboot must not start a reset loop.
    sensor_engine_init(&s, true, true, 0);
    assert(!sensor_engine_poll(&s, true, true, 20000));
    sensor_engine_poll(&s, true, false, 20001);
    sensor_engine_poll(&s, true, false, 20021);
    sensor_engine_poll(&s, true, true, 21000);
    assert(sensor_engine_poll(&s, true, true, 21020)==2);
    // Debounce across hardware time wrap.
    sensor_engine_init(&s, true, false, UINT32_MAX-10U);
    sensor_engine_poll(&s, true, true, UINT32_MAX-5U);
    assert(sensor_engine_poll(&s, true, true, 14) == 2);
    sensor_engine_init(&s, false, false, UINT32_MAX-10U);
    assert(!sensor_engine_poll(&s, true, false, UINT32_MAX-5U));
    assert(sensor_engine_poll(&s, true, false, 14) == 1);
    puts("PASS: contact debounce, both edges, immediate debounced reset, noise rejection, held boot, one-shot and clock wrap");
}
