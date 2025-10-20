/**
 * @file battleship.c
 * @author Joe Krachey (jkrachey@wisc.edu)
 * @brief 
 * @version 0.1
 * @date 2025-08-20
 * 
 * @copyright Copyright (c) 2025
 * 
 */

 #include "battleship.h"
 #include "task_lcd.h"
 #include <stdio.h>
 #include <string.h>

 #ifdef ECE353_FREERTOS

/**
 * @brief Get the coordinates of a box on the LCD screen.
 * 
 * @param coord Pointer to the lcd_coord_t structure to store the coordinates.
 * @param col The column of the box (0-9).
 * @param row The row of the box (0-9).
 * @return true if the coordinates were successfully calculated, false otherwise.
 */
bool battleship_get_box_coordinates(lcd_coord_t *coord, uint8_t col, uint8_t row)
{
    if( row < 10 && col < 10)
    {
        coord->x = BATTLE_SHIP_LEFT_MARGIN + (col * BATTLESHIP_BOX_WIDTH);
        coord->y = BATTLE_SHIP_TOP_MARGIN + (row * BATTLESHIP_BOX_HEIGHT);
        return true;
    }
    else {
        return false; // Invalid row or column
    }
}

/**
 * @brief 
 * Draw a 10x10 grid of blue Battleship rectangles.  
 * Each rectangle is 20 pixels by 20 pixels.
 * @param player_id 
 * @return true 
 * @return false 
 */
bool battleship_draw_game_board(uint8_t player_id)
{
    lcd_coord_t coord;
    
    // Draw a 10x10 grid of battleship rectangles
    for(uint8_t row = 0; row < 10; row++)
    {
        for(uint8_t col = 0; col < 10; col++)
        {
            // Get the LCD coordinates for this battleship square
            if(battleship_get_box_coordinates(&coord, col, row))
            {
                // Draw outer rectangle (20x20 blue border)
                lcd_draw_rectangle(
                    coord.x,
                    coord.y,
                    BATTLESHIP_BOX_WIDTH,
                    BATTLESHIP_BOX_HEIGHT,
                    LCD_COLOR_BLUE,
                    false
                );
                
                // Draw inner rectangle (16x16 black fill)
                lcd_draw_rectangle(
                    coord.x + BATTLESHIP_BORDER_WIDTH/2,
                    coord.y + BATTLESHIP_BORDER_WIDTH/2,
                    BATTLESHIP_BOX_WIDTH - BATTLESHIP_BORDER_WIDTH,
                    BATTLESHIP_BOX_HEIGHT - BATTLESHIP_BORDER_WIDTH,
                    LCD_COLOR_BLACK,
                    false
                );
            }
            else
            {
                return false; // Invalid coordinates
            }
        }
    }
    return true;
}

/**
 * @brief 
 * Draw a single Battleship rectangle
 * @param col 
 * @param row 
 * @param border_color 
 * @param fill_color 
 * @return true 
 * @return false 
 */
bool battleship_draw_cursor(uint8_t col, uint8_t row, uint16_t border_color, uint16_t fill_color)
{
    lcd_coord_t coord;
    
    // Get the LCD coordinates for this battleship square
    if(battleship_get_box_coordinates(&coord, col, row))
    {
        // Draw outer rectangle (20x20 with specified border color)
        lcd_draw_rectangle(
            coord.x,
            coord.y,
            BATTLESHIP_BOX_WIDTH,
            BATTLESHIP_BOX_HEIGHT,
            border_color,
            false
        );
        
        // Draw inner rectangle (16x16 with specified fill color)
        lcd_draw_rectangle(
            coord.x + BATTLESHIP_BORDER_WIDTH/2,
            coord.y + BATTLESHIP_BORDER_WIDTH/2,
            BATTLESHIP_BOX_WIDTH - BATTLESHIP_BORDER_WIDTH,
            BATTLESHIP_BOX_HEIGHT - BATTLESHIP_BORDER_WIDTH,
            fill_color,
            false
        );
        
        return true;
    }
    else
    {
        return false; // Invalid coordinates
    }
}

/**
 * @brief 
 *  Restores a box border to the color of the game board 
 * @param col 
 * @param row 
 * @param player_id 
 * @return true 
 * @return false 
 */
