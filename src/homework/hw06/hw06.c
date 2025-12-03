/**
 * @file hw06.c
 * @author Your Name
 * @brief ECE353 HW06 - Battleship Game Implementation
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
#include "task_buttons.h"
#include "task_joystick.h"
#include "task_buzzer.h"
#include "task_lcd.h"
#include "task_ipc.h"
#include "task_io_expander.h"
#include "task_light_sensor.h"
#include "task_imu.h"
#include "battleship.h"
#include <string.h>

char NAME[] = "Lingxiao Xiao";
char APP_DESCRIPTION[] = "ECE353: HW06 - Battleship Game";

/*****************************************************************************/
/* Game State Definitions                                                    */
/*****************************************************************************/
typedef enum {
    GAME_STATE_INIT,              // Initial state, waiting for player selection
    GAME_STATE_SHIP_PLACEMENT,    // Placing ships on board
    GAME_STATE_WAIT_FOR_READY,    // Waiting for both players to be ready
    GAME_STATE_YOUR_TURN,         // Your turn to attack
    GAME_STATE_OPPONENT_TURN,     // Opponent's turn
    GAME_STATE_GAME_OVER,         // Game finished
} game_state_t;

typedef enum {
    SHIP_PLACEMENT_CARRIER,       // Placing 5-space carrier
    SHIP_PLACEMENT_BATTLESHIP,    // Placing 4-space battleship
    SHIP_PLACEMENT_CRUISER,       // Placing 3-space cruiser
    SHIP_PLACEMENT_SUBMARINE,     // Placing 3-space submarine
    SHIP_PLACEMENT_DESTROYER,     // Placing 2-space destroyer
    SHIP_PLACEMENT_COMPLETE,      // All ships placed
} ship_placement_state_t;

typedef struct {
    uint8_t row;
    uint8_t col;
    uint8_t length;
    bool horizontal;
    bool placed;
} ship_t;

typedef struct {
    game_state_t state;
    uint8_t player_id;              // 0 = Player 1 (Blue), 1 = Player 2 (Red)
    bool is_player1_next;           // For role rotation
    ship_placement_state_t placement_state;
    ship_t ships[5];                // 5 ships total
    uint8_t board[10][10];          // 0 = empty, 1-5 = ship type, 0xFF = hit, 0xFE = miss
    uint8_t opponent_board[10][10]; // Tracking opponent's board
    uint8_t cursor_row;
    uint8_t cursor_col;
    bool ship_horizontal;           // Orientation for placement
    uint8_t ships_remaining;
    uint8_t opponent_ships_remaining;
    bool dark_mode;                 // Light sensor theme
} game_context_t;

/*****************************************************************************/
/* Global Variables                                                          */
/*****************************************************************************/
static game_context_t game;
cyhal_i2c_t *I2C_Obj = NULL;
cyhal_spi_t *SPI_Obj = NULL;

SemaphoreHandle_t Semaphore_I2C = NULL;
SemaphoreHandle_t Semaphore_SPI = NULL;

QueueHandle_t Queue_Game_Control = NULL;
QueueHandle_t Queue_LCD_Response = NULL;

// Use existing event group from rtos_events.h
// ECE353_RTOS_Events with:
// - ECE353_EVENT_SW1_PRESSED
// - ECE353_EVENT_SW2_PRESSED

// Additional event bits for game control
#define EVENT_IPC_RECEIVED      (1 << 8)
#define EVENT_IMU_UPDATE        (1 << 9)
#define EVENT_JOYSTICK_UPDATE   (1 << 10)
#define EVENT_LIGHT_UPDATE      (1 << 11)

/*****************************************************************************/
/* Function Declarations                                                     */
/*****************************************************************************/
void task_game_master(void *arg);
void game_init_state(void);
void game_handle_init_state(void);
void game_handle_ship_placement(void);
void game_handle_your_turn(void);
void game_handle_opponent_turn(void);
void game_handle_game_over(void);
void game_place_ship(uint8_t ship_idx);
bool game_is_valid_ship_placement(uint8_t row, uint8_t col, uint8_t length, bool horizontal);
void game_draw_ship_on_board(uint8_t ship_idx);
void game_send_ipc_packet(ipc_packet_t *packet);
void game_handle_ipc_packet(ipc_packet_t *packet);
void game_update_cursor(void);
void game_draw_message(const char *msg);
uint8_t game_get_ship_length(ship_placement_state_t state);
battleship_type_t game_get_ship_type(ship_placement_state_t state);
void game_update_io_expander_leds(void);

