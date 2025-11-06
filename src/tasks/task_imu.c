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

static SemaphoreHandle_t *SPI_Semaphore = NULL;
static cyhal_spi_t *imu_spi_obj = NULL;
static cyhal_gpio_t imu_cs_pin = NC;

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

    // Array t ostore the raw accelerometer data
    int16_t accel_data[3];

    // Grab the SPI semaphore before accessing the IMU
    xSemaphoreTake(*SPI_Semaphore, portMAX_DELAY);

    // initialize the IMU
    if (!imu_init(imu_spi_obj, imu_cs_pin))
    {
        task_console_printf("IMU Init Failed\r\n");
        vTaskDelete(NULL);
    }
    else
    {
        task_console_printf("IMU Init Succeeded\r\n");
    }

    // Give the SPI semaphore back
    xSemaphoreGive(*SPI_Semaphore);

    // Take the SPI semaphore
    while(1)
    {
        // Add code here
        vTaskDelay(pdMS_TO_TICKS(250));

        // take the spi semaphore
        xSemaphoreTake(*SPI_Semaphore, portMAX_DELAY);

        // Read the accelerometer data
        imu_read_registers(
            imu_spi_obj, 
            imu_cs_pin, 
            IMU_REG_OUTX_L_XL, 
            (uint8_t *)accel_data, 
            6
        );

        // Release the SPI semaphore
        xSemaphoreGive(*SPI_Semaphore);

        task_console_printf(
            "Accel Data: X=%d, Y=%d\r\n", 
            accel_data[0], 
            accel_data[1]
        );
    }
}
#endif /* ECE353_FREERTOS */