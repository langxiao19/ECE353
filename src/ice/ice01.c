/**
 * @file ice01.c
 * @author Joe Krachey (jkrachey@wisc.edu)
 * @brief 
 * @version 0.1
 * @date 2025-07-01
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "main.h"

#if defined(ICE01)
#include "drivers.h"

char APP_DESCRIPTION[] = "ECE353: ICE 01 - Memory Mapped IO - GPIO";

/*****************************************************************************/
/* Macros                                                                    */
/*****************************************************************************/

/*****************************************************************************/
/* Global Variables                                                          */
/*****************************************************************************/

/*****************************************************************************/
/* Function Declarations                                                     */
/*****************************************************************************/

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
    // Initialize buttons and LEDs
    buttons_init_gpio();
    leds_init_gpio();
    console_init();
    printf("\x1b[2J\x1b[;H");
    printf("**************************************************\n\r");
    printf("* %s\n\r", APP_DESCRIPTION);
    printf("* Date: %s\n\r", __DATE__);
    printf("* Time: %s\n\r", __TIME__);
    printf("* Name:%s\n\r", NAME);
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
    button_state_t sw1_state, sw2_state, sw3_state;

    while(1)
    {
        sw1_state = buttons_get_state(BUTTON_SW1);
        sw2_state = buttons_get_state(BUTTON_SW2);
        sw3_state = buttons_get_state(BUTTON_SW3);

        // SW1
        if (sw1_state == BUTTON_STATE_FALLING_EDGE) {
            printf("SW1 pressed\r\n");
            leds_set_state(LED_RED, LED_ON);
        } else if (sw1_state == BUTTON_STATE_RISING_EDGE) {
            printf("SW1 released\r\n");
            leds_set_state(LED_RED, LED_OFF);
        }

        // SW2
        if (sw2_state == BUTTON_STATE_FALLING_EDGE) {
            printf("SW2 pressed\r\n");
            leds_set_state(LED_GREEN, LED_ON);
        } else if (sw2_state == BUTTON_STATE_RISING_EDGE) {
            printf("SW2 released\r\n");
            leds_set_state(LED_GREEN, LED_OFF);
        }

        // SW3
        if (sw3_state == BUTTON_STATE_FALLING_EDGE) {
            printf("SW3 pressed\r\n");
            leds_set_state(LED_BLUE, LED_ON);
        } else if (sw3_state == BUTTON_STATE_RISING_EDGE) {
            printf("SW3 released\r\n");
            leds_set_state(LED_BLUE, LED_OFF);
        }

        cyhal_system_delay_ms(100);
    }
}
#endif
