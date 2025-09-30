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

    // Previous button states for edge detection
    bool sw1_prev = true;  // Buttons are active low, so start with true (not pressed)
    bool sw2_prev = true;
    bool sw3_prev = true;
    
    // Debounce counters (need 2 consecutive readings for 30ms debounce)
    uint8_t sw1_debounce = 0;
    uint8_t sw2_debounce = 0;
    uint8_t sw3_debounce = 0;

    while (1)
    {
        // Monitor button SW1
        bool sw1_current = ((PORT_BUTTON_SW1->IN & MASK_BUTTON_PIN_SW1) != 0);
        
        if (!sw1_current && sw1_prev) {
            // Potential falling edge detected, start debouncing
            sw1_debounce = 1;
        } else if (!sw1_current && sw1_debounce > 0) {
            // Continue debouncing
            sw1_debounce++;
            if (sw1_debounce >= 2) {
                // 30ms debounce complete (2 * 15ms), set event
                xEventGroupSetBits(ECE353_RTOS_Events, ECE353_EVENT_SW1_PRESSED);
                sw1_debounce = 0;
            }
        } else {
            // Button released or no press, reset debounce
            sw1_debounce = 0;
        }
        sw1_prev = sw1_current;

        // Monitor button SW2
        bool sw2_current = ((PORT_BUTTON_SW2->IN & MASK_BUTTON_PIN_SW2) != 0);
        
        if (!sw2_current && sw2_prev) {
            // Potential falling edge detected, start debouncing
            sw2_debounce = 1;
        } else if (!sw2_current && sw2_debounce > 0) {
            // Continue debouncing
            sw2_debounce++;
            if (sw2_debounce >= 2) {
                // 30ms debounce complete (2 * 15ms), set event
                xEventGroupSetBits(ECE353_RTOS_Events, ECE353_EVENT_SW2_PRESSED);
                sw2_debounce = 0;
            }
        } else {
            // Button released or no press, reset debounce
            sw2_debounce = 0;
        }
        sw2_prev = sw2_current;

        // Monitor button SW3
        bool sw3_current = ((PORT_BUTTON_SW3->IN & MASK_BUTTON_PIN_SW3) != 0);
        
        if (!sw3_current && sw3_prev) {
            // Potential falling edge detected, start debouncing
            sw3_debounce = 1;
        } else if (!sw3_current && sw3_debounce > 0) {
            // Continue debouncing
            sw3_debounce++;
            if (sw3_debounce >= 2) {
                // 30ms debounce complete (2 * 15ms), set event
                xEventGroupSetBits(ECE353_RTOS_Events, ECE353_EVENT_SW3_PRESSED);
                sw3_debounce = 0;
            }
        } else {
            // Button released or no press, reset debounce
            sw3_debounce = 0;
        }
        sw3_prev = sw3_current;

        // Debounce delay - sample every 15ms
        vTaskDelay(pdMS_TO_TICKS(15));
    }
 }

 /* Button Task Initialization */
bool task_button_init(void){

    BaseType_t result;

    // Create the button task
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