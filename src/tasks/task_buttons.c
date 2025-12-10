/**
 * @file task_buttons.c
 * @author Joe Krachey (jkrachey@wisc.edu)
 * @brief 
 * @version 0.1
 * @date 2025-08-13
 * 
 * @copyright Copyright (c) 2025
 * 
 */

 #include "task_buttons.h"
 #include "task_console.h"

 #ifdef ECE353_FREERTOS
 /**
  * @brief 
  * Task used to debounce button presses (SW1, SW2, SW3).  
  * The falling edge of the button press is detected by de-bouncing
  * the button for 30mS. Each button should be sampled every 15mS.
  *
  * When a button press is detected, the corresponding event is set in
  * in the event group ECE353_RTOS_Events.
  *
  * @param arg 
  * Unused parameter
  */
 void task_buttons(void *arg)
 {
    (void)arg; // Unused parameter

    // Bridge task: reads timer interrupt flags and sets FreeRTOS event bits
    while (1)
    {
        // Check if timer interrupt detected SW1 press
        if (ECE353_Events.sw1 == 1) {
            ECE353_Events.sw1 = 0;  // Clear flag
            xEventGroupSetBits(ECE353_RTOS_Events, ECE353_EVENT_SW1_PRESSED);
            task_console_printf("SW1 Button Pressed\n");
        }

        // Check if timer interrupt detected SW2 press
        if (ECE353_Events.sw2 == 1) {
            ECE353_Events.sw2 = 0;  // Clear flag
            xEventGroupSetBits(ECE353_RTOS_Events, ECE353_EVENT_SW2_PRESSED);
            task_console_printf("SW2 Button Pressed\n");
        }

        // Check if timer interrupt detected SW3 press
        if (ECE353_Events.sw3 == 1) {
            ECE353_Events.sw3 = 0;  // Clear flag
            xEventGroupSetBits(ECE353_RTOS_Events, ECE353_EVENT_SW3_PRESSED);
            task_console_printf("SW3 Button Pressed\n");
        }

        // Check frequently (10ms) to quickly bridge interrupt flags to event groups
        vTaskDelay(pdMS_TO_TICKS(10));
    }
 }

 /* Button Task Initialization */
bool task_buttons_init(void){

    cy_rslt_t rslt;
    BaseType_t result;

    // Initialize the IO pins used to control the buttons
    rslt = buttons_init_gpio();
    if (rslt != CY_RSLT_SUCCESS)
    {
        return false;
    }

    // Register the task task_buttons with the scheduler
    result = xTaskCreate(
        task_buttons, 
        "Button Task", 
        configMINIMAL_STACK_SIZE, 
        NULL, 
        tskIDLE_PRIORITY + 1, 
        NULL
    );

    if(result != pdPASS)
    {
        return false;
    }

    return true;
}
#endif