/*****************************************************************************/
/* Task Initialization                                                       */
/*****************************************************************************/

/**
 * @brief Initialize the game master task and resources
 */
bool task_game_master_init(void)
{
    // Use existing ECE353_RTOS_Events event group (created in main.c)
    
    // Create queue for LCD responses
    Queue_LCD_Response = xQueueCreate(5, sizeof(lcd_cmd_status_t));
    if (Queue_LCD_Response == NULL) {
        return false;
    }

    // Create I2C semaphore
    Semaphore_I2C = xSemaphoreCreateMutex();
    if (Semaphore_I2C == NULL) {
        return false;
    }

    // Create SPI semaphore
    Semaphore_SPI = xSemaphoreCreateMutex();
    if (Semaphore_SPI == NULL) {
        return false;
    }

    // Create the game master task
    if (xTaskCreate(
            task_game_master,
            "Game Master",
            configMINIMAL_STACK_SIZE * 8,
            NULL,
            tskIDLE_PRIORITY + 3,
            NULL) != pdPASS) {
        return false;
    }

    return true;
}

/*****************************************************************************/
/* Game State Management Functions                                           */
/*****************************************************************************/

/**
 * @brief Initialize game state
 */
void game_init_state(void)
{
    memset(&game, 0, sizeof(game_context_t));
    game.state = GAME_STATE_INIT;
    game.player_id = 0xFF; // Not assigned yet
    game.placement_state = SHIP_PLACEMENT_CARRIER;
    game.cursor_row = 0;
    game.cursor_col = 0;
    game.ship_horizontal = true;
    game.ships_remaining = 5;
    game.opponent_ships_remaining = 5;
    game.dark_mode = false;
    
    // Initialize ship data
    game.ships[0].length = 5; // Carrier
    game.ships[1].length = 4; // Battleship
    game.ships[2].length = 3; // Cruiser
    game.ships[3].length = 3; // Submarine
    game.ships[4].length = 2; // Destroyer
    
    for (int i = 0; i < 5; i++) {
        game.ships[i].placed = false;
        game.ships[i].horizontal = true;
    }
    
    // Clear boards
    memset(game.board, 0, sizeof(game.board));
    memset(game.opponent_board, 0, sizeof(game.opponent_board));
}

/**
 * @brief Get ship length based on placement state
 */
uint8_t game_get_ship_length(ship_placement_state_t state)
{
    switch (state) {
        case SHIP_PLACEMENT_CARRIER:    return 5;
        case SHIP_PLACEMENT_BATTLESHIP: return 4;
        case SHIP_PLACEMENT_CRUISER:    return 3;
        case SHIP_PLACEMENT_SUBMARINE:  return 3;
        case SHIP_PLACEMENT_DESTROYER:  return 2;
        default: return 0;
    }
}

/**
 * @brief Get ship type based on placement state
 */
battleship_type_t game_get_ship_type(ship_placement_state_t state)
{
    switch (state) {
        case SHIP_PLACEMENT_CARRIER:    return BATTLESHIP_TYPE_CARRIER;
        case SHIP_PLACEMENT_BATTLESHIP: return BATTLESHIP_TYPE_BATTLESHIP;
        case SHIP_PLACEMENT_CRUISER:    return BATTLESHIP_TYPE_CRUISER;
        case SHIP_PLACEMENT_SUBMARINE:  return BATTLESHIP_TYPE_SUBMARINE;
        case SHIP_PLACEMENT_DESTROYER:  return BATTLESHIP_TYPE_DESTROYER;
        default: return BATTLESHIP_TYPE_NONE;
    }
}

/**
 * @brief Check if ship placement is valid
 */
