#if __PROJECT_TL_BOOT_LOADER__
#include "bootloader/app_cfg.h"
#else
#include "app_cfg.h"
#endif
#include "tl_common.h"
#include "board_io.h"

/* SDK ADC supply checks drive their GPIO high. It must be a confirmed unused,
 * floating pin, distinct from every input/output, even with ADC app disabled. */
#define DISTINCT(a,b) typedef char distinct_##a##_##b[((a) != (b)) ? 1 : -1]
#define AGAINST(a,b,c,d,e,f) DISTINCT(a,b); DISTINCT(a,c); DISTINCT(a,d); DISTINCT(a,e); DISTINCT(a,f)
AGAINST(RELAY1_PIN, RELAY2_PIN, BUTTON1_PIN, BUTTON2_PIN, SERVICE_BUTTON_PIN, RELAY_LED_PIN);
AGAINST(RELAY_VBAT_ADC_PIN, RELAY1_PIN, RELAY2_PIN, BUTTON1_PIN, BUTTON2_PIN, SERVICE_BUTTON_PIN);
DISTINCT(RELAY_VBAT_ADC_PIN, RELAY_LED_PIN);
DISTINCT(RELAY2_PIN, BUTTON1_PIN); DISTINCT(RELAY2_PIN, BUTTON2_PIN);
DISTINCT(RELAY2_PIN, SERVICE_BUTTON_PIN); DISTINCT(RELAY2_PIN, RELAY_LED_PIN);
DISTINCT(BUTTON1_PIN, BUTTON2_PIN); DISTINCT(BUTTON1_PIN, SERVICE_BUTTON_PIN);
DISTINCT(BUTTON1_PIN, RELAY_LED_PIN); DISTINCT(BUTTON2_PIN, SERVICE_BUTTON_PIN);
DISTINCT(BUTTON2_PIN, RELAY_LED_PIN); DISTINCT(SERVICE_BUTTON_PIN, RELAY_LED_PIN);
#define LEVEL(a) typedef char valid_##a[((a) == 0 || (a) == 1) ? 1 : -1]
LEVEL(RELAY1_ACTIVE_LEVEL); LEVEL(RELAY2_ACTIVE_LEVEL); LEVEL(BUTTON1_ACTIVE_LEVEL);
LEVEL(BUTTON2_ACTIVE_LEVEL); LEVEL(SERVICE_BUTTON_ACTIVE_LEVEL); LEVEL(RELAY_LED_ACTIVE_LEVEL);

static void outputInit(u32 pin, bool active)
{
    drv_gpio_output_en(pin, FALSE);
    drv_gpio_write(pin, !active);
    drv_gpio_func_set(pin);
    drv_gpio_input_en(pin, FALSE);
    drv_gpio_up_down_resistor(pin, active ? PM_PIN_PULLDOWN_100K : PM_PIN_PULLUP_10K);
    drv_gpio_output_en(pin, TRUE);
}

static void inputInit(u32 pin, bool active)
{
    drv_gpio_output_en(pin, FALSE);
    drv_gpio_func_set(pin);
    drv_gpio_input_en(pin, TRUE);
    drv_gpio_up_down_resistor(pin, active ? PM_PIN_PULLDOWN_100K : PM_PIN_PULLUP_10K);
}

void board_io_init(void)
{
    outputInit(RELAY1_PIN, RELAY1_ACTIVE_LEVEL);
    outputInit(RELAY2_PIN, RELAY2_ACTIVE_LEVEL);
    outputInit(RELAY_LED_PIN, RELAY_LED_ACTIVE_LEVEL);
    inputInit(BUTTON1_PIN, BUTTON1_ACTIVE_LEVEL);
    inputInit(BUTTON2_PIN, BUTTON2_ACTIVE_LEVEL);
    inputInit(SERVICE_BUTTON_PIN, SERVICE_BUTTON_ACTIVE_LEVEL);
}
