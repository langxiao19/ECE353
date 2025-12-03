# ECE353 HW06 - Battleship Game Implementation

## Overview
This is a complete implementation of a two-player Battleship game on embedded hardware using FreeRTOS. The game runs on two boards that communicate via UART using the IPC (Inter-Processor Communication) protocol.

## Project Structure

```
src/homework/hw06/
└── hw06.c              - Main game logic and state machine

Related files:
├── src/tasks/
│   ├── battleship.c    - LCD rendering functions for game board
│   ├── battleship.h    - Game board definitions
│   ├── task_ipc.c/h    - IPC communication tasks
│   ├── task_lcd.c/h    - LCD gatekeeper task
│   ├── task_buttons.c/h - Button handling task
│   ├── task_joystick.c/h - Joystick input task
│   ├── task_imu.c/h    - IMU (accelerometer) task
│   └── task_io_expander.c/h - LED control task
└── main.h              - Enable HW06 with #define HW06
```

## Requirements Implementation

### System Initialization (System_Init_00 - System_Init_08)

**System_Init_00**: Hardware initialization order implemented in `app_init_hw()`:
1. ✅ Console
2. ✅ LCD
3. ✅ LEDs
4. ✅ Buttons
5. ✅ Buzzer
6. ✅ Joystick
7. ✅ SPI
8. ✅ I2C

**System_Init_01**: LCD starts with empty board showing hits/misses on right side
- Implemented in `battleship_draw_game_board()`

**System_Init_02**: "Press SW1 to Start" message displayed
- Implemented in `game_handle_init_state()`

**System_Init_03**: Player role assignment via SW1
- First player to press SW1 becomes Player 1
- Implemented in `game_handle_init_state()`

**System_Init_04**: Player 1 sends `IPC_GAME_CONTROL_NEW_GAME`
- Implemented using `ipc_send_game_control()`

**System_Init_05**: Player 2 responds with `IPC_GAME_CONTROL_ACK`
- Handled in `game_handle_ipc_packet()`

**System_Init_06-08**: Ship placement and game start handshake
- Implemented in `game_handle_ship_placement()`

### Player Roles (Player_Roles_00 - Player_Roles_03)

**Player_Roles_00-01**: Role rotation and first player selection
- `game.is_player1_next` tracks role for next game
- Toggles in `game_handle_game_over()` for new games

**Player_Roles_02**: Color coding
- Player 1: Blue (`BATTLESHIP_PLAYER_0_COLOR`)
- Player 2: Red (`BATTLESHIP_PLAYER_1_COLOR`)

**Player_Roles_03**: LCD turn indicator
- "Current Move: Yours/Opponent" messages in each state handler

### Input Control (Input_Control_00 - Input_Control_05)

**Input_Control_00**: IMU tilt for ship movement
- Event `EVENT_IMU_UPDATE` processed in `game_handle_ship_placement()`
- Updates `game.cursor_row` and `game.cursor_col`

**Input_Control_01**: SW1 rotates ships
- Toggles `game.ship_horizontal` flag

**Input_Control_02**: SW2 confirms placement
- Validates and places ship via `game_place_ship()`

**Input_Control_03**: Light sensor for dark/light mode
- `game.dark_mode` flag set by light sensor task
- Can be used to adjust LCD colors

**Input_Control_04**: IOEXP LEDs show opponent ships remaining
- `game_update_io_expander_leds()` updates 5 LEDs
- One LED per opponent ship

**Input_Control_05**: Joystick for attack targeting
- Event `EVENT_JOYSTICK_UPDATE` in `game_handle_your_turn()`
- SW1 fires at selected location

### Communication Flow (Communication_Flow_00 - Communication_Flow_07)

**Communication_Flow_00**: IPC packet structure
- Uses `ipc_packet_t` from `task_ipc.h`

**Communication_Flow_01**: Checksum validation
- `validate_packet()` checks integrity

**Communication_Flow_02**: Error handling and resend
- `IPC_CMD_ERROR` with `IPC_ERROR_CHECKSUM`
- Sender can detect and resend

**Communication_Flow_03**: ACK packets
- `IPC_GAME_CONTROL_ACK` confirms receipt