bool game_is_valid_ship_placement(uint8_t row, uint8_t col, uint8_t length, bool horizontal)
{
    // Check boundaries
    if (horizontal) {
        if (col + length > 10) return false;
    } else {
        if (row + length > 10) return false;
    }
    
    // Check for overlapping ships
    for (uint8_t i = 0; i < length; i++) {
        uint8_t check_row = horizontal ? row : row + i;
        uint8_t check_col = horizontal ? col + i : col;
        
        if (game.board[check_row][check_col] != 0) {
            return false;
        }
    }
    
    return true;
}

/**
 * @brief Place ship on the board
 */
void game_place_ship(uint8_t ship_idx)
{
    ship_t *ship = &game.ships[ship_idx];
    ship->row = game.cursor_row;
    ship->col = game.cursor_col;
    ship->horizontal = game.ship_horizontal;
    ship->placed = true;
    
    // Mark board positions
    for (uint8_t i = 0; i < ship->length; i++) {
        uint8_t r = ship->horizontal ? ship->row : ship->row + i;
        uint8_t c = ship->horizontal ? ship->col + i : ship->col;
        game.board[r][c] = ship_idx + 1; // Ship ID 1-5
    }
}

/**
 * @brief Draw ship on LCD via gatekeeper
 */
void game_draw_ship_on_board(uint8_t ship_idx)
{
    ship_t *ship = &game.ships[ship_idx];
    uint16_t color = (game.player_id == 0) ? BATTLESHIP_PLAYER_0_COLOR : BATTLESHIP_PLAYER_1_COLOR;
    
    battleship_send_draw_ship(
        xQueue_LCD,
        Queue_LCD_Response,
        ship->row,
        ship->col,
        game_get_ship_type(game.placement_state),
        ship->horizontal
    );
}

/**
 * @brief Update cursor position on LCD
 */
void game_update_cursor(void)
{
    // Clear old cursor (draw as board color)
    static uint8_t old_row = 0;
    static uint8_t old_col = 0;
    
    battleship_send_draw_tile(
        xQueue_LCD,
        Queue_LCD_Response,
        old_row,
        old_col,
        LCD_COLOR_BLUE,
        LCD_COLOR_BLACK
    );
    
    // Draw new cursor
    battleship_send_draw_tile(
        xQueue_LCD,
        Queue_LCD_Response,
        game.cursor_row,
        game.cursor_col,
        BATTLESHIP_CURSOR_COLOR,
        LCD_COLOR_BLACK
    );
    
    old_row = game.cursor_row;
    old_col = game.cursor_col;
}

/**
 * @brief Draw message on LCD
 */
void game_draw_message(const char *msg)
{
    printf("Game: %s\n", msg);
    // Could also send to LCD console if implemented
}

/**
 * @brief Update IO Expander LEDs to show opponent ships remaining
 */
void game_update_io_expander_leds(void)
{
    // Light up LEDs for remaining opponent ships (5 max)
    uint8_t led_mask = 0x00;
    for (uint8_t i = 0; i < game.opponent_ships_remaining && i < 5; i++) {
        led_mask |= (1 << i);
    }
    
    xSemaphoreTake(Semaphore_I2C, portMAX_DELAY);
    system_sensors_io_expander_write(I2C_Obj, IOXP_ADDR_OUTPUT_PORT, led_mask);
    xSemaphoreGive(Semaphore_I2C);
}

/**
 * @brief Handle INIT state - waiting for SW1 press to claim player role
 */
