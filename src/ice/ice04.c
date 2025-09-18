/**
 * @file ice04.c
 * @author Joe Krachey (jkrachey@wisc.edu)
 * @brief 
 * @version 0.1
 * @date 2025-07-01
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "main.h"

#if defined(ICE04)
#include "drivers.h"
#include <stdio.h>
#include "buttons.h"

char APP_DESCRIPTION[] = "ECE353: ICE 04 - PWM Buzzer";

/*****************************************************************************/
/* Global Variables                                                          */
/*****************************************************************************/
cyhal_pwm_t pwm_red;
cyhal_pwm_t pwm_green;
cyhal_pwm_t pwm_blue;

uint8_t red_intensity = 0;    // 0-100 (%)
uint8_t green_intensity = 0;  // 0-100 (%)
uint8_t blue_intensity = 0;   // 0-100 (%)

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/**
 * @brief
 * This function will initialize all of the hardware resources for
 * the ICE
 */
void app_init_hw(void)
{
    cy_rslt_t rslt;

    console_init();
    printf("\x1b[2J\x1b[;H");
    printf("**************************************************\n\r");
    printf("* %s\n\r", APP_DESCRIPTION);
    printf("* Date: %s\n\r", __DATE__);
    printf("* Time: %s\n\r", __TIME__);
    printf("* Name:%s\n\r", NAME);
    printf("**************************************************\n\r");

    // Initialize buttons
    rslt = buttons_init_gpio();
    if (rslt != CY_RSLT_SUCCESS) {
        printf("ERROR: Buttons initialization failed!\n\r");
    }

    // Initialize PWM peripherals for RGB LEDs
    rslt = leds_init_pwm(&pwm_red, &pwm_green, &pwm_blue);
    if (rslt != CY_RSLT_SUCCESS) {
        printf("ERROR: PWM initialization failed!\n\r");
    }

    // Set initial duty cycle to 0% (LEDs off), period = 100 for percentage
    cyhal_pwm_set_duty_cycle(&pwm_red, red_intensity, 100);
    cyhal_pwm_start(&pwm_red);

    cyhal_pwm_set_duty_cycle(&pwm_green, green_intensity, 100);
    cyhal_pwm_start(&pwm_green);

    cyhal_pwm_set_duty_cycle(&pwm_blue, blue_intensity, 100);
    cyhal_pwm_start(&pwm_blue);
}

/*****************************************************************************/
/* Application Code                                                          */
/*****************************************************************************/
/**
 * @brief
 * This function implements the behavioral requirements for the ICE
 */
void app_main(void)
{
    while(1)
    {
        bool pressed = false;

        // SW1: Increase red intensity
        if (buttons_get_state(BUTTON_SW1) == BUTTON_STATE_LOW)
        {
            red_intensity += 10;
            if (red_intensity > 100) red_intensity = 0;
            cyhal_pwm_set_duty_cycle(&pwm_red, red_intensity, 100);
            cyhal_pwm_start(&pwm_red);
            pressed = true;
            while (buttons_get_state(BUTTON_SW1) == BUTTON_STATE_LOW); // Debounce
        }

        // SW2: Increase green intensity
        if (buttons_get_state(BUTTON_SW2) == BUTTON_STATE_LOW)
        {
            green_intensity += 10;
            if (green_intensity > 100) green_intensity = 0;
            cyhal_pwm_set_duty_cycle(&pwm_green, green_intensity, 100);
            cyhal_pwm_start(&pwm_green);
            pressed = true;
            while (buttons_get_state(BUTTON_SW2) == BUTTON_STATE_LOW); // Debounce
        }

        // SW3: Increase blue intensity
        if (buttons_get_state(BUTTON_SW3) == BUTTON_STATE_LOW)
        {
            blue_intensity += 10;
            if (blue_intensity > 100) blue_intensity = 0;
            cyhal_pwm_set_duty_cycle(&pwm_blue, blue_intensity, 100);
            cyhal_pwm_start(&pwm_blue);
            pressed = true;
            while (buttons_get_state(BUTTON_SW3) == BUTTON_STATE_LOW); // Debounce
        }

        // Print intensity levels if any button was pressed
        if (pressed)
        {
            printf("Red: %u%%, Green: %u%%, Blue: %u%%\n\r", red_intensity, green_intensity, blue_intensity);
        }
    }
}
#endif
