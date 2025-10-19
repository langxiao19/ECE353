/**
 * @file task_console.c
 * @author Joe Krachey (jkrachey@wisc.edu)
 * @brief 
 * @version 0.1
 * @date 2025-08-15
 * 
 * @copyright Copyright (c) 2025
 * 
 */
 #include "main.h"

#ifdef ECE353_FREERTOS
#include "drivers.h"
#include "task_console.h"
#include "cyhal_uart.h"
#include <string.h>
/**
 * @brief
 * This file contains the implementation of the console transmit (Tx) task.
 * The task is responsible for sending characters to the UART.
 * 
 * Tasks can print messages by sending the string to task_console_tx() using
 * a FreeRTOS queue.
 *
 * task_console_tx() will add the characters to a circular buffer that is
 * accessed by the UART interrupt service routine (ISR).
 *
 */

/* ADD CODE*/
/* Global Variables */
// Allocate space for the trasnsmit queue
QueueHandle_t xQueue_Console_Tx;
TaskHandle_t TaskHandle_Console_Tx;

// Allocate space for the circular buffer
circular_buffer_t *circular_buffer_tx;

/**
 * @brief 
 * This task is used to transmit characters to the UART
 * @param param 
 */
void task_console_tx(void *param)
{
    (void)param; // Unused parameter
    console_buffer_t tx_msg;

    while (1)
    {
        /* ADD CODE */
        // Wait for messages to arrive from the queue
        if (xQueueReceive(xQueue_Console_Tx, &tx_msg, portMAX_DELAY) == pdTRUE)
        {
            // Character-by-Character, copy the message into the circular buffer
            for (int i = 0; i < tx_msg.index; i++)
            {
                // If the Circular Buffer is full, sleep for 5mS
                while (circular_buffer_full(circular_buffer_tx))
                {
                    vTaskDelay(pdMS_TO_TICKS(5));
                }
                
                // Make sure adding each character is completed without interruption
                taskENTER_CRITICAL();
                circular_buffer_add(circular_buffer_tx, tx_msg.data[i]);
                taskEXIT_CRITICAL();
            }
            
            // Enable Tx Empty Interrupts
            cyhal_uart_enable_event(&cy_retarget_io_uart_obj, CYHAL_UART_IRQ_TX_EMPTY, 7, true);
            
            // Return the memory allocated for the console_buffer_t data field to the heap
            vPortFree(tx_msg.data);
        }
    }
}

/**
 * @brief 
 * This function initializes the resources for the console Tx task. 
 * @return true  if initialization is successful
 * @return false if initialization fails
 */
bool task_console_resources_init_tx(void)
{
    BaseType_t rslt = pdPASS;

    /* ADD CODE */
    // For ICE09, we don't need a Tx task yet
    // This will be implemented in ICE10
    // Just return true for now

    // Initialize Circular Buffer
    circular_buffer_tx = circular_buffer_init(CONSOLE_MAX_MESSAGE_LENGTH * 4);
    if (circular_buffer_tx == NULL)
    {
        rslt = pdFAIL;
    }

    // Initialize the FreeRTOS Queue used to maintain message order
    if (rslt == pdPASS)
    {
        xQueue_Console_Tx = xQueueCreate(CONSOLE_QUEUE_LENGTH, sizeof(console_buffer_t));
        if (xQueue_Console_Tx == NULL)
        {
            rslt = pdFAIL;
        }
    }

    // Create FreeRTOS Tx Task (gatekeeper)
    if (rslt == pdPASS)
    {
        rslt = xTaskCreate(task_console_tx,
                           "Console Tx",
                           configMINIMAL_STACK_SIZE,
                           NULL,
                           tskIDLE_PRIORITY + 1,
                           &TaskHandle_Console_Tx);
    }

    if (rslt != pdPASS)
    {
        return false; // Initialization failed
    }

    return true; // Resources initialized successfully
}

/**
 * @brief
 * This function sends formatted messages to task_console_tx. It acts as a wrapper around the FreeRTOS queue
 * to send messages so other tasks can use it easily.
 *
 * Example usage: 
 * task_console_printf("Send Message");
 * task_console_printf("Formatted number: %d", 42);
 *
 * @param str_ptr Pointer to the format string.
 * @param ...     Additional arguments for formatting.
 */
void task_console_printf(char *str_ptr, ...)
{
    console_buffer_t console_buffer;
    char *message_buffer;
    char *task_name;
    uint32_t length = 0;
    va_list args;

    /* ADD CODE */
    // Allocate CONSOLE_MAX_MESSAGE_LENGTH bytes of data for the message
    message_buffer = (char *)pvPortMalloc(CONSOLE_MAX_MESSAGE_LENGTH);

    if (message_buffer)
    {
        va_start(args, str_ptr);
        task_name = pcTaskGetName(xTaskGetCurrentTaskHandle());
        length = snprintf(message_buffer, CONSOLE_MAX_MESSAGE_LENGTH, "%-16s : ",
                              task_name);

        vsnprintf((message_buffer + length), (CONSOLE_MAX_MESSAGE_LENGTH - length),
                  str_ptr, args);

        va_end(args);

        // Initialize the fields of the console_buffer_t
        console_buffer.data = message_buffer;
        console_buffer.index = strlen(message_buffer);
    
        // Send the message to the gatekeeper task using a FreeRTOS Queue
        if (xQueueSend(xQueue_Console_Tx, &console_buffer, 0) != pdTRUE)
        {
            /* Queue is full, free the memory */
            vPortFree(message_buffer);
        }
    }
    else
    {
        /* pvPortMalloc failed. Handle error */
        CY_ASSERT(0); // Halt the processor
    }
}
#endif