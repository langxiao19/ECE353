/**
 * @file task_buzzer.c
 * @author Joe Krachey (jkrachey@wisc.edu)
 * @brief 
 * @version 0.1
 * @date 2025-08-13
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "main.h"

#ifdef ECE353_FREERTOS

#include "task_buzzer.h"
#include "task_console.h"

/**
 * @brief 
 * Task used to control the buzzer based on button events.
 * 
 * SW1 -- Turn buzzer on
 * SW2 -- Turn buzzer off
 *
 * @param arg 
 * Unused parameter
 */
void task_buzzer(void *arg)
{
    (void)arg; // Unused parameter
    
    EventBits_t events;
    bool buzzer_state = false;  // Track current buzzer state
    
    while (1)
    {
        // Wait for SW1 or SW2 button press events
        events = xEventGroupWaitBits(
            ECE353_RTOS_Events,                          // Event group handle
            ECE353_EVENT_SW1_PRESSED | ECE353_EVENT_SW2_PRESSED,  // Bits to wait for
            pdTRUE,                                      // Clear bits on exit
            pdFALSE,                                     // Wait for ANY bit (OR operation)
            portMAX_DELAY                                // Wait indefinitely
        );
        
        // Check which button was pressed and control buzzer accordingly
        if (events & ECE353_EVENT_SW1_PRESSED) {
            // SW1 pressed - Turn buzzer ON
            if (!buzzer_state) {
                buzzer_on();
                buzzer_state = true;
                task_console_printf("Buzzer ON (SW1 pressed)\n");
            }
        }
        
        if (events & ECE353_EVENT_SW2_PRESSED) {
            // SW2 pressed - Turn buzzer OFF
            if (buzzer_state) {
                buzzer_off();
                buzzer_state = false;
                task_console_printf("Buzzer OFF (SW2 pressed)\n");
            }
        }
    }
}
#endif