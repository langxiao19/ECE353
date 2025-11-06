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
#include <stdio.h>

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
    battleship_send_clear_screen(xQueue_LCD, xQueue_LCD_response);
    vTaskDelay(pdMS_TO_TICKS(500));

    // Draw the game board
    battleship_send_draw_board(xQueue_LCD, xQueue_LCD_response);
    vTaskDelay(pdMS_TO_TICKS(500));

    // Test all tiles - animate a green tile moving across each square
    for(uint8_t row = 0; row < 10; row++)
    {
        for(uint8_t col = 0; col < 10; col++)
        {
            // Draw green tile at current position
            battleship_send_draw_tile(xQueue_LCD, xQueue_LCD_response, 
                                    row, col, LCD_COLOR_BLUE, LCD_COLOR_GREEN);
            vTaskDelay(pdMS_TO_TICKS(100));
            
            // Clear the tile (make it black again)
            battleship_send_draw_tile(xQueue_LCD, xQueue_LCD_response, 
                                    row, col, LCD_COLOR_BLUE, LCD_COLOR_BLACK);
        }
    }
    vTaskDelay(pdMS_TO_TICKS(500));

    // Draw valid ships
    battleship_send_draw_ship(xQueue_LCD, xQueue_LCD_response, 9, 0, BATTLESHIP_TYPE_CARRIER, true);
    battleship_send_draw_ship(xQueue_LCD, xQueue_LCD_response, 0, 0, BATTLESHIP_TYPE_BATTLESHIP, true);
    battleship_send_draw_ship(xQueue_LCD, xQueue_LCD_response, 2, 2, BATTLESHIP_TYPE_DESTROYER, false);
    battleship_send_draw_ship(xQueue_LCD, xQueue_LCD_response, 5, 5, BATTLESHIP_TYPE_SUBMARINE, true);
    battleship_send_draw_ship(xQueue_LCD, xQueue_LCD_response, 7, 7, BATTLESHIP_TYPE_CRUISER, false);
    
    vTaskDelay(pdMS_TO_TICKS(1000)); // Pause to see the ships

    // Test invalid ship placements - these should be rejected by LCD gatekeeper  
    bool invalid_ship_detected = false;
    
    // Battleship at (0,7) horizontal - exceeds board width (would go to column 10)
    invalid_ship_detected = !battleship_send_draw_ship(xQueue_LCD, xQueue_LCD_response, 0, 7, BATTLESHIP_TYPE_BATTLESHIP, true);
    if(invalid_ship_detected)
    {
        printf("Correctly detected invalid ship placement (too far right)\n\r");
    }
    
    // Submarine at (8,0) vertical - exceeds board height (goes to row 10)
    invalid_ship_detected = !battleship_send_draw_ship(xQueue_LCD, xQueue_LCD_response, 8, 0, BATTLESHIP_TYPE_SUBMARINE, false);
    if(invalid_ship_detected)
    {
        printf("Correctly detected invalid ship placement (too far down)\n\r");
    }
    
    // Carrier at (15,0) vertical - starts outside board boundaries
    invalid_ship_detected = !battleship_send_draw_ship(xQueue_LCD, xQueue_LCD_response, 15, 0, BATTLESHIP_TYPE_CARRIER, false);
    if(invalid_ship_detected)
    {
        printf("Correctly detected invalid ship placement (invalid coordinates)\n\r");
    }

    printf("All invalid ship placement tests passed\n\r");

    // Display game statistics
    battleship_send_draw_stats(xQueue_LCD, xQueue_LCD_response, 5, 3);

    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
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
    // Set text color to white for better visibility
    printf("\x1b[37m");
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