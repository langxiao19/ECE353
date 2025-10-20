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
    lcd_cmd_status_t response_status;

    while(1)
    {
        // Wait for LCD command requests
        if(xQueueReceive(xQueue_LCD, &lcd_msg, portMAX_DELAY) == pdTRUE)
        {
            response_status = LCD_CMD_STATUS_SUCCESS; // Default to success
            
            switch(lcd_msg.command)
            {
                case LCD_CMD_CLEAR_SCREEN:
                {
                    // Clear the screen with black background
                    lcd_clear_screen(LCD_COLOR_BLACK);
                    break;
                }

                case LCD_CMD_DRAW_BOARD:
                {
                    // Draw the battleship game board
                    if(!battleship_draw_game_board(lcd_msg.payload.battleship.row))
                    {
                        response_status = LCD_CMD_STATUS_ERROR;
                        printf("Error: Failed to draw game board\n\r");
                    }
                    break;
                }

                case LCD_CMD_DRAW_TILE:
                {
                    // Validate coordinates are within battleship boundaries (0-9)
                    if(lcd_msg.payload.battleship.row >= 10 || lcd_msg.payload.battleship.col >= 10)
                    {
                        response_status = LCD_CMD_STATUS_ERROR;
                        printf("Error: Invalid tile coordinates (%d, %d)\n\r", 
                               lcd_msg.payload.battleship.row, lcd_msg.payload.battleship.col);
                    }
                    else
                    {
                        // Draw a single tile/cursor
                        if(!battleship_draw_cursor(
                            lcd_msg.payload.battleship.col,
                            lcd_msg.payload.battleship.row,
                            lcd_msg.payload.battleship.border_color,
                            lcd_msg.payload.battleship.fill_color))
                        {
                            response_status = LCD_CMD_STATUS_ERROR;
                            printf("Error: Failed to draw tile at (%d, %d)\n\r",
                                   lcd_msg.payload.battleship.row, lcd_msg.payload.battleship.col);
                        }
                    }
                    break;
                }

                case LCD_CMD_DRAW_SHIP:
                {
                    // Validate ship placement within boundaries
                    uint8_t ship_length = 0;
                    switch(lcd_msg.payload.battleship.type)
                    {
                        case BATTLESHIP_TYPE_CARRIER:    ship_length = 5; break;
                        case BATTLESHIP_TYPE_BATTLESHIP: ship_length = 4; break;
                        case BATTLESHIP_TYPE_CRUISER:    ship_length = 3; break;
                        case BATTLESHIP_TYPE_SUBMARINE:  ship_length = 3; break;
                        case BATTLESHIP_TYPE_DESTROYER:  ship_length = 2; break;
                        default: ship_length = 0; break;
                    }
                    
                    // Check if ship fits within board boundaries
                    bool valid_placement = true;
                    if(lcd_msg.payload.battleship.horizontal)
                    {
                        // Horizontal placement: check if it fits within columns
                        if(lcd_msg.payload.battleship.col + ship_length > 10 || 
                           lcd_msg.payload.battleship.row >= 10)
                        {
                            valid_placement = false;
                        }
                    }
                    else
                    {
                        // Vertical placement: check if it fits within rows
                        if(lcd_msg.payload.battleship.row + ship_length > 10 || 
                           lcd_msg.payload.battleship.col >= 10)
                        {
                            valid_placement = false;
                        }
                    }
                    
                    if(!valid_placement || ship_length == 0)
                    {
                        response_status = LCD_CMD_STATUS_ERROR;
                        // Invalid ship placement detected - return error without printing
                    }
                    else
                    {
                        // Draw the ship by drawing multiple tiles
                        for(uint8_t i = 0; i < ship_length; i++)
                        {
                            uint8_t draw_row = lcd_msg.payload.battleship.row;
                            uint8_t draw_col = lcd_msg.payload.battleship.col;
                            
                            if(lcd_msg.payload.battleship.horizontal)
                            {
                                draw_col += i;
                            }
                            else
                            {
                                draw_row += i;
                            }
                            
                            // Draw only the inner part (fill) of the tile, keeping the blue border
                            if(!battleship_draw_cursor(draw_col, draw_row,
                                                     LCD_COLOR_BLUE, // Keep blue border
                                                     lcd_msg.payload.battleship.fill_color)) // Gray fill
                            {
                                response_status = LCD_CMD_STATUS_ERROR;
                                printf("Error: Failed to draw ship segment at (%d, %d)\n\r", draw_row, draw_col);
                                break;
                            }
                        }
                        
                        // Print success message with ship type name
                        const char* ship_name;
                        switch(lcd_msg.payload.battleship.type)
                        {
                            case BATTLESHIP_TYPE_CARRIER:    ship_name = "carrier"; break;
                            case BATTLESHIP_TYPE_BATTLESHIP: ship_name = "battleship"; break;
                            case BATTLESHIP_TYPE_CRUISER:    ship_name = "cruiser"; break;
                            case BATTLESHIP_TYPE_SUBMARINE:  ship_name = "submarine"; break;
                            case BATTLESHIP_TYPE_DESTROYER:  ship_name = "destroyer"; break;
                            default: ship_name = "unknown"; break;
                        }
                        printf("Drew %s successfully at (%d, %d)\n\r", ship_name, 
                               lcd_msg.payload.battleship.row, lcd_msg.payload.battleship.col);
                    }
                    break;
                }

                case LCD_CONSOLE_DRAW_MESSAGE:
                {
                    // Draw text message on LCD
                    // Use y_offset to determine which line to draw on
                    uint8_t line = 0; // Default to line 0
                    if(lcd_msg.payload.console.y_offset >= 80)
                    {
                        line = 2; // Line 2 for y_offset 80 (y=80)
                    }
                    else if(lcd_msg.payload.console.y_offset >= 40)
                    {
                        line = 1; // Line 1 for y_offset 40 (y=40)
                    }
                    
                    if (!lcd_console_draw_string(&lcd_msg.payload.console, line))
                    {
                        response_status = LCD_CMD_STATUS_ERROR;
                        printf("Error: Failed to draw console message\n\r");
                    }
                    break;
                }
                
                default:
                {
                    // Unknown command
                    response_status = LCD_CMD_STATUS_ERROR;
                    printf("Error: Unknown LCD command: %d\n\r", lcd_msg.command);
                    break;
                }
            }
            
            // Send response back to requesting task if response queue is provided
            if(lcd_msg.response_queue != NULL)
            {
                xQueueSend(lcd_msg.response_queue, &response_status, pdMS_TO_TICKS(50));
            }
        }
    }
}

/* LCD Task Initialization */
bool task_lcd_init(void){

    BaseType_t result;
    // Create LCD queue with length 10 as specified in requirements
    xQueue_LCD = xQueueCreate(10, sizeof(lcd_msg_t));
    if(xQueue_LCD == NULL)
    {
        return false;
    }

    // Create LCD task with priority 2 and minimum stack size of 1024 bytes
    result = xTaskCreate(
        task_lcd,                       // Task function
        "LCD Task",                     // Task name
        1024,                          // Stack size (minimum 1024 bytes as per LCD-002)
        NULL,                           // Task parameters
        tskIDLE_PRIORITY + 2,          // Task priority (2 as per LCD-001)
        NULL                            // Task handle
    );

    if(result != pdPASS)
    {
        return false;
    }   

    return true;
}
#endif