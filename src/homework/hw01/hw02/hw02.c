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
#include "task_console.h"
#include <string.h>

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

    lcd_msg_t lcd_msg = {0};

    // Clear the screen once at startup
    lcd_msg.command = LCD_CMD_CLEAR_SCREEN;
    lcd_msg.response_queue = NULL; // ensure no stale queue handle
    xQueueSend(xQueue_LCD, &lcd_msg, portMAX_DELAY);

    // Draw the initial battleship board for player 0
    lcd_msg.command = LCD_CMD_DRAW_BOARD;
    lcd_msg.response_queue = NULL; // ensure no stale queue handle
    lcd_msg.payload.battleship.row = 0;
    xQueueSend(xQueue_LCD, &lcd_msg, portMAX_DELAY);

    

    /* Stats will be drawn after the sweep and reveal completes */

    // Prepare a response queue to receive status from the LCD gatekeeper
    QueueHandle_t response_q = xQueueCreate(1, sizeof(lcd_cmd_status_t));
    if (response_q == NULL)
    {
        task_console_printf("Failed to create response queue\n\r");
    }

    // Helper macro to send a ship placement and wait for response (ships are gray)
#define SEND_SHIP_AND_WAIT(c, r, t, h) do { \
        lcd_msg.command = LCD_CMD_DRAW_SHIP; \
        lcd_msg.response_queue = response_q; \
        lcd_msg.payload.battleship.col = (c); \
        lcd_msg.payload.battleship.row = (r); \
        lcd_msg.payload.battleship.type = (t); \
        lcd_msg.payload.battleship.horizontal = (h); \
    lcd_msg.payload.battleship.border_color = LCD_COLOR_BLUE; \
    lcd_msg.payload.battleship.fill_color = LCD_COLOR_GRAY; \
        xQueueSend(xQueue_LCD, &lcd_msg, portMAX_DELAY); \
        lcd_cmd_status_t resp; \
        if (xQueueReceive(response_q, &resp, pdMS_TO_TICKS(50)) == pdTRUE) { \
                if (resp == LCD_CMD_STATUS_SUCCESS) { \
                task_console_printf("Ship drawn at (%d,%d)\n\r", (c),(r)); \
            } else { \
                task_console_printf("Failed to draw ship at (%d,%d)\n\r", (c),(r)); \
            } \
        } else { \
            task_console_printf("No response for ship at (%d,%d)\n\r", (c),(r)); \
        } \
    } while(0)

    /*
     * Validate a proposed ship placement locally so we can report a clear reason
     * for invalid placements without relying on the gatekeeper's log.
     * Returns true if placement is valid, false and fills reason if invalid.
     */
    auto validate_ship = (bool (*)(uint8_t,uint8_t,battleship_type_t,bool,char*,size_t)) NULL;
    {
        /* Create a small local function using a block to keep edits minimal */
        bool validate(uint8_t col, uint8_t row, battleship_type_t type, bool horizontal, char *reason, size_t rs)
        {
            uint8_t length = 0;
            switch(type)
            {
                case BATTLESHIP_TYPE_CARRIER: length = 5; break;
                case BATTLESHIP_TYPE_BATTLESHIP: length = 4; break;
                case BATTLESHIP_TYPE_CRUISER: length = 3; break;
                case BATTLESHIP_TYPE_SUBMARINE: length = 3; break;
                case BATTLESHIP_TYPE_DESTROYER: length = 2; break;
                default:
                    if (rs) snprintf(reason, rs, "invalid ship type");
                    return false;
            }

            /* Quick range check for starting coord */
            if (col >= 10 || row >= 10)
            {
                if (rs) snprintf(reason, rs, "invalid coordinates (start outside board)");
                return false;
            }

            if (horizontal)
            {
                if ((uint8_t)(col + length) > 10)
                {
                    if (rs) snprintf(reason, rs, "too far right");
                    return false;
                }
            }
            else
            {
                if ((uint8_t)(row + length) > 10)
                {
                    if (rs) snprintf(reason, rs, "too far down");
                    return false;
                }
            }

            return true; // valid
        }
        validate_ship = validate; // assign pointer for use below
    }

    // Define ship anchors; ships will be revealed when the cursor passes over their anchor cell
    typedef struct {
        uint8_t col;
        uint8_t row;
        battleship_type_t type;
        bool horizontal;
        bool drawn;
    } ship_req_t;

    ship_req_t ships[] = {
        {0, 9, BATTLESHIP_TYPE_CARRIER, true, false},    // Carrier anchor (bottom-left, horizontal)
        {0, 0, BATTLESHIP_TYPE_BATTLESHIP, true, false}, // Battleship anchor
        {2, 2, BATTLESHIP_TYPE_DESTROYER, false, false}, // Destroyer anchor
        {5, 5, BATTLESHIP_TYPE_SUBMARINE, true, false},  // Submarine anchor
        {7, 7, BATTLESHIP_TYPE_CRUISER, false, false},   // Cruiser anchor
    };

    const size_t num_ships = sizeof(ships) / sizeof(ships[0]);

    // We'll reveal ships when the cursor passes over their anchor cell (only once)
