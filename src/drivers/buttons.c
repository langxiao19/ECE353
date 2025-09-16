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
    rslt |= cyhal_gpio_init(PIN_BUTTON_SW1, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_PULLUP, true);
    rslt |= cyhal_gpio_init(PIN_BUTTON_SW2, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_PULLUP, true);
    rslt |= cyhal_gpio_init(PIN_BUTTON_SW3, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_PULLUP, true);
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

static cyhal_timer_t button_timer; // Timer object for button debouncing
static cyhal_timer_cfg_t button_timer_cfg; // Timer configuration structure
// Function to initialize the timer for button debouncing

static void button_timer_handler(void *arg, cyhal_timer_event_t event)
{
    static uint8_t button_counts[3] = {0, 0, 0};

    uint8_t sw1 = (PORT_BUTTON_SW1->IN & MASK_BUTTON_PIN_SW1) ? 1 : 0;
    uint8_t sw2 = (PORT_BUTTON_SW2->IN & MASK_BUTTON_PIN_SW2) ? 1 : 0;
    uint8_t sw3 = (PORT_BUTTON_SW3->IN & MASK_BUTTON_PIN_SW3) ? 1 : 0;

    // SW1 debounce
    if (sw1 == 0) {
        if (button_counts[0] < 255) button_counts[0]++;
        if (button_counts[0] == 5) {
            ECE353_Events.sw1 = 1;
        }
    } else {
        button_counts[0] = 0;
    }

    // SW2 debounce
    if (sw2 == 0) {
        if (button_counts[1] < 255) button_counts[1]++;
        if (button_counts[1] == 5) {
            ECE353_Events.sw2 = 1;
        }
    } else {
        button_counts[1] = 0;
    }

    // SW3 debounce
    if (sw3 == 0) {
        if (button_counts[2] < 255) button_counts[2]++;
        if (button_counts[2] == 5) {
            ECE353_Events.sw3 = 1;
        }
    } else {
        button_counts[2] = 0;
    }
}
cy_rslt_t buttons_init_timer(void)
{
    return timer_init(&button_timer, &button_timer_cfg, 500000, button_timer_handler);
}

