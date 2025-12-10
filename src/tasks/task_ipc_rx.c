/**
 * @file task_console_rx.c
 * @author Joe Krachey (jkrachey@wisc.edu)
 * @brief 
 * @version 0.1
 * @date 2025-08-21
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "main.h"
#include <stdbool.h>
#include <string.h>

#if defined(ECE353_FREERTOS)
#include "task_ipc.h"
#include "task_console.h"

/* Globals */
TaskHandle_t TaskHandle_IPC_Rx = NULL;
QueueHandle_t Queue_IPC_Rx_Game_Control = NULL;
QueueHandle_t Queue_IPC_Rx_Fire = NULL;
QueueHandle_t Queue_IPC_Rx_Result = NULL;

/* Use a double buffering strategy for IPC packets */
static volatile ipc_packet_t IPC_Rx_Buffer0;
static volatile ipc_packet_t IPC_Rx_Buffer1;

volatile ipc_packet_t* volatile IPC_Rx_Produce_Buffer = &IPC_Rx_Buffer0;
volatile ipc_packet_t* volatile IPC_Rx_Consume_Buffer = &IPC_Rx_Buffer1;

/**
 * @brief
 *
 * This task is used to process received IPC packets.  The task will block
 * on a FreeRTOS Task Notification.  When a notification is received,
 * the task will process the IPC packet stored in the consume buffer.
 *
 * For validation purposes, the task will print out the contents of the
 * received IPC packet to the console.
 * 
 * @param arg
 * Unused parameter
 */
void task_ipc_rx(void *param)
{
    ipc_packet_t packet;

    while(1)
    {
        // Wait for a FreeRTOS Task Notification
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // Copy the packet from the consume buffer (to avoid race conditions)
        memcpy(&packet, (void*)IPC_Rx_Consume_Buffer, sizeof(ipc_packet_t));

        // Validate the packet
        if(!validate_packet(&packet))
        {
            printf("[IPC RX] Invalid packet received (bad start byte or checksum)\n\r");
            continue;
        }

        // Process the IPC Packet based on command type
        switch(packet.cmd)
        {
            case IPC_CMD_FIRE:
                printf("[IPC RX] FIRE command received: Row=%d, Col=%d\n\r", 
                       packet.fire.row, packet.fire.col);
                if(Queue_IPC_Rx_Fire != NULL) {
                    xQueueSend(Queue_IPC_Rx_Fire, &packet.fire, 0);
                }
                break;

            case IPC_CMD_RESULT:
                printf("[IPC RX] RESULT command received: ");
                switch(packet.result)
                {
                    case IPC_RESULT_MISS:
                        printf("MISS\n\r");
                        break;
                    case IPC_RESULT_HIT:
                        printf("HIT\n\r");
                        break;
                    case IPC_RESULT_SUNK:
                        printf("SUNK\n\r");
                        break;
                    default:
                        printf("Unknown (0x%02X)\n\r", packet.result);
                        break;
                }
                if(Queue_IPC_Rx_Result != NULL) {
                    xQueueSend(Queue_IPC_Rx_Result, &packet.result, 0);
                }
                break;

            case IPC_CMD_GAME_CONTROL:
                printf("[IPC RX] GAME_CONTROL command received: ");
                switch(packet.game_control)
                {
                    case IPC_GAME_CONTROL_NEW_GAME:
                        printf("NEW_GAME\n\r");
                        if(Queue_IPC_Rx_Game_Control != NULL) {
                            xQueueSend(Queue_IPC_Rx_Game_Control, &packet.game_control, 0);
                        }
                        break;
                    case IPC_GAME_CONTROL_PLAYER_READY:
                        printf("PLAYER_READY\n\r");
                        break;
                    case IPC_GAME_CONTROL_PLAYER_ALIVE:
                        printf("PLAYER_ALIVE\n\r");
                        break;
                    case IPC_GAME_CONTROL_PASS_TURN:
                        printf("PASS_TURN\n\r");
                        break;
                    case IPC_GAME_CONTROL_ACK:
                        printf("ACK\n\r");
                        break;
                    case IPC_GAME_CONTROL_END_GAME:
                        printf("END_GAME\n\r");
                        break;
                    default:
                        printf("Unknown (0x%02X)\n\r", packet.game_control);
                        break;
                }
                break;

            case IPC_CMD_ERROR:
                printf("[IPC RX] ERROR command received: ");
                switch(packet.error)
                {
                    case IPC_ERROR_CHECKSUM:
                        printf("CHECKSUM\n\r");
                        break;
                    case IPC_ERROR_COORD_INVALID:
                        printf("COORD_INVALID\n\r");
                        break;
                    case IPC_ERROR_COORD_OCCUPIED:
                        printf("COORD_OCCUPIED\n\r");
                        break;
                    case IPC_ERROR_SYSTEM_FAILURE:
                        printf("SYSTEM_FAILURE\n\r");
                        break;
                    default:
                        printf("Unknown (0x%02X)\n\r", packet.error);
                        break;
                }
                break;

            default:
                printf("[IPC RX] Unknown command received: 0x%02X\n\r", packet.cmd);
                break;
        }
    }
}

bool task_ipc_resources_init_rx(void)
{
    // Create queue for game control messages
    Queue_IPC_Rx_Game_Control = xQueueCreate(5, sizeof(ipc_game_control_t));
    if(Queue_IPC_Rx_Game_Control == NULL) {
        return false;
    }
    
    // Create queue for incoming fire commands
    Queue_IPC_Rx_Fire = xQueueCreate(5, sizeof(ipc_fire_payload_t));
    if(Queue_IPC_Rx_Fire == NULL) {
        return false;
    }
    
    // Create queue for result messages
    Queue_IPC_Rx_Result = xQueueCreate(5, sizeof(ipc_result_t));
    if(Queue_IPC_Rx_Result == NULL) {
        return false;
    }
    
    // Create the IPC Rx Task
    BaseType_t task_ipc_rx_status = xTaskCreate(
        task_ipc_rx,                 // Function that implements the task.
        "IPC Rx Task",               // Text name for the task.
        IPC_STACK_SIZE,    // Stack size in words, not bytes.
        NULL,                       // Parameter passed into the task.
        IPC_PRIORITY,       // Priority at which the task is created.
        &TaskHandle_IPC_Rx          // Used to pass out the created task's handle.
    );

    if(task_ipc_rx_status != pdPASS)
    {
        return false;
    }

    return true;    
}

#endif