/**
 * @file task_lcd.c
 * @author Joe Krachey (jkrachey@wisc.edu)
 * @brief 
 * @version 0.1
 * @date 2025-08-18
 * 
 * @copyright Copyright (c) 2025
 * 
 */

 #include "task_lcd.h"

#ifdef ECE353_FREERTOS  
/* FreeRTOS Queue for LCD messages */
QueueHandle_t xQueue_LCD;

/* LCD Task */
void task_lcd(void *pvParameters)
{
    (void)pvParameters; // Unused parameter

    lcd_msg_t lcd_msg;

    while(1)
    {
        xQueueReceive(xQueue_LCD, &lcd_msg, portMAX_DELAY);

        switch(lcd_msg.command)
        {
            case LCD_CMD_CLEAR_SCREEN:
            {
                lcd_clear_screen(LCD_COLOR_BLACK);
                break;
            }

            case LCD_CMD_DRAW_BOARD:
            {
                battleship_draw_game_board(lcd_msg.payload.battleship.row);
                break;
            }

            case LCD_CMD_DRAW_CURSOR:
            {
                battleship_draw_cursor(
                    lcd_msg.payload.battleship.col,
                    lcd_msg.payload.battleship.row,
                    lcd_msg.payload.battleship.border_color,
                    lcd_msg.payload.battleship.fill_color
                );
                break;
            }

            case LCD_CONSOLE_DRAW_MESSAGE:
            {
                if (!lcd_console_draw_string(&lcd_msg.payload.console, 1))
                {
                    printf("Failed to draw console message\n");
                }
                break;
            }
            default:
            {
                // Unknown command
                break;
            }
        }
    }
}

/* LCD Task Initialization */
bool task_lcd_init(void){

    BaseType_t result;
    xQueue_LCD = xQueueCreate(10, sizeof(lcd_msg_t));
    if(xQueue_LCD == NULL)
    {
        return false;
    }

    result= xTaskCreate(
        task_lcd,                       // Task function
        "LCD Task",                     // Task name
        5*configMINIMAL_STACK_SIZE,    // Stack size
        NULL,                           // Task parameters
        tskIDLE_PRIORITY+1,              // Task priority
        NULL                            // Task handle
    );

    if(result != pdPASS)
    {
        return false;
    }   

    return true;
}
#endif