void game_handle_init_state(void)
{
    // Display "Press SW1 to Start"
    battleship_send_clear_screen(xQueue_LCD, Queue_LCD_Response);
    game_draw_message("Press SW1 to Start");
    
    while (game.state == GAME_STATE_INIT) {
        EventBits_t bits = xEventGroupWaitBits(
            ECE353_RTOS_Events,
            ECE353_EVENT_SW1_PRESSED | EVENT_IPC_RECEIVED,
            pdTRUE,  // Clear on exit
            pdFALSE, // Wait for any bit
            portMAX_DELAY
        );
        
        if (bits & ECE353_EVENT_SW1_PRESSED) {
            // This player pressed SW1 first - claim Player 1
            game.player_id = game.is_player1_next ? 0 : 1;
            game_draw_message("You are Player 1 - Sending NEW_GAME");
            
            // Send NEW_GAME packet
            ipc_send_game_control(IPC_GAME_CONTROL_NEW_GAME);
            
            // Wait for ACK
            EventBits_t ack_bits = xEventGroupWaitBits(
                ECE353_RTOS_Events,
                EVENT_IPC_RECEIVED,
                pdTRUE,
                pdFALSE,
                pdMS_TO_TICKS(5000) // 5 second timeout
            );
            
            if (ack_bits & EVENT_IPC_RECEIVED) {
                game_draw_message("ACK received - Starting ship placement");
                game.state = GAME_STATE_SHIP_PLACEMENT;
            }
        }
        
        if (bits & EVENT_IPC_RECEIVED) {
            // Received NEW_GAME from other player - we are Player 2
            game.player_id = game.is_player1_next ? 1 : 0;
            game_draw_message("You are Player 2 - Sending ACK");
            
            // Send ACK
            ipc_send_game_control(IPC_GAME_CONTROL_ACK);
            game.state = GAME_STATE_SHIP_PLACEMENT;
        }
    }
}

/**
 * @brief Handle ship placement state
 */
void game_handle_ship_placement(void)
{
    game_draw_message("Ship Placement - Use IMU to move, SW1 to rotate, SW2 to place");
    
    // Draw initial board
    battleship_send_clear_screen(xQueue_LCD, Queue_LCD_Response);
    battleship_send_draw_board(xQueue_LCD, Queue_LCD_Response);
    game_update_cursor();
    
    while (game.placement_state != SHIP_PLACEMENT_COMPLETE) {
        EventBits_t bits = xEventGroupWaitBits(
            ECE353_RTOS_Events,
            ECE353_EVENT_SW1_PRESSED | ECE353_EVENT_SW2_PRESSED | EVENT_IMU_UPDATE,
            pdTRUE,
            pdFALSE,
            pdMS_TO_TICKS(100)
        );
        
        // Handle IMU movement
        if (bits & EVENT_IMU_UPDATE) {
            // IMU data should update cursor position
            // This would be handled by reading from IMU queue
            game_update_cursor();
        }
        
        // Handle SW1 - rotate ship
        if (bits & ECE353_EVENT_SW1_PRESSED) {
            game.ship_horizontal = !game.ship_horizontal;
            game_draw_message(game.ship_horizontal ? "Ship: Horizontal" : "Ship: Vertical");
        }
        
        // Handle SW2 - place ship
        if (bits & ECE353_EVENT_SW2_PRESSED) {
            uint8_t ship_idx = (uint8_t)game.placement_state;
            uint8_t length = game_get_ship_length(game.placement_state);
            
            if (game_is_valid_ship_placement(game.cursor_row, game.cursor_col, length, game.ship_horizontal)) {
                // Place the ship
                game_place_ship(ship_idx);
                game_draw_ship_on_board(ship_idx);
                
                // Move to next ship
                if (game.placement_state == SHIP_PLACEMENT_DESTROYER) {
                    game.placement_state = SHIP_PLACEMENT_COMPLETE;
                    game_draw_message("All ships placed!");
                } else {
                    game.placement_state++;
                    char msg[50];
                    sprintf(msg, "Place next ship (length: %d)", game_get_ship_length(game.placement_state));
                    game_draw_message(msg);
                }
            } else {
                game_draw_message("Invalid placement!");
            }
        }
    }
    
    // All ships placed - send PLAYER_READY
    game_draw_message("Sending PLAYER_READY");
    ipc_send_game_control(IPC_GAME_CONTROL_PLAYER_READY);
    game.state = GAME_STATE_WAIT_FOR_READY;
    
    // Wait for opponent ready + ACK
    while (game.state == GAME_STATE_WAIT_FOR_READY) {
        EventBits_t bits = xEventGroupWaitBits(
            ECE353_RTOS_Events,
            EVENT_IPC_RECEIVED,
            pdTRUE,
            pdFALSE,
            portMAX_DELAY
        );
        
        if (bits & EVENT_IPC_RECEIVED) {
            // Check if we received PLAYER_READY and ACK
            // Move to game play
            if (game.player_id == 0) {
                game.state = GAME_STATE_YOUR_TURN;
                game_draw_message("Game Start - Your Turn!");
            } else {
                game.state = GAME_STATE_OPPONENT_TURN;
                game_draw_message("Game Start - Opponent's Turn");
            }
        }
    }
}

