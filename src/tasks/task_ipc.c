/**
 * @file task_ipc.c
 * @author Joe Krachey (jkrachey@wisc.edu)
 * @brief 
 * @version 0.1
 * @date 2025-09-03
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "task_ipc.h"
#include "cy_result.h"
#include "cyhal_hw_types.h"
#include "cyhal_uart.h"
#include "main.h"
#include "task_console.h"
#include <string.h>

#if defined(ECE353_FREERTOS)

/*****************************************************************************/
/* Global Variables                                                          */
/*****************************************************************************/
cyhal_uart_t IPC_Uart_Obj;
cyhal_uart_cfg_t IPC_Uart_Config =
{
    .data_bits = 8,
    .stop_bits = 1,
    .parity = CYHAL_UART_PARITY_NONE,
    .rx_buffer = NULL,
    .rx_buffer_size = 0
};

uint32_t IPC_Actual_Baud;

/**
 * @brief 
 * Simple checksum calculation function.  Take the XOR of all bytes
 * except the start and checksum bytes.
 * @param packet 
 * @return __inline 
 */
static __inline uint8_t calculate_checksum(ipc_packet_t *packet)
{
    uint8_t checksum = 0;
    uint8_t *packet_bytes = (uint8_t *)packet;
    for(int i = 1; i < sizeof(ipc_packet_t) - 1; i++)
    {
        checksum ^= packet_bytes[i];  // XOR all bytes except start, checksum, and end 
    }
    return checksum;
}   

/**
 * @brief 
 * Validates the given IPC packet by checking the start byte and checksum
 * @param packet 
 * @return __inline 
 */
bool validate_packet(ipc_packet_t *packet)
{
    bool packet_valid = false;
   
    // Check if the start byte is correct
    if(packet->start_byte != IPC_PACKET_START)
    {
        return false;
    }

    // Calculate the checksum and compare with the received checksum
    uint8_t calculated_checksum = calculate_checksum(packet);
    if(calculated_checksum == packet->checksum)
    {
        packet_valid = true;
    }

    return packet_valid;
}

/**
 * @brief 
 * This function is used to send a "fire" command to the opponent
 * @param row 
 * @param col 
 * @return true 
 * @return false 
 */