**Communication_Flow_04**: Turn changes in move confirmation
- Result packet (`IPC_CMD_RESULT`) signals turn change

**Communication_Flow_05**: End game notification
- Losing player sends `IPC_GAME_CONTROL_END_GAME`

**Communication_Flow_06-07**: New game start
- SW1 sends `IPC_GAME_CONTROL_NEW_GAME`
- Returns to ship placement state

## Game States

The game uses a finite state machine with these states:

1. **GAME_STATE_INIT**: Waiting for player selection
   - Display "Press SW1 to Start"
   - First SW1 press → Player 1, sends NEW_GAME
   - Receiving NEW_GAME → Player 2, sends ACK

2. **GAME_STATE_SHIP_PLACEMENT**: Place 5 ships
   - Ships: Carrier(5), Battleship(4), Cruiser(3), Submarine(3), Destroyer(2)
   - IMU tilts to move cursor
   - SW1 rotates ship orientation
   - SW2 places ship (if valid)

3. **GAME_STATE_WAIT_FOR_READY**: Synchronization
   - Wait for both players to send PLAYER_READY
   - Exchange ACK packets

4. **GAME_STATE_YOUR_TURN**: Attack phase
   - Joystick moves cursor on opponent board
   - SW1 fires at location
   - Wait for HIT/MISS/SUNK result

5. **GAME_STATE_OPPONENT_TURN**: Defense phase
   - Wait for opponent fire command
   - Check own board for hit/miss
   - Send result back
   - Update board display

6. **GAME_STATE_GAME_OVER**: End game
   - Display winner/loser message
   - SW1 starts new game with rotated roles

## Key Data Structures

### Game Context
```c
typedef struct {
    game_state_t state;
    uint8_t player_id;              // 0 or 1
    bool is_player1_next;           // Role rotation
    ship_placement_state_t placement_state;
    ship_t ships[5];                // All ships
    uint8_t board[10][10];          // Own board
    uint8_t opponent_board[10][10]; // Opponent tracking
    uint8_t cursor_row, cursor_col;
    bool ship_horizontal;
    uint8_t ships_remaining;
    uint8_t opponent_ships_remaining;
    bool dark_mode;
} game_context_t;
```

### Ship Structure
```c
typedef struct {
    uint8_t row, col;
    uint8_t length;
    bool horizontal;
    bool placed;
} ship_t;
```

## IPC Packet Types

### Commands
- `IPC_CMD_FIRE`: Attack coordinates
- `IPC_CMD_RESULT`: Hit/Miss/Sunk response
- `IPC_CMD_GAME_CONTROL`: Game state control
- `IPC_CMD_ERROR`: Error notifications

### Game Control Sub-commands
- `IPC_GAME_CONTROL_NEW_GAME`: Start new game
- `IPC_GAME_CONTROL_PLAYER_READY`: Ship placement done
- `IPC_GAME_CONTROL_ACK`: Acknowledge receipt
- `IPC_GAME_CONTROL_END_GAME`: Game over notification

### Results
- `IPC_RESULT_MISS`: Attack missed
- `IPC_RESULT_HIT`: Ship hit
- `IPC_RESULT_SUNK`: All ships destroyed

## Event Management

Uses FreeRTOS event groups (`ECE353_RTOS_Events`):
- `ECE353_EVENT_SW1_PRESSED`: Button 1 press
- `ECE353_EVENT_SW2_PRESSED`: Button 2 press
- `EVENT_IPC_RECEIVED`: IPC packet received
- `EVENT_IMU_UPDATE`: IMU position update
- `EVENT_JOYSTICK_UPDATE`: Joystick position update

## Task Synchronization

### Semaphores
- `Semaphore_I2C`: Protects I2C bus access
- `Semaphore_SPI`: Protects SPI bus access

### Queues
- `xQueue_LCD`: LCD gatekeeper request queue
- `Queue_LCD_Response`: LCD gatekeeper response queue
- `Queue_IPC_Tx`: IPC transmission queue
- `Queue_Joystick`: Joystick position data

## Hardware Peripherals