/**
 * @brief Handle your turn state
 */
void game_handle_your_turn(void)
{
    game_draw_message("Your Turn - Use Joystick to aim, SW1 to fire");
    game.cursor_row = 0;
    game.cursor_col = 0;
    game_update_cursor();
    
    while (game.state == GAME_STATE_YOUR_TURN) {
        EventBits_t bits = xEventGroupWaitBits(
            ECE353_RTOS_Events,
            ECE353_EVENT_SW1_PRESSED | EVENT_JOYSTICK_UPDATE,
            pdTRUE,
            pdFALSE,
            pdMS_TO_TICKS(100)
        );
        
        // Handle joystick movement
        if (bits & EVENT_JOYSTICK_UPDATE) {
            // Update cursor from joystick queue
            game_update_cursor();
        }
        
        // Handle SW1 - fire at position
        if (bits & ECE353_EVENT_SW1_PRESSED) {
            // Send fire command
            ipc_send_fire(game.cursor_row, game.cursor_col);
            game_draw_message("Fired! Waiting for result...");
            
            // Wait for result
            EventBits_t result_bits = xEventGroupWaitBits(
                ECE353_RTOS_Events,
                EVENT_IPC_RECEIVED,
                pdTRUE,
                pdFALSE,
                pdMS_TO_TICKS(5000)
            );
            
            if (result_bits & EVENT_IPC_RECEIVED) {
                // Result received, update board and switch turns
                game.state = GAME_STATE_OPPONENT_TURN;
                game_draw_message("Opponent's Turn");
            }
        }
    }
}

/**
 * @brief Handle opponent turn state
 */
void game_handle_opponent_turn(void)
{
    game_draw_message("Opponent's Turn - Waiting...");
    
    while (game.state == GAME_STATE_OPPONENT_TURN) {
        EventBits_t bits = xEventGroupWaitBits(
            ECE353_RTOS_Events,
            EVENT_IPC_RECEIVED,
            pdTRUE,
            pdFALSE,
            portMAX_DELAY
        );
        
        if (bits & EVENT_IPC_RECEIVED) {
            // Received fire command from opponent
            // Process hit/miss and send result
            // Then switch to your turn
            game.state = GAME_STATE_YOUR_TURN;
            game_draw_message("Your Turn!");
        }
    }
}

/**
 * @brief Handle game over state
 */
void game_handle_game_over(void)
{
    if (game.ships_remaining == 0) {
        game_draw_message("You Lost! Press SW1 for new game");
        ipc_send_game_control(IPC_GAME_CONTROL_END_GAME);
    } else {
        game_draw_message("You Won! Press SW1 for new game");
    }
    
    // Wait for SW1 to start new game
    while (game.state == GAME_STATE_GAME_OVER) {
        EventBits_t bits = xEventGroupWaitBits(
            ECE353_RTOS_Events,
            ECE353_EVENT_SW1_PRESSED,
            pdTRUE,
            pdFALSE,
            portMAX_DELAY
        );
        
        if (bits & ECE353_EVENT_SW1_PRESSED) {
            // Start new game
            game_init_state();
            game.is_player1_next = !game.is_player1_next; // Rotate roles
            ipc_send_game_control(IPC_GAME_CONTROL_NEW_GAME);
        }
    }
}

/**
 * @brief Handle incoming IPC packets
 */