bool ipc_send_fire(uint8_t row, uint8_t col)
{
    bool status = false;

    ipc_packet_t packet = {0}; // Initialize all fields to zero
    packet.start_byte = IPC_PACKET_START;
    packet.cmd = IPC_CMD_FIRE;
    packet.fire.row = row;
    packet.fire.col = col;
    packet.checksum = calculate_checksum(&packet);

    // Transmit the packet using the IPC UART
    if(xQueueSend(Queue_IPC_Tx, &packet, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        status = true;
    }

    return status;
}

/**
 * @brief 
 *  This function is used to send a "result" command to the opponent
 * @param result 
 * @return true 
 * @return false 
 */
bool ipc_send_result(ipc_result_t result)
{
    bool status = false;

    ipc_packet_t packet = {0}; // Initialize all fields to zero
    packet.start_byte = IPC_PACKET_START;
    packet.cmd = IPC_CMD_RESULT;
    packet.result = result;
    packet.checksum = calculate_checksum(&packet);

    // Transmit the packet using the IPC UART
    if(xQueueSend(Queue_IPC_Tx, &packet, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        status = true;
    }

    return status;
}

/**
 * @brief 
 * This function is used to send a "game control" command to the opponent
 * @param control 
 * @return true 
 * @return false 
 */
bool ipc_send_game_control(ipc_game_control_t control)
{
    bool status = false;

    ipc_packet_t packet = {0}; // Initialize all fields to zero
    packet.start_byte = IPC_PACKET_START;
    packet.cmd = IPC_CMD_GAME_CONTROL;
    packet.game_control = control;
    packet.checksum = calculate_checksum(&packet);

    // Transmit the packet using the IPC UART
    if(xQueueSend(Queue_IPC_Tx, &packet, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        status = true;
    }

    return status;
}

/**
 * @brief 
 * This function is used to send an "error" command to the opponent
 * @param error 
 * @return true 
 * @return false 
 */
bool ipc_send_error(ipc_error_t error)
{
    bool status = false;

    ipc_packet_t packet = {0}; // Initialize all fields to zero
    packet.start_byte = IPC_PACKET_START;
    packet.cmd = IPC_CMD_ERROR;
    packet.error = error;
    packet.checksum = calculate_checksum(&packet);

    // Transmit the packet using the IPC UART
    if(xQueueSend(Queue_IPC_Tx, &packet, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        status = true;
    }

    return status;
}

/**
 * @brief
 * Interrupt handler for the IPC UART. This function handles both RX and TX interrupts.
 *
 * @param handler_arg Pointer to handler arguments (not used).
 * @param event The UART event that triggered the interrupt.
 */
void ipc_event_handler(void *handler_arg, cyhal_uart_event_t event)
{
    (void)handler_arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    char c;
    static uint8_t raw_data[sizeof(ipc_packet_t)] = {0};
    static uint8_t raw_data_index = 0;

    if ((event & CYHAL_UART_IRQ_RX_NOT_EMPTY) == CYHAL_UART_IRQ_RX_NOT_EMPTY)
    {
        // Read the character from the UART
        cyhal_uart_getc(&IPC_Uart_Obj, (uint8_t*)&c, 0);
        
        // Store the byte in the raw data buffer
        raw_data[raw_data_index] = c;
        raw_data_index++;
        
        // Check if we have received a complete packet
        if(raw_data_index >= sizeof(ipc_packet_t))
        {
            // Copy the raw data into the produce buffer
            memcpy((void*)IPC_Rx_Produce_Buffer, raw_data, sizeof(ipc_packet_t));
            
            // Swap the produce and consume buffers
            volatile ipc_packet_t* temp = IPC_Rx_Produce_Buffer;
            IPC_Rx_Produce_Buffer = IPC_Rx_Consume_Buffer;
            IPC_Rx_Consume_Buffer = temp;
            
            // Reset the index for the next packet
            raw_data_index = 0;
            
            // Notify the IPC Rx task that a packet has been received
            vTaskNotifyGiveFromISR(TaskHandle_IPC_Rx, &xHigherPriorityTaskWoken);
            
            // Perform a context switch if necessary
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
    if ((event & CYHAL_UART_IRQ_TX_EMPTY) == CYHAL_UART_IRQ_TX_EMPTY)
    {
    }
    else
    {
    }
}

bool task_ipc_init(void)
{
    cy_rslt_t rslt;

    // Initialize the IPC UART
   rslt =  cyhal_uart_init(
        &IPC_Uart_Obj, 
        PIN_IPC_TX, 
        PIN_IPC_RX, 
        NC, 
        NC, 
        NULL, 
        &IPC_Uart_Config
    );
    if (rslt != CY_RSLT_SUCCESS)
    {
        printf("IPC UART init failed with error: 0x%08lX\n\r", rslt);
        return false; // Initialization failed
    }

    rslt = cyhal_uart_set_baud(&IPC_Uart_Obj, 115200, &IPC_Actual_Baud);
    if (rslt != CY_RSLT_SUCCESS)
    {
        printf("IPC UART baud rate set failed with error: 0x%08lX\n\r", rslt);
        return false; // Initialization failed
    }

    cyhal_uart_clear(&IPC_Uart_Obj);

    // Register the UART handler
    cyhal_uart_register_callback(&IPC_Uart_Obj, ipc_event_handler, NULL);

    // Enable Rx Interrupts
    cyhal_uart_enable_event(
        &IPC_Uart_Obj,
        CYHAL_UART_IRQ_RX_NOT_EMPTY,
        INT_PRIORITY_IPC,
        true
    );


    if(task_ipc_resources_init_rx() == false)
    {
        printf("IPC Rx task init failed\n\r");
        return false; // Initialization failed
    }

    if(task_ipc_resources_init_tx() == false)
    {
        printf("IPC Tx task init failed\n\r");
        return false; // Initialization failed
    }

    return true; // Initialization successful
}
#endif  