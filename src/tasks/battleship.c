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
 * Used to draw an empty game board for the specified player.
 * @param player_id 
 * @return true 
 * @return false 
 */
bool battleship_draw_game_board(uint8_t player_id)
{
    lcd_coord_t box_coords;
    for (uint8_t row = 0; row < 10; row++)
    {
        for (uint8_t col = 0; col < 10; col++)
        {
            if (!battleship_get_box_coordinates(&box_coords, col, row))
            {
                return false; // Failed to get box coordinates
            }
            
            lcd_draw_rectangle(
                box_coords.x,
                box_coords.y,
                BATTLESHIP_BOX_WIDTH,
                BATTLESHIP_BOX_HEIGHT,
                LCD_COLOR_BLUE,
                false
            );
            lcd_draw_rectangle(
                box_coords.x + BATTLESHIP_BORDER_WIDTH/2,
                box_coords.y + BATTLESHIP_BORDER_WIDTH/2,
                BATTLESHIP_BOX_WIDTH - BATTLESHIP_BORDER_WIDTH,
                BATTLESHIP_BOX_HEIGHT - BATTLESHIP_BORDER_WIDTH,
                LCD_COLOR_BLACK,
                false
            );
        }

    }
    return true;
}

/**
 * @brief 
 * Draws the cursor for the currently active location by changing
 * the color of the box border.
 * @param col 
 * @param row 
 * @return true 
 * @return false 
 */

 /**
 * @brief
 *  Draw a single Battleship rectangle
 * @return true
 * @return false
 */

bool battleship_draw_cursor(uint8_t col,
    uint8_t row,
    uint16_t border_color,
    uint16_t fill_color
)
{
    lcd_coord_t coord;
    if(battleship_get_box_coordinates(&coord, col, row))
    {
        // Outer rectangle
        lcd_draw_rectangle(coord.x, coord.y, BATTLESHIP_BOX_WIDTH, BATTLESHIP_BOX_HEIGHT, border_color, false);

        // Inner rectangle
        lcd_draw_rectangle(coord.x + BATTLESHIP_BORDER_WIDTH/2, coord.y + BATTLESHIP_BORDER_WIDTH/2, BATTLESHIP_BOX_WIDTH - BATTLESHIP_BORDER_WIDTH, BATTLESHIP_BOX_HEIGHT - BATTLESHIP_BORDER_WIDTH, fill_color, false);
        return true;
    }
    return false;
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

/**
 * @brief Draws a ship at the specified location and orientation.
 * Returns true on success, false if placement is invalid (out of bounds)
 */
bool battleship_draw_ship(uint8_t col, uint8_t row, battleship_type_t type, bool horizontal, uint16_t border_color, uint16_t fill_color)
{
    uint8_t length = 0;
    switch(type)
    {
        case BATTLESHIP_TYPE_CARRIER: length = 5; break;
        case BATTLESHIP_TYPE_BATTLESHIP: length = 4; break;
        case BATTLESHIP_TYPE_CRUISER: length = 3; break;
        case BATTLESHIP_TYPE_SUBMARINE: length = 3; break;
        case BATTLESHIP_TYPE_DESTROYER: length = 2; break;
        default: return false;
    }

    // Check bounds
    bool bounds_ok = true;
    if (horizontal)
    {
        if (col + length > 10) bounds_ok = false;
    }
    else
    {
        if (row + length > 10) bounds_ok = false;
    }

    if (!bounds_ok) return false;

    // Draw each box of the ship
    for (uint8_t i = 0; i < length; i++)
    {
        uint8_t c = col + (horizontal ? i : 0);
        uint8_t r = row + (horizontal ? 0 : i);
        lcd_coord_t coord;
        if (!battleship_get_box_coordinates(&coord, c, r)) return false;

        // Outer rectangle (border)
        lcd_draw_rectangle(coord.x, coord.y, BATTLESHIP_BOX_WIDTH, BATTLESHIP_BOX_HEIGHT, border_color, false);
        // Inner rectangle (fill)
        lcd_draw_rectangle(coord.x + BATTLESHIP_BORDER_WIDTH/2, coord.y + BATTLESHIP_BORDER_WIDTH/2,
                          BATTLESHIP_BOX_WIDTH - BATTLESHIP_BORDER_WIDTH, BATTLESHIP_BOX_HEIGHT - BATTLESHIP_BORDER_WIDTH,
                          fill_color, false);
    }

    return true;
}

#endif
