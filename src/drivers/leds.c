/**
 * @file leds.c
 * @author Joe Krachey (jkrachey@wisc.edu)
 * @brief 
 * @version 0.1
 * @date 2025-06-30
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "leds.h"

// Array of LED pin numbers, indexed by ece353_led_t
static const uint8_t led_pins[LED_NUM] = {
    PIN_LED_RED,
    PIN_LED_GREEN,
    PIN_LED_BLUE
};

// Initialize the GPIO pins for the LEDs as outputs, default OFF
cy_rslt_t leds_init_gpio(void)
{
    cy_rslt_t rslt = CY_RSLT_SUCCESS;
    for (int i = 0; i < LED_NUM; i++) {
        // LEDs are active HIGH: set initial value LOW (off)
        rslt |= cyhal_gpio_init(led_pins[i], CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, 0);
    }
    return rslt;
}

// Set the state of a specific LED
void leds_set_state(ece353_led_t led, ece353_led_state_t state)
{
    if (led >= LED_NUM) return;
    // LEDs are active HIGH: 1 = ON, 0 = OFF
    cyhal_gpio_write(led_pins[led], state == LED_ON ? 1 : 0);
}

// Function that configures the RGB LED pins to be controlled by PWM
cy_rslt_t leds_init_pwm(
    cyhal_pwm_t *pwm_obj_red,
    cyhal_pwm_t *pwm_obj_green,
    cyhal_pwm_t *pwm_obj_blue
)
{
    cy_rslt_t rslt = CY_RSLT_SUCCESS;

    // Initialize PWM for RED LED
    rslt |= cyhal_pwm_init(pwm_obj_red, PIN_LED_RED, NULL);

    // Initialize PWM for GREEN LED
    rslt |= cyhal_pwm_init(pwm_obj_green, PIN_LED_GREEN, NULL);

    // Initialize PWM for BLUE LED
    rslt |= cyhal_pwm_init(pwm_obj_blue, PIN_LED_BLUE, NULL);

    return rslt;
}