### Inputs
- **SW1**: Rotate ship (placement) / Fire (attack) / New game
- **SW2**: Confirm ship placement
- **IMU (ICM-20948)**: Tilt controls for cursor movement
- **Joystick**: Attack cursor positioning
- **Light Sensor (TSL2572)**: Display theme control

### Outputs
- **LCD (ILI9341)**: Game board display
- **IO Expander LEDs**: Opponent ships remaining
- **Buzzer**: Sound effects (optional)
- **RGB LEDs**: Status indicators

### Communication
- **UART**: IPC between two boards

## LCD Gatekeeper Pattern

All LCD operations go through the LCD gatekeeper task:
```c
battleship_send_clear_screen(xQueue_LCD, Queue_LCD_Response);
battleship_send_draw_board(xQueue_LCD, Queue_LCD_Response);
battleship_send_draw_tile(xQueue_LCD, Queue_LCD_Response, row, col, border, fill);
battleship_send_draw_ship(xQueue_LCD, Queue_LCD_Response, row, col, type, horizontal);
```

Benefits:
- Thread-safe LCD access
- No race conditions
- Clean separation of concerns

## Board Layout

```
10x10 Grid (200x200 pixels)
┌─────────────────────────┐
│ [Game Board]  [Stats]   │
│  20px boxes   - Hits    │
│  Blue/Red     - Misses  │
│  borders      - Ships   │
│               remaining │
└─────────────────────────┘
```

Each cell: 20x20 pixels
- Border: 4 pixels (player color)
- Fill: 16x16 pixels (black/hit/miss)

## Building and Running

### 1. Enable HW06
Edit `main.h`:
```c
//#define HW05
#define HW06
```

### 2. Build
Use VS Code task or command line:
```bash
make build
```

### 3. Program Both Boards
```bash
make program
```

### 4. Connect Boards
Connect UART TX/RX between boards:
- Board1 TX → Board2 RX
- Board1 RX → Board2 TX
- Common GND

### 5. Play!
1. Power on both boards
2. First player presses SW1 to start
3. Place ships using IMU tilt, SW1 (rotate), SW2 (place)
4. Take turns attacking
5. First to sink all opponent ships wins!

## Demonstration Checklist

### 1. Player Setup ✓
- Two boards powered and connected
- Both displays showing "Press SW1 to Start"
- Player roles assigned correctly

### 2. Ship Placement ✓
- All 5 ships (5,4,3,3,2 lengths) placed
- IMU tilt moves cursor
- SW1 rotates orientation
- SW2 places ship
- Invalid placements rejected
- Ships displayed on board

### 3. Gameplay ✓
- Turn alternation works
- Joystick selects target
- SW1 fires
- Hit/miss displayed correctly
- IOEXP LEDs show opponent ships remaining
- Communication packets exchanged properly

### 4. Game Completion ✓
- Game ends when all ships sunk
- Winner/loser displayed correctly
- Both boards synchronized

### 5. Restart Functionality ✓
- SW1 starts new game
- Boards reset properly
- Ships can be re-placed
- Roles rotate (Player 1 ↔ Player 2)

## Troubleshooting

### No Communication
- Check UART connections (TX↔RX)
- Verify baud rate matches (115200)
- Check packet start byte (0xAA)

### LCD Not Updating
- Check LCD gatekeeper task running
- Verify SPI initialization
- Check response queue

### Buttons Not Working
- Verify button task initialized
- Check event group bits
- Ensure interrupts enabled

### IMU Not Responding
- Check SPI chip select
- Verify IMU initialization
- Check SPI clock/mode settings

## Future Enhancements

1. **AI Opponent**: Single-player mode against computer
2. **Sound Effects**: Buzzer tones for hit/miss/sunk
3. **Animations**: Ship sinking animations
4. **Statistics**: Track wins/losses in EEPROM
5. **Difficulty Levels**: Ship count variations
6. **Network Play**: Multiple board network
7. **Spectator Mode**: Third board shows both boards

## Credits

- **Author**: Lingxiao Xiao
- **Course**: ECE 353 - Embedded Systems
- **Instructor**: Prof. Joe Krachey
- **Date**: December 2, 2025

## License

Copyright (c) 2025 - For educational use only.
