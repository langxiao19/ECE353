/**
 * @file ice08.c
 * @author Joe Krachey (jkrachey@wisc.edu)
 * @brief 
 * @version 0.1
 * @date 2025-06-30
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "main.h"

#if defined(ICE08)
#include "drivers.h"        
#include "rtos_events.h"
#include "task_buttons.h"
#include "task_lcd.h"
#include "task_joystick.h"

char APP_DESCRIPTION[] = "ECE353: ICE 08 - FreeRTOS LCD Gatekeeper";

/*****************************************************************************/
/* Macros                                                                    */
/*****************************************************************************/

/*****************************************************************************/
/* Global Variables                                                          */
/*****************************************************************************/

/*****************************************************************************/
/* Function Declarations                                                     */
/*****************************************************************************/

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/
void task_system_control(void *pvParameters)
{
    (void)pvParameters; // Unused parameter
    
    lcd_msg_t lcd_msg;
    static uint8_t cursor_col = 0;
    static uint8_t cursor_row = 0;
    
    // Clear the LCD screen first
    lcd_msg.command = LCD_CMD_CLEAR_SCREEN;
    lcd_msg.response_queue = NULL; // No response needed
    xQueueSend(xQueue_LCD, &lcd_msg, portMAX_DELAY);
    
    // Draw the game board
    lcd_msg.command = LCD_CMD_DRAW_BOARD;
    lcd_msg.response_queue = NULL; // No response needed
    lcd_msg.payload.battleship.row = 0; // Player ID (not used in current implementation)
    xQueueSend(xQueue_LCD, &lcd_msg, portMAX_DELAY);

    while(1)
    {
        // Draw cursor at current position
        lcd_msg.command = LCD_CMD_DRAW_CURSOR;
        lcd_msg.response_queue = NULL; // No response needed
        lcd_msg.payload.battleship.col = cursor_col;
        lcd_msg.payload.battleship.row = cursor_row;
        lcd_msg.payload.battleship.border_color = BATTLESHIP_CURSOR_COLOR;
        lcd_msg.payload.battleship.fill_color = LCD_COLOR_BLACK;
        xQueueSend(xQueue_LCD, &lcd_msg, portMAX_DELAY);
        
        // Sleep for 100 ms
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // Clear the cursor by redrawing with normal colors
        lcd_msg.command = LCD_CMD_DRAW_CURSOR;
        lcd_msg.response_queue = NULL; // No response needed
        lcd_msg.payload.battleship.col = cursor_col;
        lcd_msg.payload.battleship.row = cursor_row;
        lcd_msg.payload.battleship.border_color = LCD_COLOR_BLUE;
        lcd_msg.payload.battleship.fill_color = LCD_COLOR_BLACK;
        xQueueSend(xQueue_LCD, &lcd_msg, portMAX_DELAY);
        
        // Move cursor to next position
        cursor_col++;
        if (cursor_col >= 10) // Wrap to next row
        {
            cursor_col = 0;
            cursor_row++;
            if (cursor_row >= 10) // Wrap to beginning
            {
                cursor_row = 0;
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

    /* Initialize LCD resources */
    if (!task_lcd_init())
    {
        printf("Failed to initialize joystick task\n\r");
        for(int i = 0; i < 100000; i++) {}
       CY_ASSERT(0); // If the task initialization fails, assert
    }

    /* Start the buttons task*/
    xTaskCreate(
        task_buttons, 
        "Task Buttons", 
        configMINIMAL_STACK_SIZE, 
        NULL, 
        tskIDLE_PRIORITY + 1, 
        NULL
    );

    xTaskCreate(
        task_system_control, 
        "Task System Control", 
        configMINIMAL_STACK_SIZE*5, 
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
