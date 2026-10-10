#pragma once
void led_on(u32 pin);
void led_off(u32 pin);
void led_init(void);
void app_buttons_init(void);
void app_key_handler(void);
void app_sleep(void);
void app_clockResume(void);
u32 app_nowMs(void);
