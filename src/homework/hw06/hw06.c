/**
 * @file hw06.c
 * @author Langlang Xiao
 * @brief 
 * @version 0.1
 * @date 2025-12-02
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "cyhal_hw_types.h"
#include "main.h"

#if defined(HW06)
#include "drivers.h"
#include "devices.h"
#include "task_console.h"
#include "task_io_expander.h"
#include "task_light_sensor.h"
#include "task_temp_sensor.h"
#include "task_eeprom.h"
#include "task_imu.h"
#include "battleship.h"
#include "lcd_console.h"
#include "task_ipc.h"
#include "task_lcd.h"

// APP_DESCRIPTION is defined in main.c

// TODO: IPC RX game control queue needs to be implemented in task_ipc
// extern QueueHandle_t Queue_IPC_Rx_Game_Control;

/*****************************************************************************/
/* Macros                                                                    */
/*****************************************************************************/

/*****************************************************************************/
/* Global Variables                                                          */
/*****************************************************************************/
QueueHandle_t xQueue_LCD_response;
cyhal_i2c_t *I2C_Obj = NULL;
cyhal_spi_t *SPI_Obj = NULL;

typedef enum {
    PLAYER_ROLE_NONE = 0,
    PLAYER_ROLE_PLAYER1,
    PLAYER_ROLE_PLAYER2
} player_role_t;

volatile player_role_t player_role = PLAYER_ROLE_NONE;

SemaphoreHandle_t Semaphore_I2C = NULL;
SemaphoreHandle_t Semaphore_SPI = NULL;

    QueueHandle_t Queue_System_Control_Responses = NULL;

/*****************************************************************************/
/* Function Declarations                                                     */
/*****************************************************************************/
void task_system_control(void *arg);
void redraw_board_for_turn(bool is_my_turn, uint8_t board_grid[10][10], uint8_t opponent_grid[10][10], 
                           uint8_t target_col, uint8_t target_row, uint16_t board_color, uint16_t bg_color);

// Game state variables for restart capability
static uint8_t board_grid[10][10];
static uint8_t opponent_grid[10][10];
static uint8_t hits;
static uint8_t misses;
static int num_placed;
static uint8_t opponent_ships_remaining;
static uint8_t my_ships_remaining;
static uint8_t led_value;/*****************************************************************************/
/* Helper Functions                                                          */
/*****************************************************************************/
/**
 * @brief Helper function to draw text using Consolas 20pt font
 * @param x X position
 * @param y Y position
 * @param text Text to draw
 * @param fg_color Foreground color
 * @param bg_color Background color
 */
void draw_text(uint16_t x, uint16_t y, const char *text, uint16_t fg_color, uint16_t bg_color)
{
    uint16_t char_x = x;
    uint16_t char_y = y;
    
    for (const char *c = text; *c != '\0'; c++) 
    {
        uint16_t char_index = *c - Consolas_20ptFontInfo.start_char;
        uint16_t bitmap_height = Consolas_20ptFontInfo.height;
        uint16_t bitmap_offset = Consolas_20ptDescriptors[char_index].offset;
        uint16_t bitmap_width = Consolas_20ptDescriptors[char_index].width;
        uint8_t *bitmap = (uint8_t *)&Consolas_20ptBitmaps[bitmap_offset];
        
        lcd_draw_image(char_x, char_y, bitmap_width, bitmap_height, 
                      bitmap, fg_color, bg_color, false);
        char_x += bitmap_width;
    }
}

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/
/**
 * @brief Redraws the game board based on whose turn it is
 */
void redraw_board_for_turn(bool is_my_turn, uint8_t board_grid[10][10], uint8_t opponent_grid[10][10], 
                           uint8_t target_col, uint8_t target_row, uint16_t board_color, uint16_t bg_color)
{
    // Clear the board area
    lcd_draw_rectangle(0, 0, 220, 240, bg_color, false);
    battleship_draw_game_board(0);
    
    if (is_my_turn) {
        // MY TURN: Show my attack history on opponent's board
        for (int r = 0; r < 10; r++) {
            for (int c = 0; c < 10; c++) {
                if (opponent_grid[r][c] == 2) {
                    // I hit this square
                    battleship_draw_cursor(c, r, board_color, LCD_COLOR_RED);
                } else if (opponent_grid[r][c] == 3) {
                    // I missed this square
                    battleship_draw_cursor(c, r, board_color, LCD_COLOR_WHITE);
                } else {
                    // Unknown square
                    battleship_draw_cursor(c, r, board_color, bg_color);
                }
            }
        }
        // Draw cursor
        battleship_draw_cursor(target_col, target_row, BATTLESHIP_CURSOR_COLOR, bg_color);
    } else {
        // OPPONENT'S TURN: Show my ships and where I've been hit/missed
        for (int r = 0; r < 10; r++) {
            for (int c = 0; c < 10; c++) {
                if (board_grid[r][c] == 2) {
                    // My ship was hit here
                    battleship_draw_cursor(c, r, board_color, LCD_COLOR_RED);
                } else if (board_grid[r][c] == 3) {
                    // Opponent missed here
                    battleship_draw_cursor(c, r, board_color, LCD_COLOR_WHITE);
                } else if (board_grid[r][c] == 1) {
                    // My ship (not hit yet)
                    battleship_draw_cursor(c, r, board_color, LCD_COLOR_GRAY);
                } else {
                    // Empty square
                    battleship_draw_cursor(c, r, board_color, bg_color);
                }
            }
        }
    }
}

/**
 * @brief 
 * This function will initialize all of the software resources for the 
 * System Control Task
 * @return true 
 * @return false 
 */
bool task_system_control_resources_init(void)
{
    /* Create the I2C Semaphore */
    Semaphore_I2C = xSemaphoreCreateMutex();
    if (Semaphore_I2C == NULL)
    {
        return false;
    }

    /* Create the SPI Semaphore */
    Semaphore_SPI = xSemaphoreCreateMutex();
    if (Semaphore_SPI == NULL)
    {
        return false;
    }

    /* Initialize IPC RX and TX resources (queues) */
    if (!task_ipc_resources_init_rx())
    {
        printf("ERROR: Failed to initialize IPC RX resources\n\r");
        return false;
    }
    
    if (!task_ipc_resources_init_tx())
    {
        printf("ERROR: Failed to initialize IPC TX resources\n\r");
        return false;
    }

    /* Create the System Control Task */
    if (xTaskCreate(
            task_system_control,
            "System Control Task",
            configMINIMAL_STACK_SIZE*5,
            NULL,
            tskIDLE_PRIORITY + 1,
            NULL) != pdPASS)
    {
        return false;
    }

    return true;
}

/**
 * @brief 
 * This function implements the behavioral requirements for HW06
 * @param arg 
 */
