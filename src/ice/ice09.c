/**
 * @file ex03.c
 * @author Joe Krachey (jkrachey@wisc.edu)
 * @brief 
 * @version 0.1
 * @date 2025-06-30
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "main.h"

#if defined(ICE09)
#include "drivers.h"
#include "task_console.h"
#include "task_buttons.h"

char APP_DESCRIPTION[] = "ECE353: ICE 09 - FreeRTOS UART IRQs";

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
    cy_rslt_t rslt;

    console_init();
    printf("**************************************************\n\r");
    printf("* %s\n\r", APP_DESCRIPTION);
    printf("* Date: %s\n\r", __DATE__);
    printf("* Time: %s\n\r", __TIME__);
    printf("* Name:%s\n\r", NAME);
    printf("**************************************************\n\r");

    // Initialize the LEDs
    rslt = leds_init_gpio();
    if (rslt != CY_RSLT_SUCCESS)
    {
        printf("LED initialization failed!\n\r");
    }

    // Button initialization is done in task_buttons_init()
    // Remove duplicate initialization here

    // Remove buzzer initialization since we're not using it
    // rslt = buzzer_init(0.5f, 1000);  // 50% duty cycle, 1000 Hz frequency
    // if (rslt != CY_RSLT_SUCCESS)
    // {
    //     printf("Buzzer initialization failed!\n\r");
    // }
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
    /* Create the FreeRTOS Event Group */
    ECE353_RTOS_Events = xEventGroupCreate();
    if (ECE353_RTOS_Events == NULL)
    {
        printf("Failed to create event group!\n\r");
        CY_ASSERT(0);
    }

    if(!task_console_init())
    {
        printf("Console initialization failed!\n\r");
        for(int i = 0; i < 10000; i++);
        CY_ASSERT(0);
    }

    /* Initialize the Button Task resources */
    if (!task_buttons_init())  // Fixed function name
    {
        printf("Failed to initialize button task\n\r");
        for(int i = 0; i < 10000; i++);
        CY_ASSERT(0);
    }

    /* Remove the buzzer task - we just want button messages */
    // if (!xTaskCreate(task_buzzer, "Buzzer Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL))
    // {
    //     printf("Failed to create buzzer task\n\r");
    //     for(int i = 0; i < 10000; i++);
    //     CY_ASSERT(0);
    // }

    /* Start the scheduler*/
    vTaskStartScheduler();

    /* Will never reach this loop once the scheduler starts */
    while (1)
    {
    }
}
#endif