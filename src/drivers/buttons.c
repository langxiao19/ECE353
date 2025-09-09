/**
 * @file buttons.c
 * @author Joe Krachey (jkrachey@wisc.edu)
 * @brief 
 * @version 0.1
 * @date 2025-06-30
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "buttons.h"

// Initialize GPIO pins for buttons as input with pull-up resistors
cy_rslt_t buttons_init_gpio(void)
{
    cy_rslt_t rslt = CY_RSLT_SUCCESS;
    rslt |= cyhal_gpio_init(PIN_BUTTON_SW1, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_PULLUP, CYBSP_BTN1);
    rslt |= cyhal_gpio_init(PIN_BUTTON_SW2, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_PULLUP, CYBSP_BTN2);
    rslt |= cyhal_gpio_init(PIN_BUTTON_SW3, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_PULLUP, CYBSP_BTN3);
    return rslt;
}

// Optimized button state detection
button_state_t buttons_get_state(ece353_button_t button)
{
    static uint8_t prev_state[3] = {1, 1, 1}; // Assume pull-up, so default HIGH
    uint8_t curr_state;
    button_state_t state;

    switch (button) {
        case BUTTON_SW1:
            curr_state = cyhal_gpio_read(PIN_BUTTON_SW1);
            break;
        case BUTTON_SW2:
            curr_state = cyhal_gpio_read(PIN_BUTTON_SW2);
            break;
        case BUTTON_SW3:
            curr_state = cyhal_gpio_read(PIN_BUTTON_SW3);
            break;
        default:
            return BUTTON_STATE_HIGH;
    }

    if (prev_state[button] == 1 && curr_state == 0)
        state = BUTTON_STATE_FALLING_EDGE;
    else if (prev_state[button] == 0 && curr_state == 0)
        state = BUTTON_STATE_LOW;
    else if (prev_state[button] == 0 && curr_state == 1)
        state = BUTTON_STATE_RISING_EDGE;
    else
        state = BUTTON_STATE_HIGH;

    prev_state[button] = curr_state;
    return state;
}