void task_system_control(void *arg)
{
    (void)arg; // Unused parameter
    
    vTaskDelay(pdMS_TO_TICKS(10));
    printf("System Control   : Starting System Control Task\r\n");
    
    /* Create LCD response queue for battleship tile functions */
    xQueue_LCD_response = xQueueCreate(1, sizeof(lcd_cmd_status_t));
    if (xQueue_LCD_response == NULL)
    {
        printf("System Control   : Failed to create LCD response queue\n\r");
        CY_ASSERT(0);
    }
    
    /* Small delay to ensure IO expander task is ready */
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Configure the IO Expander*/
    system_sensors_io_expander_write(NULL, IOXP_ADDR_CONFIG, 0x80); //Set P7 as input, all others as outputs
    
    /* Set the initial state of the LEDs*/
    system_sensors_io_expander_write(NULL, IOXP_ADDR_OUTPUT_PORT, 0x01); //Turn on LED0

    // Input_Control_03: Initialize light sensor color scheme variables
    uint16_t bg_color = LCD_COLOR_BLACK;
    uint16_t text_color = LCD_COLOR_WHITE;
    bool dark_mode = true; // Start in dark mode
    QueueHandle_t light_sensor_response = xQueueCreate(1, sizeof(device_response_msg_t));

    /* System_Init_01: Draw empty game board */
    lcd_clear_screen(bg_color);
    
    // Draw the main 10x10 game board on the left (player's board)
    battleship_draw_game_board(0); // Player ID doesn't matter for empty board
    
    /* System_Init_02: Display "Press SW1 to Start" */
    uint16_t text_x = 220;
    uint16_t text_y = 60;
    
    draw_text(text_x, text_y, "Press", text_color, bg_color);
    draw_text(text_x, text_y + 25, "SW1 to", text_color, bg_color);
    draw_text(text_x, text_y + 50, "Start", text_color, bg_color);

    printf("System Control   : Game board drawn - waiting for SW1\n\r");
    
    // System_Init_03: Wait for SW1 button press or incoming IPC_GAME_CONTROL_NEW_GAME
    uint32_t light_check_counter = 0;
    while(player_role == PLAYER_ROLE_NONE)
    {
        // Input_Control_03: Check light sensor to update color scheme
        if (light_check_counter % 50 == 0) {
            uint16_t light_level = 0;
            if (system_sensors_get_light(light_sensor_response, &light_level)) {
                bool should_be_dark = (light_level >= 100); // Uncovered (bright) = dark mode (black)
                if (should_be_dark != dark_mode) {
                    dark_mode = should_be_dark;
                    bg_color = dark_mode ? LCD_COLOR_BLACK : LCD_COLOR_WHITE; // dark_mode=true => black, false => white
                    text_color = dark_mode ? LCD_COLOR_WHITE : LCD_COLOR_BLACK;
                    
                    // Redraw UI with new colors
                    lcd_clear_screen(bg_color);
                    battleship_draw_game_board(0);
                    
                    // Fill all grid boxes with bg_color (use blue since player role not determined yet)
                    for (int r = 0; r < 10; r++) {
                        for (int c = 0; c < 10; c++) {
                            battleship_draw_cursor(c, r, LCD_COLOR_BLUE, bg_color);
                        }
                    }
                    
                    draw_text(text_x, text_y, "Press", text_color, bg_color);
                    draw_text(text_x, text_y + 25, "SW1 to", text_color, bg_color);
                    draw_text(text_x, text_y + 50, "Start", text_color, bg_color);
                }
            }
        }
        light_check_counter++;
        
        // Check if SW1 was pressed (become Player 1)
        if(ECE353_Events.sw1 == 1)
        {
            ECE353_Events.sw1 = 0;
            player_role = PLAYER_ROLE_PLAYER1;
            printf("System Control   : Became Player 1 (pressed SW1 first)\n\r");
            
            // System_Init_04: Send IPC_GAME_CONTROL_NEW_GAME packet
            ipc_send_game_control(IPC_GAME_CONTROL_NEW_GAME);
            printf("System Control   : Sent IPC_GAME_CONTROL_NEW_GAME\n\r");
        }
        
        // Check if we received IPC_GAME_CONTROL_NEW_GAME (other board pressed first)
        ipc_game_control_t control_msg;
        if(xQueueReceive(Queue_IPC_Rx_Game_Control, &control_msg, 0) == pdPASS)
        {
            if(control_msg == IPC_GAME_CONTROL_NEW_GAME)
            {
                player_role = PLAYER_ROLE_PLAYER2;
                printf("System Control   : Became Player 2 (other board pressed SW1 first)\n\r");
                
                // Clear the "Press SW1 to Start" text
                draw_text(text_x, text_y, "          ", bg_color, bg_color);
                draw_text(text_x, text_y + 25, "          ", bg_color, bg_color);
                draw_text(text_x, text_y + 50, "          ", bg_color, bg_color);
                
                // Update LCD to show "You are P2"
                draw_text(text_x, text_y, "You are", text_color, bg_color);
                draw_text(text_x + 30, text_y + 25, "P2", LCD_COLOR_RED, bg_color);
                vTaskDelay(pdMS_TO_TICKS(1500));
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // System_Init_05: If we're Player 2, send ACK
    if(player_role == PLAYER_ROLE_PLAYER2)
    {
        ipc_send_game_control(IPC_GAME_CONTROL_ACK);
        printf("System Control   : Became Player 2, sent ACK\\n\\r");
    }
    else if(player_role == PLAYER_ROLE_PLAYER1)
    {
        // Wait for ACK from Player 2
        printf("System Control   : Waiting for ACK from Player 2\\n\\r");
        // TODO: Replace with queue-based ACK detection when ready
        vTaskDelay(pdMS_TO_TICKS(500)); // Temporary delay for testing
    }
    
    printf("System Control   : Player roles established - Role: %d\\n\\r", player_role);
    
RESTART_GAME: // Label for game restart after game over
    
    // Player_Roles_02: Redraw game board with appropriate color
    // Player 1 = Blue, Player 2 = Red
    uint16_t board_color = (player_role == PLAYER_ROLE_PLAYER1) ? BATTLESHIP_PLAYER_0_COLOR : BATTLESHIP_PLAYER_1_COLOR;
    
    lcd_clear_screen(bg_color);
    
    // Redraw the game board with the player's color
    lcd_coord_t box_coords;
    for (uint8_t row = 0; row < 10; row++)
    {
        for (uint8_t col = 0; col < 10; col++)
        {
            box_coords.x = BATTLE_SHIP_LEFT_MARGIN + (col * BATTLESHIP_BOX_WIDTH);
            box_coords.y = BATTLE_SHIP_TOP_MARGIN + (row * BATTLESHIP_BOX_HEIGHT);
            
            lcd_draw_rectangle(
                box_coords.x,
                box_coords.y,
                BATTLESHIP_BOX_WIDTH,
                BATTLESHIP_BOX_HEIGHT,
                board_color,
                false
            );
            lcd_draw_rectangle(
                box_coords.x + BATTLESHIP_BORDER_WIDTH/2,
                box_coords.y + BATTLESHIP_BORDER_WIDTH/2,
                BATTLESHIP_BOX_WIDTH - BATTLESHIP_BORDER_WIDTH,
                BATTLESHIP_BOX_HEIGHT - BATTLESHIP_BORDER_WIDTH,
                bg_color,
                false
            );
        }
    }
    
    printf("System Control   : Game board redrawn with player color\\n\\r");
    
    printf("System Control   : Ready for ship placement\n\r");

    // System_Init_06: Ship placement phase
    // Ships to place: Carrier(5), Battleship(4), Cruiser(3), Submarine(3), Destroyer(2)
    battleship_type_t ships_to_place[] = {
        BATTLESHIP_TYPE_CARRIER,
        BATTLESHIP_TYPE_BATTLESHIP,
        BATTLESHIP_TYPE_CRUISER,
        BATTLESHIP_TYPE_SUBMARINE,
        BATTLESHIP_TYPE_DESTROYER
    };
    const char *ship_names[] = {"Carrier", "B.Ship", "Cruiser", "Sub", "Dest"};
    uint8_t ship_sizes[] = {5, 4, 3, 3, 2};
    
    // Reset board grid for ship placement
    for (int r = 0; r < 10; r++) {
        for (int c = 0; c < 10; c++) {
            board_grid[r][c] = 0;  // 0 = empty, 1 = occupied
        }
    }
    
    // Store placed ship info for redrawing
    typedef struct {
        uint8_t col;
        uint8_t row;
        battleship_type_t type;
        bool horizontal;
    } placed_ship_t;
    placed_ship_t placed_ships[5];
    num_placed = 0;
    bool ship_sunk_flags[5] = {false, false, false, false, false}; // Track which ships are already sunk
    
    // Create queue for IMU responses (one queue for all ships)
    QueueHandle_t imu_response_queue = xQueueCreate(1, sizeof(device_response_msg_t));
    
    for (int ship_index = 0; ship_index < 5; ship_index++) {
        battleship_type_t current_ship = ships_to_place[ship_index];
        uint8_t ship_size = ship_sizes[ship_index];
        bool ship_placed = false;
        bool horizontal = true;  // Default orientation
        uint8_t cursor_row = 4;
        uint8_t cursor_col = 4;
        uint8_t prev_cursor_row = 4;
        uint8_t prev_cursor_col = 4;
        bool prev_horizontal = true;
        
        printf("System Control   : Placing %s (size %d)\n\r", ship_names[ship_index], ship_size);
        
        // Draw ship placement UI (only clear and draw if first ship, otherwise just update)
        if (ship_index == 0) {
            // First ship - draw all UI elements
            draw_text(text_x, 20, "Ships", text_color, bg_color);
            draw_text(text_x, 170, "SW1:Rot", text_color, bg_color);
            draw_text(text_x, 195, "SW2:OK", text_color, bg_color);
        }
        
        // Update the ship-specific info
        char remaining[12];
        snprintf(remaining, sizeof(remaining), "Left: %d", 5 - ship_index);
        draw_text(text_x, 45, "            ", bg_color, bg_color);
        draw_text(text_x, 45, remaining, text_color, bg_color);
        
        draw_text(text_x, 110, "Place:", text_color, bg_color);
        draw_text(text_x, 135, "            ", bg_color, bg_color);
        draw_text(text_x, 135, ship_names[ship_index], text_color, bg_color);
        
        // Draw initial cursor position
        battleship_draw_ship(cursor_col, cursor_row, current_ship, horizontal,
                           BATTLESHIP_CURSOR_COLOR, bg_color);
        
        uint32_t ship_light_counter = 0;
        while (!ship_placed) {
            // Input_Control_03: Check light sensor during ship placement
            if (ship_light_counter % 20 == 0) {
                uint16_t light_level = 0;
                if (system_sensors_get_light(light_sensor_response, &light_level)) {
                    bool should_be_dark = (light_level >= 100); // Uncovered (bright) = dark mode (black)
                    if (should_be_dark != dark_mode) {
                        dark_mode = should_be_dark;
                        bg_color = dark_mode ? LCD_COLOR_BLACK : LCD_COLOR_WHITE; // dark_mode=true => black, false => white
                        text_color = dark_mode ? LCD_COLOR_WHITE : LCD_COLOR_BLACK;
                        
                        // Redraw everything with new colors
                        lcd_clear_screen(bg_color);
                        battleship_draw_game_board(0);
                        
                        // Redraw only placed ships (don't fill empty squares)
                        for (int r = 0; r < 10; r++) {
                            for (int c = 0; c < 10; c++) {
                                if (board_grid[r][c] == 1) {
                                    // Ship square - gray fill
                                    battleship_draw_cursor(c, r, board_color, LCD_COLOR_GRAY);
                                }
                            }
                        }
                        
                        // Redraw current ship cursor on top
                        battleship_draw_ship(cursor_col, cursor_row, current_ship, horizontal,
                                           BATTLESHIP_CURSOR_COLOR, bg_color);
                        
                        // Redraw UI text
                        draw_text(text_x, 20, "Ships", text_color, bg_color);
                        draw_text(text_x, 45, remaining, text_color, bg_color);
                        draw_text(text_x, 110, "Place:", text_color, bg_color);
                        draw_text(text_x, 135, ship_names[ship_index], text_color, bg_color);
                        draw_text(text_x, 170, "SW1:Rot", text_color, bg_color);
                        draw_text(text_x, 195, "SW2:OK", text_color, bg_color);
                    }
                }
            }
            ship_light_counter++;
            
            // Input_Control_00: Read IMU data to move cursor
            int16_t imu_data[3] = {0, 0, 0};  // ax, ay, az (only accelerometer data)
            bool read_success = system_sensors_imu_read(imu_response_queue, imu_data);
            
            if (!read_success) {
                printf("IMU read FAILED!\n\r");
                vTaskDelay(pdMS_TO_TICKS(50));
                continue;
            }
            
            // Use X and Y accelerometer values to determine grid position
            // When flat (1g on Z-axis): X≈0, Y≈0, Z≈±16384
            // ±2g range = ±16384 raw values
            int16_t accel_x = imu_data[0];
            int16_t accel_y = imu_data[1];
            
            // Map tilt to grid position (when flat: center at 4,4)
            // X axis controls column (tilt left/right)
            // Y axis controls row (tilt forward/back)
            // Range: ±16384 for ±2g. Divide by 2400 for more sensitivity (less tilt needed)
            // Center the ship based on its size and orientation
            int16_t center_offset_col = horizontal ? (ship_size / 2) : 0;
            int16_t center_offset_row = horizontal ? 0 : (ship_size / 2);
            
            int16_t calc_col = 4 + (accel_x / 2400) - center_offset_col;
            int16_t calc_row = 4 + (accel_y / 2400) - center_offset_row;  // Fixed Y direction
            
            // Clamp to grid boundaries accounting for ship size
            if (calc_col < 0) calc_col = 0;
            if (calc_row < 0) calc_row = 0;
            
            if (horizontal) {
                if (calc_col > 10 - ship_size) calc_col = 10 - ship_size;
                if (calc_row > 9) calc_row = 9;
            } else {
                if (calc_col > 9) calc_col = 9;
                if (calc_row > 10 - ship_size) calc_row = 10 - ship_size;
            }
            
            uint8_t new_col = (uint8_t)calc_col;
            uint8_t new_row = (uint8_t)calc_row;
            
            // Update cursor if position changed
            if (new_col != cursor_col || new_row != cursor_row) {
                // Erase old cursor squares by redrawing them
                for (int i = 0; i < ship_size; i++) {
                    uint8_t old_c = prev_horizontal ? (prev_cursor_col + i) : prev_cursor_col;
                    uint8_t old_r = prev_horizontal ? prev_cursor_row : (prev_cursor_row + i);
                    
                    // Check if this square has a placed ship
                    bool occupied = board_grid[old_r][old_c] == 1;
                    
                    if (occupied) {
                        // Draw this square as grey (part of placed ship)
                        battleship_draw_cursor(old_c, old_r, board_color, LCD_COLOR_GRAY);
                    } else {
                        // Draw empty square (bg_color with board color border)
                        battleship_draw_cursor(old_c, old_r, board_color, bg_color);
                    }
                }
                
                cursor_col = new_col;
                cursor_row = new_row;
                prev_cursor_col = cursor_col;
                prev_cursor_row = cursor_row;
                prev_horizontal = horizontal;
                
                // Draw new cursor
                battleship_draw_ship(cursor_col, cursor_row, current_ship, horizontal,
                                   BATTLESHIP_CURSOR_COLOR, bg_color);
            }
            
            vTaskDelay(pdMS_TO_TICKS(50));
            
            // Input_Control_01: SW1 to rotate ship
            if (ECE353_Events.sw1 == 1) {
                ECE353_Events.sw1 = 0;
                
                // Erase old cursor
                for (int i = 0; i < ship_size; i++) {
                    uint8_t old_c = prev_horizontal ? (prev_cursor_col + i) : prev_cursor_col;
                    uint8_t old_r = prev_horizontal ? prev_cursor_row : (prev_cursor_row + i);
                    
                    bool occupied = board_grid[old_r][old_c] == 1;
                    
                    if (occupied) {
                        battleship_draw_cursor(old_c, old_r, board_color, LCD_COLOR_GRAY);
                    } else {
                        battleship_draw_cursor(old_c, old_r, board_color, bg_color);
                    }
                }
                
                // Toggle orientation
                horizontal = !horizontal;
                
                // Adjust position if new orientation goes out of bounds
                if (horizontal) {
                    if (cursor_col > 10 - ship_size) cursor_col = 10 - ship_size;
                    if (cursor_row > 9) cursor_row = 9;
                } else {
                    if (cursor_col > 9) cursor_col = 9;
                    if (cursor_row > 10 - ship_size) cursor_row = 10 - ship_size;
                }
                
                prev_cursor_col = cursor_col;
                prev_cursor_row = cursor_row;
                prev_horizontal = horizontal;
                
                // Draw new preview
                battleship_draw_ship(cursor_col, cursor_row, current_ship, horizontal,
                                   BATTLESHIP_CURSOR_COLOR, bg_color);
                
                printf("System Control   : Rotated ship to %s\n\r", horizontal ? "horizontal" : "vertical");
            }
            
            // Remove the vTaskDelay at the end since we already have it in the movement section
            
            // Input_Control_02: SW2 to confirm placement
            if (ECE353_Events.sw2 == 1) {
                ECE353_Events.sw2 = 0;
                
                // Check for collision with existing ships
                bool collision = false;
                for (int i = 0; i < ship_size; i++) {
                    uint8_t check_row = horizontal ? cursor_row : cursor_row + i;
                    uint8_t check_col = horizontal ? cursor_col + i : cursor_col;
                    if (board_grid[check_row][check_col] != 0) {
                        collision = true;
                        break;
                    }
                }
                
                if (collision) {
                    // Show error message
                    draw_text(text_x, 110, "Ship in", LCD_COLOR_RED, bg_color);
                    draw_text(text_x, 135, "the way", LCD_COLOR_RED, bg_color);
                    vTaskDelay(pdMS_TO_TICKS(1500));
                    printf("System Control   : Collision detected!\n\r");
                    
                    // Overwrite error text with spaces to clear it, then redraw UI
                    draw_text(text_x, 110, "            ", bg_color, bg_color);  // Clear "Ship in"
                    draw_text(text_x, 135, "            ", bg_color, bg_color);  // Clear "the way"
                    
                    // Redraw the UI
                    draw_text(text_x, 20, "Ships", text_color, bg_color);
                    char remaining_restore[12];
                    snprintf(remaining_restore, sizeof(remaining_restore), "Left: %d", 5 - ship_index);
                    draw_text(text_x, 45, remaining_restore, text_color, bg_color);
                    draw_text(text_x, 110, "Place:", text_color, bg_color);
                    draw_text(text_x, 135, "            ", bg_color, bg_color);  // Clear ship name line
                    draw_text(text_x, 135, ship_names[ship_index], text_color, bg_color);
                    draw_text(text_x, 170, "SW1:Rot", text_color, bg_color);
                    draw_text(text_x, 195, "SW2:OK", text_color, bg_color);
                } else {
                    // Mark grid spaces as occupied
                    for (int i = 0; i < ship_size; i++) {
                        uint8_t mark_row = horizontal ? cursor_row : cursor_row + i;
                        uint8_t mark_col = horizontal ? cursor_col + i : cursor_col;
                        board_grid[mark_row][mark_col] = 1;
                    }
                    
                    // Store ship info for redrawing
                    placed_ships[num_placed].col = cursor_col;
                    placed_ships[num_placed].row = cursor_row;
                    placed_ships[num_placed].type = current_ship;
                    placed_ships[num_placed].horizontal = horizontal;
                    num_placed++;
                    
                    // Place the ship permanently with grey fill
                    battleship_draw_ship(cursor_col, cursor_row, current_ship, horizontal,
                                       board_color, LCD_COLOR_GRAY);
                    
                    ship_placed = true;
                    printf("System Control   : %s placed at (%d,%d) %s\n\r", 
                           ship_names[ship_index], cursor_col, cursor_row, 
                           horizontal ? "horizontal" : "vertical");
                }
            }
        }
    }
    
    printf("System Control   : All ships placed\n\r");
    
    // Update "Ships Left: 0"
    char final_remaining[12];
    snprintf(final_remaining, sizeof(final_remaining), "Left: %d", 0);
    draw_text(text_x, 45, "            ", bg_color, bg_color);  // Clear old count
    draw_text(text_x, 45, final_remaining, text_color, bg_color);
    
    // Clear the Place: messages and show "All ships placed!"
    draw_text(text_x, 110, "            ", bg_color, bg_color);  // Clear "Place:"
    draw_text(text_x, 135, "            ", bg_color, bg_color);  // Clear ship name
    draw_text(text_x, 80, "All", LCD_COLOR_GREEN, bg_color);
    draw_text(text_x, 105, "ships", LCD_COLOR_GREEN, bg_color);
    draw_text(text_x, 130, "placed!", LCD_COLOR_GREEN, bg_color);
    
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // System_Init_07: Send IPC_GAME_CONTROL_PLAYER_READY
    ipc_send_game_control(IPC_GAME_CONTROL_PLAYER_READY);
    printf("System Control   : Sent IPC_GAME_CONTROL_PLAYER_READY\n\r");
    
    // Clear entire text area (all messages) and show waiting message
    lcd_draw_rectangle(220, 0, 100, 240, bg_color, false);
    draw_text(text_x, 80, "Wait", LCD_COLOR_YELLOW, bg_color);
    draw_text(text_x, 105, "for", LCD_COLOR_YELLOW, bg_color);
    draw_text(text_x, 130, "foe", LCD_COLOR_YELLOW, bg_color);
    
    // System_Init_08: Wait for opponent's PLAYER_READY and send ACK
    // TODO: Replace with queue-based detection when ready
    bool opponent_ready = false;
    
    while (!opponent_ready) {
        // Temporary: assume opponent ready after 2 seconds for testing
        vTaskDelay(pdMS_TO_TICKS(100));
        static uint32_t wait_count = 0;
        wait_count++;
        if (wait_count > 20) {
            opponent_ready = true;
        }
    }
    
    // Send ACK when we receive opponent's PLAYER_READY
    ipc_send_game_control(IPC_GAME_CONTROL_ACK);
    printf("System Control   : Received opponent PLAYER_READY, sent ACK\n\r");
    
    // Wait for ACK from opponent
    bool received_ack = false;
    while (!received_ack) {
        // Temporary: assume ACK received after 1 second for testing
        vTaskDelay(pdMS_TO_TICKS(100));
        static uint32_t ack_count = 0;
        ack_count++;
        if (ack_count > 10) {
            received_ack = true;
        }
    }
    
    printf("System Control   : Both players ready - game starting!\n\r");
    
    // Clear entire text area and show game start message
    lcd_draw_rectangle(220, 0, 100, 240, bg_color, false);
    draw_text(text_x, 80, "Ready!", LCD_COLOR_GREEN, bg_color);
    draw_text(text_x, 110, "Game", text_color, bg_color);
    draw_text(text_x, 140, "Start!", text_color, bg_color);
    
    vTaskDelay(pdMS_TO_TICKS(1500));

    // Clear and show game UI with hit/miss stats
    lcd_draw_rectangle(220, 0, 100, 240, bg_color, false);
    
    // Display hit/miss stats at top (reset on restart)
    hits = 0;
    misses = 0;
    char stats_text[15];
    snprintf(stats_text, sizeof(stats_text), "Hits: %d", hits);
    draw_text(text_x, 10, stats_text, LCD_COLOR_GREEN, bg_color);
    snprintf(stats_text, sizeof(stats_text), "Miss: %d", misses);
    draw_text(text_x, 35, stats_text, LCD_COLOR_RED, bg_color);
    
    // Player_Roles_03: Turn indicator (Player 1 goes first)
    bool my_turn = (player_role == PLAYER_ROLE_PLAYER1);
    draw_text(text_x, 70, "Current", text_color, bg_color);
    draw_text(text_x, 95, "Move:", text_color, bg_color);
    if (my_turn) {
        draw_text(text_x, 120, "Yours", LCD_COLOR_CYAN, bg_color);
    } else {
        draw_text(text_x, 120, "Opp  ", LCD_COLOR_YELLOW, bg_color);
    }
    
    // Input_Control_04: IO expander LEDs show opponent's remaining ships (start with 5 ships = 0x1F)
    opponent_ships_remaining = 5;
    my_ships_remaining = 5; // Track my own ships for game over detection
    
    // Reset ship sunk flags for new game
    for (int i = 0; i < 5; i++) {
        ship_sunk_flags[i] = false;
    }
    
    led_value = (1 << opponent_ships_remaining) - 1; // 5 ships = 0x1F (5 LEDs on)
    system_sensors_io_expander_write(NULL, IOXP_ADDR_OUTPUT_PORT, led_value);
    
    // SW1:Fire/Wait instruction
    draw_text(text_x, 200, "         ", bg_color, bg_color); // Clear the area first
    if (my_turn) {
        draw_text(text_x, 200, "SW1:Fire", text_color, bg_color);
    } else {
        draw_text(text_x, 200, "Wait...", text_color, bg_color);
    }
    
    // Attack phase - use joystick to select target
    uint8_t target_col = 0;
    uint8_t target_row = 0;
    
    // Reset opponent grid for new game
    for (int r = 0; r < 10; r++) {
        for (int c = 0; c < 10; c++) {
            opponent_grid[r][c] = 0;
        }
    }
    
    // Variable for receiving incoming fire coordinates
    ipc_fire_payload_t incoming_fire;
    
    // Draw initial board state (Player 1 starts, so show attack view)
    redraw_board_for_turn(my_turn, board_grid, opponent_grid, target_col, target_row, board_color, bg_color);
    
    printf("System Control   : Entering attack mode - cursor at (%d, %d)\n\r", target_col, target_row);
    
    light_check_counter = 0; // Reuse counter from earlier
    while(1)
    {
        // Input_Control_03: Check light sensor every 10 iterations to toggle light/dark mode
        if (light_check_counter % 10 == 0) {
            uint16_t light_level = 0;
            if (system_sensors_get_light(light_sensor_response, &light_level)) {
                bool should_be_dark = (light_level >= 100); // Uncovered (bright/high light) = black bg, Covered (dark/low light) = white bg
                if (should_be_dark != dark_mode) {
                    dark_mode = should_be_dark;
                    bg_color = dark_mode ? LCD_COLOR_BLACK : LCD_COLOR_WHITE; // dark_mode=true => black, false => white
                    text_color = dark_mode ? LCD_COLOR_WHITE : LCD_COLOR_BLACK;
                    
                    // Redraw entire UI with new colors
                    lcd_clear_screen(bg_color);
                    
                    // Redraw board based on whose turn it is
                    redraw_board_for_turn(my_turn, board_grid, opponent_grid, target_col, target_row, board_color, bg_color);
                    
                    // Redraw stats and UI text
                    snprintf(stats_text, sizeof(stats_text), "Hits: %d", hits);
                    draw_text(text_x, 10, stats_text, LCD_COLOR_GREEN, bg_color);
                    snprintf(stats_text, sizeof(stats_text), "Miss: %d", misses);
                    draw_text(text_x, 35, stats_text, LCD_COLOR_RED, bg_color);
                    draw_text(text_x, 70, "Current", text_color, bg_color);
                    draw_text(text_x, 95, "Move:", text_color, bg_color);
                    draw_text(text_x, 200, "         ", bg_color, bg_color); // Clear
                    if (my_turn) {
                        draw_text(text_x, 120, "Yours", LCD_COLOR_CYAN, bg_color);
                        draw_text(text_x, 200, "SW1:Fire", text_color, bg_color);
                    } else {
                        draw_text(text_x, 120, "Opp  ", LCD_COLOR_YELLOW, bg_color);
                        draw_text(text_x, 200, "Wait...", text_color, bg_color);
                    }
                    
                    // Redraw cursor
                    battleship_draw_cursor(target_col, target_row, BATTLESHIP_CURSOR_COLOR, bg_color);
                }
            }
        }
        light_check_counter++;
        
        // Read joystick to move target cursor (only when it's your turn)
        if (my_turn) {
            uint16_t joystick_x = joystick_read_x();
            uint16_t joystick_y = joystick_read_y();
            
            bool moved = false;
            uint8_t new_col = target_col;
            uint8_t new_row = target_row;
            
            // Map joystick to grid movement - SWAPPED X to fix reversed direction
            // X axis: 0-65535, center ~32768, thresholds at 0x3FFF and 0xBFFF
            if (joystick_x < JOYSTICK_THRESH_X_RIGHT && target_col < 9) {
                new_col = target_col + 1; // Move right
                moved = true;
            } else if (joystick_x > JOYSTICK_THRESH_X_LEFT && target_col > 0) {
                new_col = target_col - 1; // Move left
                moved = true;
            }
            
            // Y axis: 0-65535, center ~32768
            if (joystick_y < JOYSTICK_THRESH_Y_DOWN && target_row < 9) {
                new_row = target_row + 1; // Move down
                moved = true;
            } else if (joystick_y > JOYSTICK_THRESH_Y_UP && target_row > 0) {
                new_row = target_row - 1; // Move up
                moved = true;
            }
            
            if (moved) {
                // Clear old cursor position - check opponent_grid (not board_grid)
                if (opponent_grid[target_row][target_col] == 2) {
                    // Hit marker - restore red fill
                    battleship_draw_cursor(target_col, target_row, board_color, LCD_COLOR_RED);
                } else if (opponent_grid[target_row][target_col] == 3) {
                    // Miss marker - restore white fill
                    battleship_draw_cursor(target_col, target_row, board_color, LCD_COLOR_WHITE);
                } else {
                    // Unknown square - restore bg_color fill
                    battleship_draw_cursor(target_col, target_row, board_color, bg_color);
                }
                
                // Update position
                target_col = new_col;
                target_row = new_row;
                
                // Draw cursor at new position - green border with bg_color fill (hollow square)
                battleship_draw_cursor(target_col, target_row, BATTLESHIP_CURSOR_COLOR, bg_color);
                
                printf("System Control   : Cursor at (%d, %d)\n\r", target_row, target_col);
                
                vTaskDelay(pdMS_TO_TICKS(150)); // Debounce
            }
        }
        
        // Check for END_GAME control message (opponent lost)
        ipc_game_control_t game_ctrl;
        if (xQueueReceive(Queue_IPC_Rx_Game_Control, &game_ctrl, 0) == pdPASS) {
            if (game_ctrl == IPC_GAME_CONTROL_END_GAME) {
                printf("System Control   : Received END_GAME - I WON!\n\r");
                
                // Display WIN message (centered on screen)
                lcd_clear_screen(bg_color);
                draw_text(120, 80, "GAME", LCD_COLOR_GREEN, bg_color);
                draw_text(120, 110, "OVER", LCD_COLOR_GREEN, bg_color);
                draw_text(120, 160, "YOU", LCD_COLOR_GREEN, bg_color);
                draw_text(120, 190, "WIN!", LCD_COLOR_GREEN, bg_color);
                
                vTaskDelay(pdMS_TO_TICKS(3000));
                
                // Communication_Flow_06 & 07: Wait for SW1 or NEW_GAME
                lcd_clear_screen(bg_color);
                draw_text(60, 100, "Press", text_color, bg_color);
                draw_text(60, 130, "SW1 to", text_color, bg_color);
                draw_text(60, 160, "Restart", text_color, bg_color);
                
                // Wait for SW1 press or NEW_GAME from opponent
                bool game_restarted = false;
                while (!game_restarted) {
                    if (ECE353_Events.sw1 == 1) {
                        ECE353_Events.sw1 = 0;
                        // Swap roles: P1 becomes P2, P2 becomes P1
                        player_role = (player_role == PLAYER_ROLE_PLAYER1) ? PLAYER_ROLE_PLAYER2 : PLAYER_ROLE_PLAYER1;
                        ipc_send_game_control(IPC_GAME_CONTROL_NEW_GAME);
                        printf("System Control   : Restarting game as Player %d\n\r", player_role);
                        game_restarted = true;
                    }
                    
                    // Check if other board pressed SW1 first
                    ipc_game_control_t ctrl_msg;
                    if (xQueueReceive(Queue_IPC_Rx_Game_Control, &ctrl_msg, 0) == pdPASS) {
                        if (ctrl_msg == IPC_GAME_CONTROL_NEW_GAME) {
                            // Swap roles: P1 becomes P2, P2 becomes P1
                            player_role = (player_role == PLAYER_ROLE_PLAYER1) ? PLAYER_ROLE_PLAYER2 : PLAYER_ROLE_PLAYER1;
                            ipc_send_game_control(IPC_GAME_CONTROL_ACK);
                            printf("System Control   : Restarting game as Player %d\n\r", player_role);
                            game_restarted = true;
                        }
                    }
                    
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
                
                // Jump to ship placement phase
                goto RESTART_GAME;
            }
        }
        
        // Handle incoming fire from opponent
        if(xQueueReceive(Queue_IPC_Rx_Fire, &incoming_fire, 0) == pdPASS) {
            ipc_result_t result;
            if(board_grid[incoming_fire.row][incoming_fire.col] == 1) {
                result = IPC_RESULT_HIT;
                printf("System Control   : HIT!\n\r");
                // Mark as hit with red X and update grid state
                board_grid[incoming_fire.row][incoming_fire.col] = 2; // 2 = hit
                battleship_draw_cursor(incoming_fire.col, incoming_fire.row, board_color, LCD_COLOR_RED);
                
                // Check if any ship was sunk (only if not already reported as sunk)
                for (int s = 0; s < num_placed; s++) {
                    if (ship_sunk_flags[s]) continue; // Skip ships already sunk
                    
                    placed_ship_t *ship = &placed_ships[s];
                    uint8_t ship_size = ship_sizes[s];
                    bool ship_sunk = true;
                    
                    // Check all squares of this ship
                    for (int i = 0; i < ship_size; i++) {
                        uint8_t check_col = ship->horizontal ? (ship->col + i) : ship->col;
                        uint8_t check_row = ship->horizontal ? ship->row : (ship->row + i);
                        if (board_grid[check_row][check_col] != 2) {
                            ship_sunk = false;
                            break;
                        }
                    }
                    
                    if (ship_sunk) {
                        printf("System Control   : My ship sunk!\n\r");
                        ship_sunk_flags[s] = true; // Mark this ship as sunk
                        my_ships_remaining--; // Decrement my ships remaining
                        // Send SUNK result instead of just HIT
                        result = IPC_RESULT_SUNK;
                        break;
                    }
                }
            } else {
                result = IPC_RESULT_MISS;
                printf("System Control   : MISS!\n\r");
                // Mark as miss with white dot and update grid state
                board_grid[incoming_fire.row][incoming_fire.col] = 3; // 3 = miss
                battleship_draw_cursor(incoming_fire.col, incoming_fire.row, board_color, LCD_COLOR_WHITE);
            }
            
            // Send result back to opponent
            if(ipc_send_result(result)) {
                printf("System Control   : Result sent successfully\n\r");
            } else {
                printf("System Control   : ERROR - Failed to send result\n\r");
            }
            
            // Communication_Flow_05: Check if I lost (all my ships sunk)
            if (my_ships_remaining == 0) {
                printf("System Control   : GAME OVER - I LOST!\n\r");
                // Send END_GAME message to opponent
                ipc_send_game_control(IPC_GAME_CONTROL_END_GAME);
                
                // Display LOSE message (centered on screen)
                lcd_clear_screen(bg_color);
                draw_text(120, 80, "GAME", LCD_COLOR_RED, bg_color);
                draw_text(120, 110, "OVER", LCD_COLOR_RED, bg_color);
                draw_text(120, 160, "YOU", LCD_COLOR_RED, bg_color);
                draw_text(120, 190, "LOSE", LCD_COLOR_RED, bg_color);
                
                vTaskDelay(pdMS_TO_TICKS(3000));
                
                // Communication_Flow_06 & 07: Wait for SW1 to start new game
                lcd_clear_screen(bg_color);
                draw_text(60, 100, "Press", text_color, bg_color);
                draw_text(60, 130, "SW1 to", text_color, bg_color);
                draw_text(60, 160, "Restart", text_color, bg_color);
                
                // Wait for SW1 press (either player can restart)
                bool game_restarted = false;
                while (!game_restarted) {
                    if (ECE353_Events.sw1 == 1) {
                        ECE353_Events.sw1 = 0;
                        // Swap roles: P1 becomes P2, P2 becomes P1
                        player_role = (player_role == PLAYER_ROLE_PLAYER1) ? PLAYER_ROLE_PLAYER2 : PLAYER_ROLE_PLAYER1;
                        ipc_send_game_control(IPC_GAME_CONTROL_NEW_GAME);
                        printf("System Control   : Restarting game as Player %d\n\r", player_role);
                        game_restarted = true;
                    }
                    
                    // Check if other board pressed SW1 first
                    ipc_game_control_t ctrl_msg;
                    if (xQueueReceive(Queue_IPC_Rx_Game_Control, &ctrl_msg, 0) == pdPASS) {
                        if (ctrl_msg == IPC_GAME_CONTROL_NEW_GAME) {
                            // Swap roles: P1 becomes P2, P2 becomes P1
                            player_role = (player_role == PLAYER_ROLE_PLAYER1) ? PLAYER_ROLE_PLAYER2 : PLAYER_ROLE_PLAYER1;
                            ipc_send_game_control(IPC_GAME_CONTROL_ACK);
                            printf("System Control   : Restarting game as Player %d\n\r", player_role);
                            game_restarted = true;
                        }
                    }
                    
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
                
                // Jump to ship placement phase (skip role establishment since already done)
                goto RESTART_GAME;
            }
            
            // Show hit/miss/sunk notification (adjusted spacing to prevent overlap)
            if(result == IPC_RESULT_SUNK) {
                draw_text(text_x, 150, "They", LCD_COLOR_YELLOW, bg_color);
                draw_text(text_x, 175, "SUNK!", LCD_COLOR_YELLOW, bg_color);
            } else if(result == IPC_RESULT_HIT) {
                draw_text(text_x, 150, "They", LCD_COLOR_RED, bg_color);
                draw_text(text_x, 175, "HIT!", LCD_COLOR_RED, bg_color);
            } else {
                draw_text(text_x, 150, "They", LCD_COLOR_WHITE, bg_color);
                draw_text(text_x, 175, "MISS", LCD_COLOR_WHITE, bg_color);
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
            lcd_draw_rectangle(text_x, 150, 320 - text_x, 50, bg_color, false);
            
            // After opponent fires, it's now your turn
            my_turn = true;
            
            // Redraw board to show attack view (opponent's grid)
            redraw_board_for_turn(my_turn, board_grid, opponent_grid, target_col, target_row, board_color, bg_color);
            
            lcd_draw_rectangle(text_x, 120, 320 - text_x, 25, bg_color, false);
            draw_text(text_x, 120, "Yours", LCD_COLOR_CYAN, bg_color);
            lcd_draw_rectangle(text_x, 200, 320 - text_x, 25, bg_color, false);
            draw_text(text_x, 200, "SW1:Fire", text_color, bg_color);
            printf("System Control   : Your turn now\n\r");
        }
        
        // Check if SW1 pressed to fire
        if(my_turn && ECE353_Events.sw1 == 1)
        {
            ECE353_Events.sw1 = 0;
            
            // Check if already fired at this location
            if(opponent_grid[target_row][target_col] != 0) {
                // Already fired here - show message
                draw_text(text_x, 150, "Already", LCD_COLOR_ORANGE, bg_color);
                draw_text(text_x, 175, "fired!", LCD_COLOR_ORANGE, bg_color);
                vTaskDelay(pdMS_TO_TICKS(1000));
                lcd_draw_rectangle(text_x, 150, 320 - text_x, 50, bg_color, false);
                vTaskDelay(pdMS_TO_TICKS(10));
                continue; // Don't send fire command, stay on my turn
            }
            
            printf("System Control   : FIRE at (col=%d, row=%d)\n\r", target_col, target_row);
            
            // Send IPC_CMD_FIRE packet (row, col order)
            if(ipc_send_fire(target_row, target_col)) {
                printf("System Control   : Fire command sent\n\r");
            } else {
                printf("System Control   : ERROR - Failed to send fire command\n\r");
            }
            
            // After you fire, it's now opponent's turn
            my_turn = false;
            
            // Redraw board to show defense view (your ships)
            redraw_board_for_turn(my_turn, board_grid, opponent_grid, target_col, target_row, board_color, bg_color);
            
            lcd_draw_rectangle(text_x, 120, 320 - text_x, 25, bg_color, false);
            draw_text(text_x, 120, "Opp  ", LCD_COLOR_YELLOW, bg_color);
            lcd_draw_rectangle(text_x, 200, 320 - text_x, 25, bg_color, false);
            draw_text(text_x, 200, "Wait...", text_color, bg_color);
            printf("System Control   : Opponent's turn now\n\r");
        }
        
        // Check for result from opponent (non-blocking)
        ipc_result_t fire_result;
        if(xQueueReceive(Queue_IPC_Rx_Result, &fire_result, 0) == pdPASS)
        {
            if(fire_result == IPC_RESULT_HIT) {
                hits++;
                printf("System Control   : Your shot was a HIT!\n\r");
                // Mark opponent's grid with hit
                opponent_grid[target_row][target_col] = 2;
                // Show "You HIT!" message (adjusted spacing)
                draw_text(text_x, 150, "You", LCD_COLOR_GREEN, bg_color);
                draw_text(text_x, 175, "HIT!", LCD_COLOR_GREEN, bg_color);
                vTaskDelay(pdMS_TO_TICKS(1000));
                lcd_draw_rectangle(text_x, 150, 320 - text_x, 50, bg_color, false);
            } else if(fire_result == IPC_RESULT_MISS) {
                misses++;
                printf("System Control   : Your shot was a MISS\n\r");
                // Mark opponent's grid with miss
                opponent_grid[target_row][target_col] = 3;
                // Show "You MISS" message (adjusted spacing)
                draw_text(text_x, 150, "You", LCD_COLOR_GRAY, bg_color);
                draw_text(text_x, 175, "MISS", LCD_COLOR_GRAY, bg_color);
                vTaskDelay(pdMS_TO_TICKS(1000));
                lcd_draw_rectangle(text_x, 150, 320 - text_x, 50, bg_color, false);
            } else if(fire_result == IPC_RESULT_SUNK) {
                hits++;
                printf("System Control   : Your shot SUNK an enemy ship!\n\r");
                // Mark opponent's grid with hit
                opponent_grid[target_row][target_col] = 2;
                // Show "You SUNK!" message (adjusted spacing)
                draw_text(text_x, 150, "You", LCD_COLOR_YELLOW, bg_color);
                draw_text(text_x, 175, "SUNK!", LCD_COLOR_YELLOW, bg_color);
                vTaskDelay(pdMS_TO_TICKS(1500));
                lcd_draw_rectangle(text_x, 150, 320 - text_x, 50, bg_color, false);
                // Decrement opponent's remaining ships and update LEDs
                if (opponent_ships_remaining > 0) {
                    opponent_ships_remaining--;
                    led_value = (1 << opponent_ships_remaining) - 1;
                    system_sensors_io_expander_write(NULL, IOXP_ADDR_OUTPUT_PORT, led_value);
                    printf("System Control   : Enemy ships remaining: %d, LED value: 0x%02X\n\r", 
                           opponent_ships_remaining, led_value);
                    
                    // Communication_Flow_05: Check if opponent lost (I won!)
                    if (opponent_ships_remaining == 0) {
                        printf("System Control   : GAME OVER - I WON!\n\r");
                        
                        // Display WIN message (centered on screen)
                        lcd_clear_screen(bg_color);
                        draw_text(120, 80, "GAME", LCD_COLOR_GREEN, bg_color);
                        draw_text(120, 110, "OVER", LCD_COLOR_GREEN, bg_color);
                        draw_text(120, 160, "YOU", LCD_COLOR_GREEN, bg_color);
                        draw_text(120, 190, "WIN!", LCD_COLOR_GREEN, bg_color);
                        
                        vTaskDelay(pdMS_TO_TICKS(3000));
                        
                        // Communication_Flow_06 & 07: Wait for SW1 or receive NEW_GAME
                        lcd_clear_screen(bg_color);
                        draw_text(60, 100, "Press", text_color, bg_color);
                        draw_text(60, 130, "SW1 to", text_color, bg_color);
                        draw_text(60, 160, "Restart", text_color, bg_color);
                        
                        // Wait for SW1 press or NEW_GAME from opponent
                        bool game_restarted = false;
                        while (!game_restarted) {
                            if (ECE353_Events.sw1 == 1) {
                                ECE353_Events.sw1 = 0;
                                // Swap roles: P1 becomes P2, P2 becomes P1
                                player_role = (player_role == PLAYER_ROLE_PLAYER1) ? PLAYER_ROLE_PLAYER2 : PLAYER_ROLE_PLAYER1;
                                ipc_send_game_control(IPC_GAME_CONTROL_NEW_GAME);
                                printf("System Control   : Restarting game as Player %d\n\r", player_role);
                                game_restarted = true;
                            }
                            
                            // Check if other board pressed SW1 first
                            ipc_game_control_t ctrl_msg;
                            if (xQueueReceive(Queue_IPC_Rx_Game_Control, &ctrl_msg, 0) == pdPASS) {
                                if (ctrl_msg == IPC_GAME_CONTROL_NEW_GAME) {
                                    // Swap roles: P1 becomes P2, P2 becomes P1
                                    player_role = (player_role == PLAYER_ROLE_PLAYER1) ? PLAYER_ROLE_PLAYER2 : PLAYER_ROLE_PLAYER1;
                                    ipc_send_game_control(IPC_GAME_CONTROL_ACK);
                                    printf("System Control   : Restarting game as Player %d\n\r", player_role);
                                    game_restarted = true;
                                }
                            }
                            
                            vTaskDelay(pdMS_TO_TICKS(10));
                        }
                        
                        // Jump to ship placement phase
                        goto RESTART_GAME;
                    }
                }
            }
            
            // Update stats display (clear area fully to right edge to prevent ghosting)
            lcd_draw_rectangle(text_x, 10, 320 - text_x, 25, bg_color, false);
            snprintf(stats_text, sizeof(stats_text), "Hits: %d", hits);
            draw_text(text_x, 10, stats_text, LCD_COLOR_GREEN, bg_color);
            lcd_draw_rectangle(text_x, 35, 320 - text_x, 25, bg_color, false);
            snprintf(stats_text, sizeof(stats_text), "Miss: %d", misses);
            draw_text(text_x, 35, stats_text, LCD_COLOR_RED, bg_color);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief
 * This function will initialize all of the hardware resources for HW06
 * System_Init_00: Initialize console, LCD, LEDs, buttons, buzzer, joystick, SPI, and I2C in that order.
 */
void app_init_hw(void)
{
    /* Initialize console */
    console_init();
    printf("**************************************************\n\r");
    printf("* %s\n\r", APP_DESCRIPTION);
    printf("* Date: %s\n\r", __DATE__);
    printf("* Time: %s\n\r", __TIME__);
    printf("* Name:%s\n\r", NAME);
    printf("**************************************************\n\r");

    /* Initialize LCD */
    lcd_initialize();
    printf("LCD initialized\n\r");

    /* Initialize LEDs */
    leds_init_gpio();
    printf("LEDs initialized\n\r");

    /* Initialize buttons */
    buttons_init_gpio();
    buttons_init_timer();
    printf("Buttons initialized\n\r");

    /* Initialize buzzer */
    buzzer_init(0.5f, 1000);  // 50% duty cycle, 1kHz frequency
    printf("Buzzer initialized\n\r");

    /* Initialize joystick */
    joystick_init();
    printf("Joystick initialized\n\r");

    /* Initialize the SPI interface */
    SPI_Obj = spi_init(PIN_SPI_MOSI, PIN_SPI_MISO, PIN_SPI_CLK);
    if (SPI_Obj == NULL)
    {
        printf("SPI initialization failed!\n\r");
        for(int i = 0; i < 10000; i++);
        CY_ASSERT(0);
    }
    printf("SPI initialized\n\r");

    /* Configure the chip select pins for the EEPROM and IMU*/
    cyhal_gpio_init(PIN_SPI_EEPROM_CS, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, true);
    cyhal_gpio_init(PIN_IMU_CS, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, true);

    /* Initialize the I2C interface */
    I2C_Obj = i2c_init(PIN_I2C_SDA, PIN_I2C_SCL);
    if (I2C_Obj == NULL)
    {
        printf("I2C initialization failed!\n\r");
        for(int i = 0; i < 10000; i++);
        CY_ASSERT(0);
    }
    printf("I2C initialized\n\r");
    printf("**************************************************\n\r");
    printf("Hardware initialization complete\n\r");
    printf("**************************************************\n\r");
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

    if(!task_system_control_resources_init())
    {
        printf("System Control Task initialization failed!\n\r");
        for(int i = 0; i < 10000; i++);
        CY_ASSERT(0);
    }

    if(!task_console_init())
    {
        printf("Console initialization failed!\n\r");
        for(int i = 0; i < 10000; i++);
        CY_ASSERT(0);
    }

    if(!task_io_expander_resources_init(I2C_Obj, &Semaphore_I2C))
    {
        printf("IO Expander Task initialization failed!\n\r");
        for(int i = 0; i < 10000; i++);
        CY_ASSERT(0);
    }

    if(!task_light_sensor_resources_init(I2C_Obj, &Semaphore_I2C))
    {
        printf("Light Sensor Task initialization failed!\n\r");
        for(int i = 0; i < 10000; i++);
        CY_ASSERT(0);
    }

    if(!task_eeprom_resources_init(&Semaphore_SPI, SPI_Obj, PIN_SPI_EEPROM_CS))
    {
        printf("EEPROM Task initialization failed!\n\r");
        for(int i = 0; i < 10000; i++);
        CY_ASSERT(0);
    }

    if(!task_imu_resources_init(&Semaphore_SPI, SPI_Obj, PIN_IMU_CS))
    {
        printf("IMU Task initialization failed!\n\r");
        for(int i = 0; i < 10000; i++);
        CY_ASSERT(0);
    }

    if(!task_ipc_init())
    {
        printf("IPC Task initialization failed!\n\r");
        for(int i = 0; i < 10000; i++);
        CY_ASSERT(0);
    }

    /* Start the scheduler*/
    vTaskStartScheduler();

    /* Will never reach this loop once the scheduler starts */
    while (1)
    {
    }
}

#endif