bool battleship_clear_cursor(uint8_t col, uint8_t row, uint8_t player_id)
{
    return battleship_draw_cursor(col, row, LCD_COLOR_BLUE, LCD_COLOR_BLACK);
}

/**
 * @brief Send a clear screen command to the LCD gatekeeper task
 * @param request_queue The queue to send the request to
 * @param response_queue The queue to receive the response from
 * @return true if command was sent successfully, false otherwise
 */
bool battleship_send_clear_screen(QueueHandle_t request_queue, QueueHandle_t response_queue)
{
    lcd_msg_t lcd_msg;
    lcd_cmd_status_t lcd_msg_response;
    
    // Initialize the LCD message structure
    lcd_msg.command = LCD_CMD_CLEAR_SCREEN;
    lcd_msg.response_queue = response_queue;
    
    // Send the message to the LCD task
    xQueueSend(request_queue, &lcd_msg, portMAX_DELAY);
    
    // Wait for a response from the LCD task
    if(xQueueReceive(response_queue, &lcd_msg_response, pdMS_TO_TICKS(50)) != pdTRUE)
    {
        // Response not received in the specified time
        return false;
    }
    
    return (lcd_msg_response == LCD_CMD_STATUS_SUCCESS);
}

/**
 * @brief Send a draw board command to the LCD gatekeeper task
 * @param request_queue The queue to send the request to
 * @param response_queue The queue to receive the response from
 * @return true if command was sent successfully, false otherwise
 */
bool battleship_send_draw_board(QueueHandle_t request_queue, QueueHandle_t response_queue)
{
    lcd_msg_t lcd_msg;
    lcd_cmd_status_t lcd_msg_response;
    
    // Initialize the LCD message structure
    lcd_msg.command = LCD_CMD_DRAW_BOARD;
    lcd_msg.response_queue = response_queue;
    lcd_msg.payload.battleship.row = 0; // Player ID (not used in current implementation)
    
    // Send the message to the LCD task
    xQueueSend(request_queue, &lcd_msg, portMAX_DELAY);
    
    // Wait for a response from the LCD task
    if(xQueueReceive(response_queue, &lcd_msg_response, pdMS_TO_TICKS(50)) != pdTRUE)
    {
        // Response not received in the specified time
        return false;
    }
    
    return (lcd_msg_response == LCD_CMD_STATUS_SUCCESS);
}

/**
 * @brief Send a draw tile command to the LCD gatekeeper task
 * @param request_queue The queue to send the request to
 * @param response_queue The queue to receive the response from
 * @param row The row of the tile (0-9)
 * @param col The column of the tile (0-9)
 * @param border_color The border color of the tile
 * @param fill_color The fill color of the tile
 * @return true if command was sent successfully, false otherwise
 */
bool battleship_send_draw_tile(QueueHandle_t request_queue, QueueHandle_t response_queue, 
                              uint8_t row, uint8_t col, uint16_t border_color, uint16_t fill_color)
{
    lcd_msg_t lcd_msg;
    lcd_cmd_status_t lcd_msg_response;
    
    // Initialize the LCD message structure
    lcd_msg.command = LCD_CMD_DRAW_TILE;
    lcd_msg.response_queue = response_queue;
    lcd_msg.payload.battleship.row = row;
    lcd_msg.payload.battleship.col = col;
    lcd_msg.payload.battleship.border_color = border_color;
    lcd_msg.payload.battleship.fill_color = fill_color;
    
    // Send the message to the LCD task
    xQueueSend(request_queue, &lcd_msg, portMAX_DELAY);
    
    // Wait for a response from the LCD task
    if(xQueueReceive(response_queue, &lcd_msg_response, pdMS_TO_TICKS(50)) != pdTRUE)
    {
        // Response not received in the specified time
        return false;
    }
    
    return (lcd_msg_response == LCD_CMD_STATUS_SUCCESS);
}

/**
 * @brief Send a draw ship command to the LCD gatekeeper task
 * @param request_queue The queue to send the request to
 * @param response_queue The queue to receive the response from
 * @param row The starting row of the ship (0-9)
 * @param col The starting column of the ship (0-9)
 * @param type The type of ship to draw
 * @param horizontal True for horizontal orientation, false for vertical
 * @return true if command was sent successfully, false otherwise
 */
