/**
 * @file timer.c
 * @author Joe Krachey (jkrachey@wisc.edu)
 * @brief Timer initialization and configuration
 * @version 0.1
 * @date 2024-08-14
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include "timer.h"
#include <complex.h>

cy_rslt_t timer_init(cyhal_timer_t *timer_obj, cyhal_timer_cfg_t *timer_cfg, uint32_t ticks, void *Handler)
{
    cy_rslt_t rslt;

    timer_cfg->period = ticks; // Set the timer period to the specified number of ticks
    timer_cfg->direction = CYHAL_TIMER_DIR_UP; // Set the timer to count up
    timer_cfg->is_compare = false; // disable compare mode
    timer_cfg->is_continuous = true; // Set the timer to continuous mode
    timer_cfg->value = 0; // Initialize the timer value to 0

    // Initialize timer with correct parameter order and types
    rslt = cyhal_timer_init(timer_obj, NC, NULL); // NC is the unused GPIO pin, NULL for default clock
    if (rslt != CY_RSLT_SUCCESS)
    {
        return rslt; // Return if initialization failed
    }

    rslt = cyhal_timer_configure(timer_obj, timer_cfg); // Configure the timer with the specified settings
    if (rslt != CY_RSLT_SUCCESS)
    {
        return rslt; // Return if configuration failed
    }

    // Set timer frequency
    rslt = cyhal_timer_set_frequency(timer_obj, 100000000);
    if (rslt != CY_RSLT_SUCCESS)
    {
        return rslt; // Return if callback registration failed
    }

    // Register callback without assignment (function returns void)
    cyhal_timer_register_callback(timer_obj, Handler, NULL); // Register the callback function for timer interrupts

    // Enable timer event with correct parameter order
    cyhal_timer_enable_event(timer_obj, CYHAL_TIMER_IRQ_TERMINAL_COUNT, 3, true); // Enable terminal count interrupts with priority 7
    
    rslt = cyhal_timer_start(timer_obj); // Start the timer
    if (rslt != CY_RSLT_SUCCESS)
    {
        return rslt; // Return if starting the timer failed
    }

    return rslt; // Return the result of the initialization
}