/**
 * @file task_joystick.c
 * @author Joe Krachey (jkrachey@wisc.edu)
 * @brief 
 * @version 0.1
 * @date 2025-08-14
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "main.h"

#ifdef ECE353_FREERTOS  
#include "drivers.h"
 #include "task_joystick.h"

 QueueHandle_t Queue_Joystick = NULL;

/* Message lookup table for joystick positions */
const char * const joystick_pos_names[] = {
    "Center",
    "Left",
    "Right",
    "Up",
    "Down",
    "Upper Left",
    "Upper Right",
    "Lower Left",
    "Lower Right"
};

 /**
  * @brief 
  *  Task used to monitor the joystick
  * @param arg 
  */
 void task_joystick(void *arg)
{
    (void)arg; // Unused parameter
    joystick_position_t current_position, previous_position = JOYSTICK_POS_CENTER;
    
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
        
        // Get the current joystick position
        current_position = joystick_get_pos();
        
        // If the position has changed, send it to the queue
        if (current_position != previous_position) {
            xQueueSend(Queue_Joystick, &current_position, 0);
            previous_position = current_position;
        }
    }
}


bool task_joystick_init(void)
{
    /* Create the Queue used to send Joystick Positions*/
    Queue_Joystick = xQueueCreate(1, sizeof(joystick_position_t));
    if (Queue_Joystick == NULL) {
        return false;
    }

    /* Create the joystick task */
    BaseType_t result = xTaskCreate(
        task_joystick,           // Task function
        "Task Joystick",         // Task name
        2048,                    // Stack size
        NULL,                    // Parameters
        2,                       // Priority
        NULL                     // Task handle
    );
    
    if (result != pdPASS) {
        return false;
    }
    
    return true;
}
#endif