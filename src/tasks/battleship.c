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
    return false;
}
#endif