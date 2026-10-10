#include "sensor_engine.h"
static bool sample(sensor_input_t *input, bool raw, uint32_t now)
{
    if (raw != input->raw) {
        input->raw = raw;
        input->changed = now;
    }
    if (raw != input->stable && (uint32_t)(now - input->changed) >= SENSOR_DEBOUNCE_MS) {
        input->stable = raw;
        return true;
    }
    return false;
}
void sensor_engine_init(sensor_engine_t *s, bool closed, bool pressed, uint32_t now)
{
    s->reed = (sensor_input_t){closed, closed, now};
    s->button = (sensor_input_t){pressed, pressed, now};
}
unsigned sensor_engine_poll(sensor_engine_t *s, bool closed, bool pressed, uint32_t now)
{
    unsigned events = sample(&s->reed, closed, now) ? 1U : 0U;
    /* Only a new press triggers reset. A held button at boot must be released
     * first, otherwise reboot after reset would repeat indefinitely. */
    if (sample(&s->button, pressed, now) && s->button.stable) events |= 2U;
    return events;
}
bool sensor_engine_busy(const sensor_engine_t *s)
{
    return s->button.stable || s->button.raw || s->reed.raw != s->reed.stable;
}
