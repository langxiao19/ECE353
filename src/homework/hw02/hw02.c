 /**
 * @file hw02.c
 * @author Joe Krachey (jkrachey@wisc.edu)
 * @brief 
 * @version 0.1
 * @date 2025-10-08
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "hw02.h"
#include "battleship.h"
#include "main.h"
#include "projdefs.h"
#include "task_lcd.h"
#include <stdbool.h>

#if defined(HW02)

char APP_DESCRIPTION[] = "ECE353 F25 HW02 -- LCD Gatekeeper";

/*****************************************************************************/
/* Global Variables                                                          */
/*****************************************************************************/
QueueHandle_t xQueue_LCD_response;

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/
void task_hw02_system_control(void *pvParameters)
{
    (void)pvParameters; // Unused parameter

    // Clear the screen

    // Draw the game board

    // Test all tiles

    // Draw valid ships

    // Test invalid ship placements
    
    // Print the hits/misses to the LCD

    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


/*************************************************
 * @brief
 * This function will initialize all of the hardware resources for
 * the ICE
 ************************************************/
void app_init_hw(void)
{
    cy_rslt_t rslt;

    console_init();
    // Set text color to black
    printf("\x1b[30m");
    printf("\x1b[2J\x1b[;H");
    printf("**************************************************\n\r");
    printf("* %s\n\r", APP_DESCRIPTION);
    printf("* Date: %s\n\r", __DATE__);
    printf("* Time: %s\n\r", __TIME__);
    printf("* Name:%s\n\r", NAME);
    printf("**************************************************\n\r");

    rslt = lcd_initialize();
    if (rslt != CY_RSLT_SUCCESS)
    {
        printf("LCD initialization failed!\n\r");
        for(int i = 0; i < 100000; i++) {}
        CY_ASSERT(0);
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
/* Register the tasks with FreeRTOS*/

    ECE353_RTOS_Events = xEventGroupCreate();

    /* Initialize the response queue */
    xQueue_LCD_response = xQueueCreate(1, sizeof(lcd_cmd_status_t));
    if (xQueue_LCD_response == NULL)
    {
        printf("Failed to create LCD response queue\n\r");
        for(int i = 0; i < 100000; i++) {}
        CY_ASSERT(0);
    }

    /* Initialize LCD resources */
    if (!task_lcd_init())
    {
        printf("Failed to initialize joystick task\n\r");
        for(int i = 0; i < 100000; i++) {}
       CY_ASSERT(0); // If the task initialization fails, assert
    }

    xTaskCreate(
        task_hw02_system_control, 
        "Task System Control", 
        configMINIMAL_STACK_SIZE*10, 
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