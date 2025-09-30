/**
 * @file ex03.c
 * @author Joe Krachey (jkrachey@wisc.edu)
 * @brief 
 * @version 0.1
 * @date 2025-06-30
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "main.h"

#if defined(ICE05)
#include "drivers.h"

char APP_DESCRIPTION[] = "ECE353: ICE 05 - FreeRTOS Event Groups";

/*****************************************************************************/
/* Macros                                                                    */
/*****************************************************************************/
// ADD CODE for Event Group Bit Definitions
#define ICE05_EVENT_SW1_PRESSED    (1 << 0)    /* Bit 0 - SW1 button pressed */
#define ICE05_EVENT_SW2_PRESSED    (1 << 1)    /* Bit 1 - SW2 button pressed */

/*****************************************************************************/
/* Global Variables                                                          */
/*****************************************************************************/
// ADD CODE for Event Group Handle
EventGroupHandle_t ICE05_EventGroup;

/*****************************************************************************/
/* Function Declarations                                                     */
/*****************************************************************************/
void task_button_sw1(void *arg);
void task_button_sw2(void *arg);
void task_buzzer_ice05(void *arg);

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/
void task_button_sw1(void *arg)
{
    (void)arg; // Unused parameter

    bool button_pressed_last = false;
    uint8_t debounce_count = 0;

    while (1)
    {
        // ADD CODE to detect button SW1 presses
        bool button_pressed_now = ((PORT_BUTTON_SW1->IN & MASK_BUTTON_PIN_SW1) == 0);
        
        if (button_pressed_now && !button_pressed_last) {
            // Potential falling edge detected, start debouncing
            debounce_count = 1;
        } else if (button_pressed_now && debounce_count > 0) {
            // Continue debouncing
            debounce_count++;
            if (debounce_count >= 2) {
                // 30ms debounce complete (2 * 15ms), set event
                xEventGroupSetBits(ICE05_EventGroup, ICE05_EVENT_SW1_PRESSED);
                printf("SW1 pressed - Buzzer ON event set\n\r");
                debounce_count = 0;
            }
        } else {
            // Button released or no press, reset debounce
            debounce_count = 0;
        }
        
        button_pressed_last = button_pressed_now;
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}

void task_button_sw2(void *arg)
{
    (void)arg; // Unused parameter

    bool button_pressed_last = false;
    uint8_t debounce_count = 0;

    while (1)
    {
        // ADD CODE to detect button SW2 presses
        bool button_pressed_now = ((PORT_BUTTON_SW2->IN & MASK_BUTTON_PIN_SW2) == 0);
        
        if (button_pressed_now && !button_pressed_last) {
            // Potential falling edge detected, start debouncing
            debounce_count = 1;
        } else if (button_pressed_now && debounce_count > 0) {
            // Continue debouncing
            debounce_count++;
            if (debounce_count >= 2) {
                // 30ms debounce complete (2 * 15ms), set event
                xEventGroupSetBits(ICE05_EventGroup, ICE05_EVENT_SW2_PRESSED);
                printf("SW2 pressed - Buzzer OFF event set\n\r");
                debounce_count = 0;
            }
        } else {
            // Button released or no press, reset debounce
            debounce_count = 0;
        }
        
        button_pressed_last = button_pressed_now;
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}

void task_buzzer_ice05(void *arg)
{
    (void)arg; // Unused parameter
    
    EventBits_t events;
    bool buzzer_state = false;  // Track current buzzer state
    
    while (1)
    {
        // ADD CODE to handle buzzer events
        events = xEventGroupWaitBits(
            ICE05_EventGroup,                                    // Event group handle
            ICE05_EVENT_SW1_PRESSED | ICE05_EVENT_SW2_PRESSED,   // Bits to wait for
            pdTRUE,                                              // Clear bits on exit
            pdFALSE,                                             // Wait for ANY bit (OR operation)
            portMAX_DELAY                                        // Wait indefinitely
        );
        
        // Check which button was pressed and control buzzer accordingly
        if (events & ICE05_EVENT_SW1_PRESSED) {
            // SW1 pressed - Turn buzzer ON
            if (!buzzer_state) {
                buzzer_on();
                buzzer_state = true;
                printf("Buzzer turned ON\n\r");
            }
        }
        
        if (events & ICE05_EVENT_SW2_PRESSED) {
            // SW2 pressed - Turn buzzer OFF
            if (buzzer_state) {
                buzzer_off();
                buzzer_state = false;
                printf("Buzzer turned OFF\n\r");
            }
        }
    }
}

/**
 * @brief
 * This function will initialize all of the hardware resources for
 * the ICE
 */
void app_init_hw(void)
{
    cy_rslt_t rslt;

    console_init();
    printf("**************************************************\n\r");
    printf("* %s\n\r", APP_DESCRIPTION);
    printf("* Date: %s\n\r", __DATE__);
    printf("* Time: %s\n\r", __TIME__);
    printf("* Name:%s\n\r", NAME);
    printf("**************************************************\n\r");

    /* ADD CODE Initialize the buttons */
    buttons_init_gpio();

    /* ADD CODE Initialize the buzzer */
    rslt = buzzer_init(0.8, 2000);
    if (rslt != CY_RSLT_SUCCESS) {
        printf("Buzzer initialization failed\n\r");
    }
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
    /* ADD CODE Create the event group */
    ICE05_EventGroup = xEventGroupCreate();
    if (ICE05_EventGroup == NULL) {
        printf("Failed to create event group\n\r");
        return;
    }

    /* ADD CODE Register the tasks with FreeRTOS*/
    xTaskCreate(
        task_button_sw1,
        "SW1 Task",
        configMINIMAL_STACK_SIZE,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL
    );

    xTaskCreate(
        task_button_sw2,
        "SW2 Task",
        configMINIMAL_STACK_SIZE,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL
    );

    xTaskCreate(
        task_buzzer_ice05,
        "Buzzer Task",
        configMINIMAL_STACK_SIZE,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL
    );
    
    /* ADD CODE Start the scheduler*/
    vTaskStartScheduler();

    /* Will never reach this loop once the scheduler starts */
    while (1)
    {
    }
}
#endif