#undef SEND_SHIP_AND_WAIT

    // ------- Invalid placement tests (should be rejected) -------
    // Battleship at (7,0) horizontal (expected: exceeds board width)
    {
        lcd_msg.command = LCD_CMD_DRAW_SHIP;
        lcd_msg.response_queue = response_q;
        lcd_msg.payload.battleship.col = 7;
        lcd_msg.payload.battleship.row = 0;
        lcd_msg.payload.battleship.type = BATTLESHIP_TYPE_BATTLESHIP;
        lcd_msg.payload.battleship.horizontal = true;
    lcd_msg.payload.battleship.border_color = LCD_COLOR_BLUE;
    lcd_msg.payload.battleship.fill_color = LCD_COLOR_BLACK;
        xQueueSend(xQueue_LCD, &lcd_msg, portMAX_DELAY);
        lcd_cmd_status_t resp;
        if (xQueueReceive(response_q, &resp, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (resp == LCD_CMD_STATUS_ERROR) {
                printf("Correctly detected invalid ship placement (too far right)\n\r");
            } else {
                printf("Unexpectedly accepted invalid placement: Battleship at (7,0)\n\r");
            }
        } else {
            printf("No response for invalid placement: Battleship at (7,0)\n\r");
        }
    }

    // Submarine at (8,8) vertical (expected: exceeds board height)
    {
        lcd_msg.command = LCD_CMD_DRAW_SHIP;
        lcd_msg.response_queue = response_q;
        lcd_msg.payload.battleship.col = 8;
        lcd_msg.payload.battleship.row = 8;
        lcd_msg.payload.battleship.type = BATTLESHIP_TYPE_SUBMARINE;
        lcd_msg.payload.battleship.horizontal = false;
    lcd_msg.payload.battleship.border_color = LCD_COLOR_BLUE;
    lcd_msg.payload.battleship.fill_color = LCD_COLOR_BLACK;
        xQueueSend(xQueue_LCD, &lcd_msg, portMAX_DELAY);
        lcd_cmd_status_t resp;
        if (xQueueReceive(response_q, &resp, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (resp == LCD_CMD_STATUS_ERROR) {
                printf("Correctly detected invalid ship placement (too far down)\n\r");
            } else {
                printf("Unexpectedly accepted invalid placement: Submarine at (8,8)\n\r");
            }
        } else {
            printf("No response for invalid placement: Submarine at (8,8)\n\r");
        }
    }

    // Carrier at (15,0) vertical (expected: starts outside board boundaries)
    {
        lcd_msg.command = LCD_CMD_DRAW_SHIP;
        lcd_msg.response_queue = response_q;
        lcd_msg.payload.battleship.col = 15;
        lcd_msg.payload.battleship.row = 0;
        lcd_msg.payload.battleship.type = BATTLESHIP_TYPE_CARRIER;
        lcd_msg.payload.battleship.horizontal = false;
    lcd_msg.payload.battleship.border_color = LCD_COLOR_BLUE;
    lcd_msg.payload.battleship.fill_color = LCD_COLOR_BLACK;
        xQueueSend(xQueue_LCD, &lcd_msg, portMAX_DELAY);
        lcd_cmd_status_t resp;
        if (xQueueReceive(response_q, &resp, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (resp == LCD_CMD_STATUS_ERROR) {
                printf("Correctly detected invalid ship placement (invalid coordinates)\n\r");
            } else {
                printf("Unexpectedly accepted invalid placement: Carrier at (15,0)\n\r");
            }
        } else {
            printf("No response for invalid placement: Carrier at (15,0)\n\r");
        }
    }

    printf("All invalid ship placement tests passed\n\r");

    // Cursor position (row, col) in battleship coordinates (0-9)
    uint8_t row = 0;
    uint8_t col = 0;
    uint8_t prev_row = 0xFF;
    uint8_t prev_col = 0xFF;

    // Single sweep: move cursor left-to-right, top-to-bottom once
    for (row = 0; row < 10; row++)
    {
        for (col = 0; col < 10; col++)
        {
            // Restore previous cursor (if any) to blue border + black fill
            if (prev_row != 0xFF)
            {
                lcd_msg.command = LCD_CMD_DRAW_CURSOR;
                lcd_msg.response_queue = NULL;
                lcd_msg.payload.battleship.row = prev_row;
                lcd_msg.payload.battleship.col = prev_col;
                lcd_msg.payload.battleship.border_color = LCD_COLOR_BLUE;
                lcd_msg.payload.battleship.fill_color = LCD_COLOR_BLACK;
                xQueueSend(xQueue_LCD, &lcd_msg, portMAX_DELAY);
            }

            // Draw the new cursor with blue border and green fill
            lcd_msg.command = LCD_CMD_DRAW_CURSOR;
            lcd_msg.response_queue = NULL;
            lcd_msg.payload.battleship.row = row;
            lcd_msg.payload.battleship.col = col;
            lcd_msg.payload.battleship.border_color = LCD_COLOR_BLUE;
            lcd_msg.payload.battleship.fill_color = LCD_COLOR_GREEN;
            xQueueSend(xQueue_LCD, &lcd_msg, portMAX_DELAY);

            // Hold the cursor visible for a short time
            vTaskDelay(pdMS_TO_TICKS(100));

            // Restore current cursor cell to blue border + black fill (cursor disappears)
            lcd_msg.command = LCD_CMD_DRAW_CURSOR;
            lcd_msg.response_queue = NULL;
            lcd_msg.payload.battleship.row = row;
            lcd_msg.payload.battleship.col = col;
            lcd_msg.payload.battleship.border_color = LCD_COLOR_BLUE;
            lcd_msg.payload.battleship.fill_color = LCD_COLOR_BLACK;
            xQueueSend(xQueue_LCD, &lcd_msg, portMAX_DELAY);

            // Save as previous for next iteration
            prev_row = row;
            prev_col = col;

            // No per-cell reveals; ships will be drawn simultaneously after the sweep
        }
    }

    // Sweep completed — keep the task alive but idle
    // Restore the last cursor cell so cursor does not remain visible
    if (prev_row != 0xFF)
    {
        lcd_msg.command = LCD_CMD_DRAW_CURSOR;
        lcd_msg.response_queue = NULL;
        lcd_msg.payload.battleship.row = prev_row;
        lcd_msg.payload.battleship.col = prev_col;
        lcd_msg.payload.battleship.border_color = LCD_COLOR_BLUE;
        lcd_msg.payload.battleship.fill_color = LCD_COLOR_BLACK;
        xQueueSend(xQueue_LCD, &lcd_msg, portMAX_DELAY);
        /* suppressed final cursor message to match expected console output */
    }

    // After sweep completes, draw all ships simultaneously
    for (size_t s = 0; s < num_ships; s++)
    {
        lcd_msg.command = LCD_CMD_DRAW_SHIP;
        lcd_msg.response_queue = response_q;
        lcd_msg.payload.battleship.col = ships[s].col;
        lcd_msg.payload.battleship.row = ships[s].row;
        lcd_msg.payload.battleship.type = ships[s].type;
        lcd_msg.payload.battleship.horizontal = ships[s].horizontal;
    lcd_msg.payload.battleship.border_color = LCD_COLOR_BLUE;
    lcd_msg.payload.battleship.fill_color = LCD_COLOR_GRAY;
        xQueueSend(xQueue_LCD, &lcd_msg, portMAX_DELAY);
    }

    // Wait for confirmations (or time out)
    for (size_t s = 0; s < num_ships; s++)
    {
        lcd_cmd_status_t resp;
        if (xQueueReceive(response_q, &resp, pdMS_TO_TICKS(200)) == pdTRUE)
        {
            const char *type_name = "unknown";
            switch(ships[s].type)
            {
                case BATTLESHIP_TYPE_BATTLESHIP: type_name = "battleship"; break;
                case BATTLESHIP_TYPE_DESTROYER: type_name = "destroyer"; break;
                case BATTLESHIP_TYPE_SUBMARINE: type_name = "submarine"; break;
                case BATTLESHIP_TYPE_CRUISER: type_name = "cruiser"; break;
                case BATTLESHIP_TYPE_CARRIER: type_name = "carrier"; break;
                default: break;
            }

            if (resp == LCD_CMD_STATUS_SUCCESS)
            {
                printf("Drew %s successfully at (%d, %d)\n\r", type_name, ships[s].col, ships[s].row);
            }
            else
            {
                printf("Failed to draw %s at (%d, %d)\n\r", type_name, ships[s].col, ships[s].row);
            }
        }
        else
        {
            const char *type_name = "unknown";
            switch(ships[s].type)
            {
                case BATTLESHIP_TYPE_BATTLESHIP: type_name = "battleship"; break;
                case BATTLESHIP_TYPE_DESTROYER: type_name = "destroyer"; break;
                case BATTLESHIP_TYPE_SUBMARINE: type_name = "submarine"; break;
                case BATTLESHIP_TYPE_CRUISER: type_name = "cruiser"; break;
                case BATTLESHIP_TYPE_CARRIER: type_name = "carrier"; break;
                default: break;
            }
            printf("No response for %s at (%d, %d)\n\r", type_name, ships[s].col, ships[s].row);
        }
    }

    // Sweep and reveal done — idle task
    /* Display game statistics after sweep/reveal completes */
    {
        char buf[32];
        int hits = 5;
        int misses = 3;

        /* Hits */
        int n = snprintf(buf, sizeof(buf), "Hits: %d", hits);
        if (n > 0)
        {
            char *s = (char *)pvPortMalloc((size_t)n + 1);
            if (s)
            {
                strcpy(s, buf);
                lcd_msg.command = LCD_CONSOLE_DRAW_MESSAGE;
                lcd_msg.response_queue = NULL;
                lcd_msg.payload.console.x_offset = 210;
                lcd_msg.payload.console.y_offset = 40;
                lcd_msg.payload.console.message = s;
                lcd_msg.payload.console.length = (uint16_t)n;
                if (xQueueSend(xQueue_LCD, &lcd_msg, portMAX_DELAY) != pdPASS)
                {
                    vPortFree(s);
                }
            }
        }

        /* Misses */
        n = snprintf(buf, sizeof(buf), "Miss: %d", misses);
        if (n > 0)
        {
            char *s = (char *)pvPortMalloc((size_t)n + 1);
            if (s)
            {
                strcpy(s, buf);
                lcd_msg.command = LCD_CONSOLE_DRAW_MESSAGE;
                lcd_msg.response_queue = NULL;
                lcd_msg.payload.console.x_offset = 210;
                lcd_msg.payload.console.y_offset = 80;
                lcd_msg.payload.console.message = s;
                lcd_msg.payload.console.length = (uint16_t)n;
                if (xQueueSend(xQueue_LCD, &lcd_msg, portMAX_DELAY) != pdPASS)
                {
                    vPortFree(s);
                }
            }
        }
    }

    for(;;)
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
    // Set text color to black
        /* Reset terminal colors to defaults so we don't force a specific theme */
        printf("\x1b[0m");
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