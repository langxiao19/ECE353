/**
 * @file task_eeprom.c
 * @author Joe Krachey (jkrachey@wisc.edu)
 * @brief 
 * @version 0.1
 * @date 2025-09-17
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "main.h"

#if defined(ECE353_FREERTOS)
 #include "task_console.h"
 #include "task_eeprom.h"

 #define TASK_EEPROM_STACK_SIZE (configMINIMAL_STACK_SIZE * 10)
 #define TASK_EEPROM_PRIORITY   (tskIDLE_PRIORITY + 1)

 /* Global Variables */
QueueHandle_t Queue_EEPROM_Requests;
static SemaphoreHandle_t *SPI_Semaphore = NULL;
static cyhal_spi_t *eeprom_spi_obj = NULL;
static cyhal_gpio_t eeprom_cs_pin = NC;

/**
 * @brief 
 *  This function acts as the published interface to write data to the EEPROM.
 *  It will format the device request message and send it to the EEPROM task.
 *
 *  If the user provides a valid return queue, this function will wait for
 *  the response from the EEPROM task before returning.  If the user provides
 *  a NULL return queue, this function will return immediately after sending
 *  the request.
 * @param return_queue 
 * @param address 
 * @param data 
 * @param length 
 * @return true 
 * @return false 
 */
bool system_sensors_eeprom_write(QueueHandle_t return_queue, uint16_t address, uint8_t data)
{
    bool status = false;
    device_request_msg_t request_packet;
    device_response_msg_t response_packet;

    // Format the request packet
    request_packet.device = DEVICE_EEPROM;
    request_packet.operation = DEVICE_OP_WRITE;
    request_packet.address = address;
    request_packet.value = data;
    request_packet.response_queue = return_queue;

    // Send the request to the EEPROM task
    if (xQueueSend(Queue_EEPROM_Requests, &request_packet, portMAX_DELAY) == pdPASS)
    {
        // If a return queue was provided, wait for the response
        if (return_queue != NULL)
        {
            if (xQueueReceive(return_queue, &response_packet, portMAX_DELAY) == pdPASS)
            {
                status = (response_packet.status == DEVICE_OPERATION_STATUS_WRITE_SUCCESS);
            }
        }
        else
        {
            status = true;  // Request sent successfully, not waiting for response
        }
    }

    return status;
}   

/**
 * @brief 
 * This function is the published interface to read data from the EEPROM.
 * It will format the device request message and send it to the EEPROM task.
 *
 * The value read from the EEPROM will be returned via the data pointer
 *
 * @param return_queue 
 * @param address 
 * @param data 
 * @return true 
 * @return false 
 */
bool system_sensors_eeprom_read(QueueHandle_t return_queue, uint16_t address, uint8_t *data)
{
    bool status = false;
    device_request_msg_t request_packet;
    device_response_msg_t response_packet;

    // Validate input
    if (data == NULL || return_queue == NULL)
    {
        return false;
    }

    // Format the request packet
    request_packet.device = DEVICE_EEPROM;
    request_packet.operation = DEVICE_OP_READ;
    request_packet.address = address;
    request_packet.value = 0;  // Not used for read
    request_packet.response_queue = return_queue;

    // Send the request to the EEPROM task
    if (xQueueSend(Queue_EEPROM_Requests, &request_packet, portMAX_DELAY) == pdPASS)
    {
        // Wait for the response
        if (xQueueReceive(return_queue, &response_packet, portMAX_DELAY) == pdPASS)
        {
            if (response_packet.status == DEVICE_OPERATION_STATUS_READ_SUCCESS)
            {
                *data = response_packet.payload.eeprom;
                status = true;
            }
        }
    }

    return status;
}

/**
 * @brief 
 *  Task used to monitor the reception of command packets sent the EEPROM
 * @param arg 
 */
void task_eeprom(void *arg)
 {
    (void) arg;
    device_request_msg_t request_packet;
    device_response_msg_t response_packet;

    while(1)
    {
        // Wait for a request from the queue
        if (xQueueReceive(Queue_EEPROM_Requests, &request_packet, portMAX_DELAY) == pdPASS)
        {
            // Claim the SPI bus semaphore
            xSemaphoreTake(*SPI_Semaphore, portMAX_DELAY);

            // Process the request based on operation type
            if (request_packet.operation == DEVICE_OP_WRITE)
            {
                // Write to EEPROM
                eeprom_write_byte(eeprom_spi_obj, eeprom_cs_pin, request_packet.address, request_packet.value);

                // If a response queue was provided, send back success status
                if (request_packet.response_queue != NULL)
                {
                    response_packet.device = DEVICE_EEPROM;
                    response_packet.status = DEVICE_OPERATION_STATUS_WRITE_SUCCESS;
                    xQueueSend(request_packet.response_queue, &response_packet, portMAX_DELAY);
                }
            }
            else if (request_packet.operation == DEVICE_OP_READ)
            {
                // Read from EEPROM
                uint8_t read_data = eeprom_read_byte(eeprom_spi_obj, eeprom_cs_pin, request_packet.address);

                // Send the response back with the data
                if (request_packet.response_queue != NULL)
                {
                    response_packet.device = DEVICE_EEPROM;
                    response_packet.status = DEVICE_OPERATION_STATUS_READ_SUCCESS;
                    response_packet.payload.eeprom = read_data;
                    xQueueSend(request_packet.response_queue, &response_packet, portMAX_DELAY);
                }
            }

            // Release the SPI bus semaphore
            xSemaphoreGive(*SPI_Semaphore);
        }
    }
}


/**
 * @brief 
 * Function used to initialize resources for the EEPROM task
 * @param spi_semaphore 
 * @param spi_obj 
 * @param cs_pin 
 * @return true 
 * @return false 
 */
bool task_eeprom_resources_init(SemaphoreHandle_t *spi_semaphore, cyhal_spi_t *spi_obj, cyhal_gpio_t cs_pin)
 {
    if(spi_semaphore == NULL || spi_obj == NULL || cs_pin == NC)
    {
        return false;
    }

    // Save handles for access to the SPI peripheral and semaphore
    SPI_Semaphore = spi_semaphore;
    eeprom_spi_obj = spi_obj;
    eeprom_cs_pin = cs_pin;

    /*Create the EEPROM Requests Queue */
    Queue_EEPROM_Requests = xQueueCreate(1, sizeof(device_request_msg_t ));

    /* Create the FreeRTOS task for the EEPROM */
    if (xTaskCreate(
        task_eeprom, 
        "EEPROM", 
        TASK_EEPROM_STACK_SIZE, 
        spi_semaphore, 
        TASK_EEPROM_PRIORITY, 
        NULL) != pdPASS)
    {
        return false;
    }
    return true;
 }

#endif /* ECE353_FREERTOS */    