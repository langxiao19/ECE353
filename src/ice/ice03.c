/**
 * @file ice02.c
 * @author Joe Krachey (jkrachey@wisc.edu)
 * @brief 
 * @version 0.1
 * @date 2025-07-01
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "main.h"

#if defined(ICE03)
#include "drivers.h"
#include <stdio.h>
#include <stdbool.h>

char APP_DESCRIPTION[] = "ECE353: ICE 03 - Timer Interrupts/Debounce Buttons";

/*****************************************************************************/
/* Macros                                                                    */
/*****************************************************************************/

/*****************************************************************************/
/* Global Variables                                                          */
/*****************************************************************************/
 // Finite State Machine for sequence: SW1 -> SW2 -> SW2 -> SW3
typedef enum {
    STATE_ICE03_INIT = 0,
    STATE_ICE03_SW1_DET,
    STATE_ICE03_SW2_DET_1,
    STATE_ICE03_SW2_DET_2,
    STATE_ICE03_SW3_DET,
} ice03_state_t;

static ice03_state_t current_state = STATE_ICE03_INIT;

/*****************************************************************************/
/* Function Declarations                                                     */
/*****************************************************************************/
static void ice03_set_leds(ice03_state_t state);

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/**
 * @brief
 * This function will initialize all of the hardware resources for
 * the ICE
 */
void app_init_hw(void)
{
    cy_rslt_t rslt;

    console_init();
    printf("\x1b[2J\x1b[;H");
    printf("**************************************************\n\r");
    printf("* %s\n\r", APP_DESCRIPTION);
    printf("* Date: %s\n\r", __DATE__);
    printf("* Time: %s\n\r", __TIME__);
    printf("* Name:%s\n\r", NAME);
    printf("**************************************************\n\r");

    // Initialize GPIO for LEDs and Buttons, and start the debounce timer
    rslt = CY_RSLT_SUCCESS;
    rslt |= leds_init_gpio();
    rslt |= buttons_init_gpio();
    rslt |= buttons_init_timer();
    if (rslt != CY_RSLT_SUCCESS) {
        printf("ICE03: Hardware init error: 0x%08lX\n\r", (unsigned long)rslt);
    }

    current_state = STATE_ICE03_INIT;
    ice03_set_leds(current_state);
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
    while(1)
    {
        if (ECE353_Events.sw1 || ECE353_Events.sw2 || ECE353_Events.sw3)
        {
            // Latch which button caused the event, then clear events
            bool sw1 = (ECE353_Events.sw1 != 0);
            bool sw2 = (ECE353_Events.sw2 != 0);
            bool sw3 = (ECE353_Events.sw3 != 0);
            ECE353_Events.sw1 = 0;
            ECE353_Events.sw2 = 0;
            ECE353_Events.sw3 = 0;

            switch(current_state)
            {
                case STATE_ICE03_INIT:
                    if (sw1) {
                        current_state = STATE_ICE03_SW1_DET;
                    } else {
                        // SW2 or SW3 pressed while idle restarts/stays in INIT
                        current_state = STATE_ICE03_INIT;
                    }
                    break;

                case STATE_ICE03_SW1_DET:
                    if (sw2) {
                        current_state = STATE_ICE03_SW2_DET_1;
                    } else {
                        // Wrong key, restart
                        current_state = STATE_ICE03_INIT;
                    }
                    break;

                case STATE_ICE03_SW2_DET_1:
                    if (sw2) {
                        current_state = STATE_ICE03_SW2_DET_2;
                    } else {
                        current_state = STATE_ICE03_INIT;
                    }
                    break;

                case STATE_ICE03_SW2_DET_2:
                    if (sw3) {
                        current_state = STATE_ICE03_SW3_DET;
                    } else {
                        current_state = STATE_ICE03_INIT;
                    }
                    break;

                case STATE_ICE03_SW3_DET:
                    // Any subsequent press returns to INIT
                    current_state = STATE_ICE03_INIT;
                    break;

                default:
                    printf("ICE03: Unknown State!\n");
                    current_state = STATE_ICE03_INIT;
            }

            // Update the LEDs based on the current state
            ice03_set_leds(current_state);
        }
    }
}

// Helper to drive LEDs for each state
static void ice03_set_leds(ice03_state_t state)
{
    switch(state)
    {
        case STATE_ICE03_INIT: // Red
            leds_set_state(LED_RED, LED_ON);
            leds_set_state(LED_GREEN, LED_OFF);
            leds_set_state(LED_BLUE, LED_OFF);
            break;

        case STATE_ICE03_SW1_DET: // Red + Blue (Magenta)
            leds_set_state(LED_RED, LED_ON);
            leds_set_state(LED_BLUE, LED_ON);
            leds_set_state(LED_GREEN, LED_OFF);
            break;

        case STATE_ICE03_SW2_DET_1: // Blue
            leds_set_state(LED_RED, LED_OFF);
            leds_set_state(LED_GREEN, LED_OFF);
            leds_set_state(LED_BLUE, LED_ON);
            break;

        case STATE_ICE03_SW2_DET_2: // Blue + Green (Cyan)
            leds_set_state(LED_RED, LED_OFF);
            leds_set_state(LED_GREEN, LED_ON);
            leds_set_state(LED_BLUE, LED_ON);
            break;

        case STATE_ICE03_SW3_DET: // Green
            leds_set_state(LED_RED, LED_OFF);
            leds_set_state(LED_GREEN, LED_ON);
            leds_set_state(LED_BLUE, LED_OFF);
            break;

        default:
            break;
    }
}
#endif