bool battleship_send_draw_ship(QueueHandle_t request_queue, QueueHandle_t response_queue,
                              uint8_t row, uint8_t col, battleship_type_t type, bool horizontal)
{
    lcd_msg_t lcd_msg;
    lcd_cmd_status_t lcd_msg_response;
    
    // Initialize the LCD message structure
    lcd_msg.command = LCD_CMD_DRAW_SHIP;
    lcd_msg.response_queue = response_queue;
    lcd_msg.payload.battleship.row = row;
    lcd_msg.payload.battleship.col = col;
    lcd_msg.payload.battleship.type = type;
    lcd_msg.payload.battleship.horizontal = horizontal;
    lcd_msg.payload.battleship.border_color = LCD_COLOR_BLUE; // Keep blue border
    lcd_msg.payload.battleship.fill_color = LCD_COLOR_GRAY;   // Ships are gray
    
    // Send the message to the LCD task
    xQueueSend(request_queue, &lcd_msg, portMAX_DELAY);
    
    // Wait for a response from the LCD task
    if(xQueueReceive(response_queue, &lcd_msg_response, pdMS_TO_TICKS(50)) != pdTRUE)
    {
        // Response not received in the specified time
        return false;
    }
    
    return (lcd_msg_response == LCD_CMD_STATUS_SUCCESS);
}

/**
 * @brief Send a draw statistics command to the LCD gatekeeper task
 * @param request_queue The queue to send the request to
 * @param response_queue The queue to receive the response from
 * @param hits Number of hits to display
 * @param misses Number of misses to display
 * @return true if command was sent successfully, false otherwise
 */
bool battleship_send_draw_stats(QueueHandle_t request_queue, QueueHandle_t response_queue,
                               uint8_t hits, uint8_t misses)
{
    lcd_msg_t lcd_msg;
    lcd_cmd_status_t lcd_msg_response;
    
    // Create hits message
    char *hits_msg = pvPortMalloc(20);
    if(hits_msg == NULL)
    {
        return false;
    }
    sprintf(hits_msg, "Hits: %d", hits);
    
    // Draw hits message - position to the right of the game board
    // Game board ends at x = 10 + (10 * 20) = 210, so start text at x = 220
    lcd_msg.command = LCD_CONSOLE_DRAW_MESSAGE;
    lcd_msg.response_queue = response_queue;
    lcd_msg.payload.console.x_offset = 220; // Position to the right of the board
    lcd_msg.payload.console.y_offset = 40; // Line 1 starts at y=40
    lcd_msg.payload.console.message = hits_msg;
    lcd_msg.payload.console.length = strlen(hits_msg);
    
    // Send hits message to the LCD task
    xQueueSend(request_queue, &lcd_msg, portMAX_DELAY);
    
    // Wait for response
    if(xQueueReceive(response_queue, &lcd_msg_response, pdMS_TO_TICKS(50)) != pdTRUE)
    {
        return false;
    }
    
    if(lcd_msg_response != LCD_CMD_STATUS_SUCCESS)
    {
        return false;
    }
    
    // Create misses message
    char *misses_msg = pvPortMalloc(20);
    if(misses_msg == NULL)
    {
        return false;
    }
    sprintf(misses_msg, "Miss: %d", misses);
    
    // Draw misses message - position to the right of the game board  
    lcd_msg.payload.console.x_offset = 220; // Position to the right of the board
    lcd_msg.payload.console.y_offset = 80; // Line 2 starts at y=80
    lcd_msg.payload.console.message = misses_msg;
    lcd_msg.payload.console.length = strlen(misses_msg);
    
    // Send misses message to the LCD task
    xQueueSend(request_queue, &lcd_msg, portMAX_DELAY);
    
    // Wait for response
    if(xQueueReceive(response_queue, &lcd_msg_response, pdMS_TO_TICKS(50)) != pdTRUE)
    {
        return false;
    }
    
    return (lcd_msg_response == LCD_CMD_STATUS_SUCCESS);
}
#endif