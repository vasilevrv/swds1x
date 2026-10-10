#pragma once
/* TLSR8258 door/window board. */
#define SENSOR_LED_PIN GPIO_PB4
#define SENSOR_BUTTON_PIN GPIO_PD3
#define SENSOR_REED_PIN GPIO_PD7
/* PC5 is not bonded out on TLSR8258F512ET32. SDK drives it HIGH and
 * samples the GPIO ADC channel to measure VDD (not the direct VBAT input). */
#define SENSOR_VBAT_ADC_PIN GPIO_PC5
