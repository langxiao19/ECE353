/**
 * @file task_imu.c
 * @author Joe Krachey (jkrachey@wisc.edu)
 * @brief 
 * @version 0.1
 * @date 2025-09-16
 * 
 * @copyright Copyright (c) 2025
 * 
 */

 #include "task_imu.h"

 #if defined(ECE353_FREERTOS)
#include "imu.h"
#include "task_console.h"
#include "devices.h"

static SemaphoreHandle_t *SPI_Semaphore = NULL;
static cyhal_spi_t *imu_spi_obj = NULL;
static cyhal_gpio_t imu_cs_pin = NC;

QueueHandle_t Queue_IMU_Requests;

/**
 * @brief 
 * This is a helper function that is called by tasks other than the IMU task
 * when they want to read the IMU data.
 */
bool system_sensors_imu_read(QueueHandle_t return_queue, int16_t imu_data[3])
{
    bool status = false;
    device_request_msg_t request_packet;
    device_response_msg_t response_packet;

    if (return_queue == NULL || imu_data == NULL)
    {
        return false;
    }

    // Format the request packet
    request_packet.device = DEVICE_IMU;
    request_packet.operation = DEVICE_OP_READ;
    request_packet.response_queue = return_queue;

    // Send the request to the IMU task
    if (xQueueSend(Queue_IMU_Requests, &request_packet, portMAX_DELAY) == pdPASS)
    {
        // Wait for the response
        if (xQueueReceive(return_queue, &response_packet, portMAX_DELAY) == pdPASS)
        {
            if (response_packet.status == DEVICE_OPERATION_STATUS_READ_SUCCESS)
            {
                imu_data[0] = response_packet.payload.imu[0];
                imu_data[1] = response_packet.payload.imu[1];
                imu_data[2] = response_packet.payload.imu[2];
                status = true;
            }
        }
    }

    return status;
}

 /**
  * @brief 
  * This function will create the IMU task for reading data from the IMU sensor.
  * It assumes that you have already created a semaphore for SPI access and initialized
  * the SPI peripheral.  This function does NOT initialize the SPI peripheral OR CS Pin 
  * because the SPI peripheral is shared between multiple tasks (e.g. IMU, EEPROM, etc.). 
  * @param spi_semaphore 
  * @return true 
  * @return false 
  */
 bool task_imu_resources_init(void *spi_semaphore, cyhal_spi_t *spi_obj, cyhal_gpio_t cs_pin)
 {
    SPI_Semaphore = (SemaphoreHandle_t *) spi_semaphore;
    imu_spi_obj = spi_obj;
    imu_cs_pin = cs_pin;

    // Create the IMU request queue
    Queue_IMU_Requests = xQueueCreate(10, sizeof(device_request_msg_t));
    if (Queue_IMU_Requests == NULL)
    {
        return false;
    }

    // Create the IMU task
    if (xTaskCreate(
            task_imu, 
            "IMU Task", 
            TASK_IMU_STACK_SIZE, 
            NULL, 
            TASK_IMU_PRIORITY, 
            NULL
        ) != pdPASS)
    {
        return false;
    }
   return true;
 }

 void task_imu(void *arg)
 {
    (void) arg;
    device_request_msg_t request_packet;
    device_response_msg_t response_packet;
    int16_t accel_data[3];

    // Grab the SPI semaphore before accessing the IMU
    xSemaphoreTake(*SPI_Semaphore, portMAX_DELAY);

    // initialize the IMU
    if (!imu_init(imu_spi_obj, imu_cs_pin))
    {
        printf("IMU              : IMU Init Failed\r\n");
        xSemaphoreGive(*SPI_Semaphore);
        vTaskDelete(NULL);
    }

    // Give the SPI semaphore back
    xSemaphoreGive(*SPI_Semaphore);

    while(1)
    {
        // Wait for a request from the queue
        if (xQueueReceive(Queue_IMU_Requests, &request_packet, portMAX_DELAY) == pdPASS)
        {
            // Claim the SPI bus semaphore
            xSemaphoreTake(*SPI_Semaphore, portMAX_DELAY);

            // Process the request based on operation type
            if (request_packet.operation == DEVICE_OP_READ)
            {
                uint8_t raw_data[6];
                
                // Read the accelerometer data
                imu_read_registers(
                    imu_spi_obj, 
                    imu_cs_pin, 
                    IMU_REG_OUTX_L_XL, 
                    raw_data, 
                    6
                );

                // Properly combine the bytes (little-endian format)
                accel_data[0] = -((int16_t)((raw_data[1] << 8) | raw_data[0])); // X-axis (inverted)
                accel_data[1] = (int16_t)((raw_data[3] << 8) | raw_data[2]); // Y-axis
                accel_data[2] = (int16_t)((raw_data[5] << 8) | raw_data[4]); // Z-axis

                // Send the response back with the data
                if (request_packet.response_queue != NULL)
                {
                    response_packet.device = DEVICE_IMU;
                    response_packet.status = DEVICE_OPERATION_STATUS_READ_SUCCESS;
                    response_packet.payload.imu[0] = accel_data[0];
                    response_packet.payload.imu[1] = accel_data[1];
                    response_packet.payload.imu[2] = accel_data[2];
                    xQueueSend(request_packet.response_queue, &response_packet, portMAX_DELAY);
                }
            }

            // Release the SPI bus semaphore
            xSemaphoreGive(*SPI_Semaphore);
        }
    }
}
#endif /* ECE353_FREERTOS */