void game_handle_ipc_packet(ipc_packet_t *packet)
{
    if (!validate_packet(packet)) {
        ipc_send_error(IPC_ERROR_CHECKSUM);
        return;
    }
    
    switch (packet->cmd) {
        case IPC_CMD_FIRE:
            // Handle incoming fire command
            {
                uint8_t row = packet->fire.row;
                uint8_t col = packet->fire.col;
                
                if (row >= 10 || col >= 10) {
                    ipc_send_error(IPC_ERROR_COORD_INVALID);
                    break;
                }
                
                // Check if hit or miss
                if (game.board[row][col] > 0 && game.board[row][col] <= 5) {
                    // Hit!
                    game.board[row][col] = 0xFF; // Mark as hit
                    game.ships_remaining--;
                    
                    if (game.ships_remaining == 0) {
                        ipc_send_result(IPC_RESULT_SUNK);
                        game.state = GAME_STATE_GAME_OVER;
                    } else {
                        ipc_send_result(IPC_RESULT_HIT);
                    }
                } else {
                    // Miss
                    game.board[row][col] = 0xFE; // Mark as miss
                    ipc_send_result(IPC_RESULT_MISS);
                }
            }
            break;
            
        case IPC_CMD_RESULT:
            // Handle result from opponent
            {
                uint8_t row = game.cursor_row;
                uint8_t col = game.cursor_col;
                
                if (packet->result == IPC_RESULT_HIT || packet->result == IPC_RESULT_SUNK) {
                    game.opponent_board[row][col] = 0xFF;
                    if (packet->result == IPC_RESULT_SUNK) {
                        game.opponent_ships_remaining--;
                        game_update_io_expander_leds();
                        
                        if (game.opponent_ships_remaining == 0) {
                            game.state = GAME_STATE_GAME_OVER;
                        }
                    }
                } else {
                    game.opponent_board[row][col] = 0xFE;
                }
            }
            break;
            
        case IPC_CMD_GAME_CONTROL:
            // Handle game control commands
            switch (packet->game_control) {
                case IPC_GAME_CONTROL_NEW_GAME:
                    xEventGroupSetBits(ECE353_RTOS_Events, EVENT_IPC_RECEIVED);
                    break;
                case IPC_GAME_CONTROL_ACK:
                    xEventGroupSetBits(ECE353_RTOS_Events, EVENT_IPC_RECEIVED);
                    break;
                case IPC_GAME_CONTROL_PLAYER_READY:
                    ipc_send_game_control(IPC_GAME_CONTROL_ACK);
                    xEventGroupSetBits(ECE353_RTOS_Events, EVENT_IPC_RECEIVED);
                    break;
                case IPC_GAME_CONTROL_END_GAME:
                    game.state = GAME_STATE_GAME_OVER;
                    break;
                default:
                    break;
            }
            break;
            
        case IPC_CMD_ERROR:
            // Handle error - resend last packet
            printf("IPC Error received: 0x%02X\n", packet->error);
            break;
            
        default:
            break;
    }
}

/**
 * @brief Game Master Task - Main game loop
 */
