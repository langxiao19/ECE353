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

#if defined(EX05)

#include "drivers.h"

char APP_DESCRIPTION[] = "ECE353: Example 05 - FreeRTOS Tasks";

/*****************************************************************************/
/* Macros                                                                    */
/*****************************************************************************/

/*****************************************************************************/
/* Global Variables                                                          */
/*****************************************************************************/
volatile bool buzzer_enabled = false;
/*****************************************************************************/
/* Function Declarations                                                     */
/*****************************************************************************/
void task_button_sw1(void *arg);
void task_button_sw2(void *arg);
void task_buzzer_ex05(void *arg);

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/
void task_button_sw1(void *arg)
{   (void)arg; // unused parameter
    uint32_t button_count = 0;
    bool button_pressed_last = false;
    printf("Task SW1 created\n\r");
    while(1)
    {
        bool button_pressed_now = ((PORT_BUTTON_SW1 -> IN & MASK_BUTTON_PIN_SW1) == 0);
        
        if (button_pressed_now && !button_pressed_last) {
            // Button just pressed (rising edge detection)
            printf("Button SW1 Pressed -- Enabled Buzzer\n\r");
            buzzer_enabled = true;
        }
        
        button_pressed_last = button_pressed_now;
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}

void task_button_sw2(void *arg)
{
    (void)arg; // unused parameter
    printf("Task SW2 created\n\r");
    bool button_pressed_last = false;
    while (1)
    {
        bool button_pressed_now = ((PORT_BUTTON_SW2 -> IN & MASK_BUTTON_PIN_SW2) == 0);
        
        if (button_pressed_now && !button_pressed_last) {
            // Button just pressed (rising edge detection)
            printf("Button SW2 Pressed -- Disabled Buzzer\n\r");
            buzzer_enabled = false;
        }
        
        button_pressed_last = button_pressed_now;
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}

void task_buzzer_ex05(void *arg)
{
    (void)arg; // unused parameter
    printf("Task Buzzer created\n\r");
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
        if (buzzer_enabled){
            printf("Buzzer enabled\n\r");
            buzzer_on();
        }
        else{
            printf("Buzzer disabled\n\r");
            buzzer_off();
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
    console_init();
    printf("**************************************************\n\r");
    printf("* %s\n\r", APP_DESCRIPTION);
    printf("* Date: %s\n\r", __DATE__);
    printf("* Time: %s\n\r", __TIME__);
    printf("* Name:%s\n\r", NAME);
    printf("**************************************************\n\r");

    /* Initialize the buttons */
    buttons_init_gpio();
    /* Initialize the buzzer */
    buzzer_init(0.8, 2000);

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
    /* Register the tasks with FreeRTOS*/
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
        task_buzzer_ex05,
        "Buzzer Task",
        configMINIMAL_STACK_SIZE,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL
    );
    
    /* Start the scheduler*/
    vTaskStartScheduler();

    /* Will never reach this loop once the scheduler starts */
    while (1)
    {
    }
}
#endif