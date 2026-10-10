#if __PROJECT_TL_BOOT_LOADER__
#include "bootloader/app_cfg.h"
#else
#include "app_cfg.h"
#endif
#include "tl_common.h"
#include "board_io.h"
#define DISTINCT(a,b) typedef char distinct_##a##_##b[((a) != (b)) ? 1 : -1]
DISTINCT(SENSOR_LED_PIN, SENSOR_BUTTON_PIN);
DISTINCT(SENSOR_LED_PIN, SENSOR_REED_PIN);
DISTINCT(SENSOR_BUTTON_PIN, SENSOR_REED_PIN);
DISTINCT(SENSOR_VBAT_ADC_PIN, SENSOR_LED_PIN);
DISTINCT(SENSOR_VBAT_ADC_PIN, SENSOR_BUTTON_PIN);
DISTINCT(SENSOR_VBAT_ADC_PIN, SENSOR_REED_PIN);
void board_io_init(void)
{
    drv_gpio_output_en(SENSOR_LED_PIN, FALSE);
    drv_gpio_write(SENSOR_LED_PIN, LED_OFF);
    drv_gpio_func_set(SENSOR_LED_PIN);
    drv_gpio_input_en(SENSOR_LED_PIN, FALSE);
    drv_gpio_output_en(SENSOR_LED_PIN, TRUE);
    drv_gpio_output_en(SENSOR_BUTTON_PIN, FALSE);
    drv_gpio_func_set(SENSOR_BUTTON_PIN);
    drv_gpio_input_en(SENSOR_BUTTON_PIN, TRUE);
    drv_gpio_up_down_resistor(SENSOR_BUTTON_PIN, PM_PIN_PULLUP_10K);
    drv_gpio_output_en(SENSOR_REED_PIN, FALSE);
    drv_gpio_func_set(SENSOR_REED_PIN);
    drv_gpio_input_en(SENSOR_REED_PIN, TRUE);
    drv_gpio_up_down_resistor(SENSOR_REED_PIN, PM_PIN_PULLDOWN_100K);
}