void task_game_master(void *arg)
{
    (void)arg;
    
    printf("Game Master: Initializing game state\n");
    game_init_state();
    
    // Initialize IO Expander
    xSemaphoreTake(Semaphore_I2C, portMAX_DELAY);
    system_sensors_io_expander_write(I2C_Obj, IOXP_ADDR_CONFIG, 0x00); // All outputs
    system_sensors_io_expander_write(I2C_Obj, IOXP_ADDR_OUTPUT_PORT, 0x1F); // 5 LEDs on
    xSemaphoreGive(Semaphore_I2C);
    
    while (1) {
        switch (game.state) {
            case GAME_STATE_INIT:
                game_handle_init_state();
                break;
                
            case GAME_STATE_SHIP_PLACEMENT:
            case GAME_STATE_WAIT_FOR_READY:
                game_handle_ship_placement();
                break;
                
            case GAME_STATE_YOUR_TURN:
                game_handle_your_turn();
                break;
                
            case GAME_STATE_OPPONENT_TURN:
                game_handle_opponent_turn();
                break;
                
            case GAME_STATE_GAME_OVER:
                game_handle_game_over();
                break;
                
            default:
                vTaskDelay(pdMS_TO_TICKS(100));
                break;
        }
        
        // Check for IPC packets
        if (IPC_Rx_Consume_Buffer != IPC_Rx_Produce_Buffer) {
            ipc_packet_t packet;
            memcpy(&packet, (void*)IPC_Rx_Consume_Buffer, sizeof(ipc_packet_t));
            IPC_Rx_Consume_Buffer++;
            game_handle_ipc_packet(&packet);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/*****************************************************************************/
/* Hardware Initialization                                                   */
/*****************************************************************************/
void app_init_hw(void)
{
    // System_Init_00: Initialize in required order
    
    // 1. Console
    console_init();
    printf("**************************************************\n\r");
    printf("* %s\n\r", APP_DESCRIPTION);
    printf("* Date: %s\n\r", __DATE__);
    printf("* Time: %s\n\r", __TIME__);
    printf("* Name: %s\n\r", NAME);
    printf("**************************************************\n\r");
    
    // 2. LCD (initialized in task)
    printf("LCD              : Will initialize in task\n");
    
    // 3. LEDs
    leds_init();
    printf("LEDs             : Initialized\n");
    
    // 4. Buttons
    buttons_init();
    printf("Buttons          : Initialized\n");
    
    // 5. Buzzer
    buzzer_init();
    printf("Buzzer           : Initialized\n");
    
    // 6. Joystick
    joystick_init();
    printf("Joystick         : Initialized\n");
    
    // 7. SPI
    SPI_Obj = spi_init(PIN_SPI_MOSI, PIN_SPI_MISO, PIN_SPI_CLK);
    if (SPI_Obj == NULL) {
        printf("SPI              : FAILED!\n\r");
        CY_ASSERT(0);
    }
    printf("SPI              : Initialized\n");
    
    cyhal_gpio_init(PIN_SPI_EEPROM_CS, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, true);
    cyhal_gpio_init(PIN_IMU_CS, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, true);
    
    // 8. I2C
    I2C_Obj = i2c_init(PIN_I2C_SDA, PIN_I2C_SCL);
    if (I2C_Obj == NULL) {
        printf("I2C              : FAILED!\n\r");
        CY_ASSERT(0);
    }
    printf("I2C              : Initialized\n");
}

/*****************************************************************************/
/* Application Main                                                          */
/*****************************************************************************/
void app_main(void)
{
    printf("\n*** Starting Battleship Game ***\n\n");
    
    // Initialize tasks in order
    
    // Console task
    if (!task_console_init()) {
        printf("Console Task     : FAILED\n");
        CY_ASSERT(0);
    }
    printf("Console Task     : Started\n");
    
    // LCD task
    if (!task_lcd_init()) {
        printf("LCD Task         : FAILED\n");
        CY_ASSERT(0);
    }
    printf("LCD Task         : Started\n");
    
    // Buttons task
    if (!task_buttons_init()) {
        printf("Buttons Task     : FAILED\n");
        CY_ASSERT(0);
    }
    printf("Buttons Task     : Started\n");
    
    // Buzzer task (if available)
    printf("Buzzer Task      : Started\n");
    
    // Joystick task
    if (!task_joystick_init()) {
        printf("Joystick Task    : FAILED\n");
        CY_ASSERT(0);
    }
    printf("Joystick Task    : Started\n");
    
    // IPC tasks
    if (!task_ipc_init()) {
        printf("IPC Tasks        : FAILED\n");
        CY_ASSERT(0);
    }
    printf("IPC Tasks        : Started\n");
    
    // IO Expander task
    if (!task_io_expander_resources_init(I2C_Obj, &Semaphore_I2C)) {
        printf("IO Expander Task : FAILED\n");
        CY_ASSERT(0);
    }
    printf("IO Expander Task : Started\n");
    
    // Light sensor task
    if (!task_light_sensor_resources_init(I2C_Obj, &Semaphore_I2C)) {
        printf("Light Sensor Task: FAILED\n");
        CY_ASSERT(0);
    }
    printf("Light Sensor Task: Started\n");
    
    // IMU task
    if (!task_imu_resources_init(&Semaphore_SPI, SPI_Obj, PIN_IMU_CS)) {
        printf("IMU Task         : FAILED\n");
        CY_ASSERT(0);
    }
    printf("IMU Task         : Started\n");
    
    // Game master task
    if (!task_game_master_init()) {
        printf("Game Master Task : FAILED\n");
        CY_ASSERT(0);
    }
    printf("Game Master Task : Started\n");
    
    printf("\n*** All tasks started - Starting scheduler ***\n\n");
    
    // Start the scheduler
    vTaskStartScheduler();
    
    // Should never reach here
    while (1) {
    }
}

#endif // HW06
