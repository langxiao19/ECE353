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

#ifdef ECE353_FREERTOS
#include "drivers.h"
#include "task_console.h"
#include "cyhal_uart.h"
#include <string.h>
#include <ctype.h>

#if defined(HW05)
#include "devices.h"
#include "task_imu.h"
#include "task_light_sensor.h"
#include "task_io_expander.h"
#include "task_eeprom.h"
#endif

/**
 * @brief
 * This file contains the implementation of the console receive (Rx) task.
 * The task is responsible for processing incoming console commands and
 * controlling the state of the LEDs accordingly.
 * 
 * The task uses a double buffer to process the incoming console commands.
 * The supported commands will be "RED_ON" and "RED_OFF" to control the red LED.
 */

/* ADD CODE */
/* Global Variables */
console_buffer_t console_buffer1;
console_buffer_t console_buffer2;

console_buffer_t *produce_console_buffer;
console_buffer_t *consume_console_buffer;

TaskHandle_t TaskHandle_Console_Rx;

#if defined(HW05)
static QueueHandle_t console_response_queue = NULL;

/**
 * @brief Helper function to convert string to uppercase
 */
static void str_to_upper(char *str)
{
    for (int i = 0; str[i] != '\0'; i++)
    {
        str[i] = toupper((unsigned char)str[i]);
    }
}

/**
 * @brief Helper function to parse hex value from string
 */
static bool parse_hex(const char *str, uint32_t *value)
{
    char *endptr;
    *value = strtoul(str, &endptr, 16);
    return (*endptr == '\0' && endptr != str);
}

/**
 * @brief Process CLI commands for HW05
 */
static void process_hw05_command(char *cmd)
{
    char *token;
    char *saveptr;
    
    // Make command uppercase for case-insensitive comparison
    str_to_upper(cmd);
    
    // Get first token (command)
    token = strtok_r(cmd, " ", &saveptr);
    if (token == NULL) return;
    
    // CLI-001: IMU
    if (strcmp(token, "IMU") == 0)
    {
        int16_t imu_data[3];
        if (system_sensors_imu_read(console_response_queue, imu_data))
        {
            task_console_printf("IMU Data: X=%d, Y=%d, Z=%d\r\n", 
                imu_data[0], imu_data[1], imu_data[2]);
        }
        else
        {
            task_console_printf("Error: Failed to read IMU data\r\n");
        }
    }
    // CLI-002: LIGHT
    else if (strcmp(token, "LIGHT") == 0)
    {
        uint16_t light_value;
        if (system_sensors_get_light(console_response_queue, &light_value))
        {
            task_console_printf("Light Sensor: %u\r\n", light_value);
        }
        else
        {
            task_console_printf("Error: Failed to read light sensor\r\n");
        }
    }
    // CLI-003 & CLI-004: IOEXP
    else if (strcmp(token, "IOEXP") == 0)
    {
        token = strtok_r(NULL, " ", &saveptr);
        if (token == NULL)
        {
            task_console_printf("Error: IOEXP command requires R/W parameter\r\n");
            return;
        }
        
        // CLI-003: IOEXP R ADDR
        if (strcmp(token, "R") == 0)
        {
            token = strtok_r(NULL, " ", &saveptr);
            if (token == NULL)
            {
                task_console_printf("Error: IOEXP R requires address parameter\r\n");
                return;
            }
            
            uint32_t addr;
            if (!parse_hex(token, &addr) || addr > 0xFF)
            {
                task_console_printf("Error: Invalid address format\r\n");
                return;
            }
            
            uint8_t value;
            if (system_sensors_io_expander_read(console_response_queue, (uint8_t)addr, &value))
            {
                task_console_printf("IO Expander READ: Addr=0x%02X, Value=0x%02X\r\n", 
                    (uint8_t)addr, value);
            }
            else
            {
                task_console_printf("Error: Failed to read from IO Expander\r\n");
            }
        }
        // CLI-004: IOEXP W ADDR VAL
        else if (strcmp(token, "W") == 0)
        {
            token = strtok_r(NULL, " ", &saveptr);
            if (token == NULL)
            {
                task_console_printf("Error: IOEXP W requires address parameter\r\n");
                return;
            }
            
            uint32_t addr;
            if (!parse_hex(token, &addr) || addr > 0xFF)
            {
                task_console_printf("Error: Invalid address format\r\n");
                return;
            }
            
            token = strtok_r(NULL, " ", &saveptr);
            if (token == NULL)
            {
                task_console_printf("Error: IOEXP W requires value parameter\r\n");
                return;
            }
            
            uint32_t val;
            if (!parse_hex(token, &val) || val > 0xFF)
            {
                task_console_printf("Error: Invalid value format\r\n");
                return;
            }
            
            if (system_sensors_io_expander_write(console_response_queue, (uint8_t)addr, (uint8_t)val))
            {
                task_console_printf("IO Expander WRITE: Addr=0x%02X, Value=0x%02X\r\n", 
                    (uint8_t)addr, (uint8_t)val);
            }
            else
            {
                task_console_printf("Error: Failed to write to IO Expander\r\n");
            }
        }
        else
        {
            task_console_printf("Error: Unknown IOEXP operation '%s'\r\n", token);
        }
    }
    // CLI-005 & CLI-006: EEPROM
    else if (strcmp(token, "EEPROM") == 0)
    {
        token = strtok_r(NULL, " ", &saveptr);
        if (token == NULL)
        {
            task_console_printf("Error: EEPROM command requires R/W parameter\r\n");
            return;
        }
        
        // CLI-005: EEPROM R ADDR
        if (strcmp(token, "R") == 0)
        {
            token = strtok_r(NULL, " ", &saveptr);
            if (token == NULL)
            {
                task_console_printf("Error: EEPROM R requires address parameter\r\n");
                return;
            }
            
            uint32_t addr;
            if (!parse_hex(token, &addr) || addr > 0xFFFF)
            {
                task_console_printf("Error: Invalid address format\r\n");
                return;
            }
            
            uint8_t value;
            if (system_sensors_eeprom_read(console_response_queue, (uint16_t)addr, &value))
            {
                task_console_printf("EEPROM READ: Addr=0x%04X, Value=0x%02X\r\n", 
                    (uint16_t)addr, value);
            }
            else
            {
                task_console_printf("Error: Failed to read from EEPROM\r\n");
            }
        }
        // CLI-006: EEPROM W ADDR VAL
        else if (strcmp(token, "W") == 0)
        {
            token = strtok_r(NULL, " ", &saveptr);
            if (token == NULL)
            {
                task_console_printf("Error: EEPROM W requires address parameter\r\n");
                return;
            }
            
            uint32_t addr;
            if (!parse_hex(token, &addr) || addr > 0xFFFF)
            {
                task_console_printf("Error: Invalid address format\r\n");
                return;
            }
            
            token = strtok_r(NULL, " ", &saveptr);
            if (token == NULL)
            {
                task_console_printf("Error: EEPROM W requires value parameter\r\n");
                return;
            }
            
            uint32_t val;
            if (!parse_hex(token, &val) || val > 0xFF)
            {
                task_console_printf("Error: Invalid value format\r\n");
                return;
            }
            
            if (system_sensors_eeprom_write(console_response_queue, (uint16_t)addr, (uint8_t)val))
            {
                task_console_printf("EEPROM WRITE: Addr=0x%04X, Value=0x%02X\r\n", 
                    (uint16_t)addr, (uint8_t)val);
            }
            else
            {
                task_console_printf("Error: Failed to write to EEPROM\r\n");
            }
        }
        else
        {
            task_console_printf("Error: Unknown EEPROM operation '%s'\r\n", token);
        }
    }
    else
    {
        task_console_printf("Error: Unknown command '%s'\r\n", token);
    }
}
#endif

/**
 * @brief
 * This function is the bottom half task for receiving console input.
 *
 * It waits for a task notification from the ISR indicating that a new 
 * command has been received. The task then processes the command and 
 * controls the state of the LEDs accordingly.
 *
 * @param param Unused parameter
 */
void task_console_rx(void *param)
{
    (void)param; // Unused parameter
    while (1)
    {
        /* ADD CODE */
        // wait indefinitely for a task notification
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // process the data pointed to by the console buffer pointer
        // Null-terminate the string for safe comparison
        consume_console_buffer->data[consume_console_buffer->index] = '\0';

#if defined(HW05)
        // Process HW05 commands
        process_hw05_command(consume_console_buffer->data);
#else
        // if "RED_ON" is received, turn on the red LED
        if (strcmp(consume_console_buffer->data, "RED_ON") == 0)
        {
            cyhal_gpio_write(PIN_LED_RED, true); // Turn on red LED (active high)
        }
        // if "RED_OFF" is received, turn off the red LED
        else if (strcmp(consume_console_buffer->data, "RED_OFF") == 0)
        {
            cyhal_gpio_write(PIN_LED_RED, false); // Turn off red LED (active high)
        }
        // all other commands are ignored
#endif
    }
}

/**
 * @brief
 * This function initializes the resources for the console Rx task.
 * @return true if resources were initialized successfully
 * @return false if resource initialization failed
 */
bool task_console_resources_init_rx(void)
{
    BaseType_t rslt;

    /* ADD CODE */
    // Allocate an array of data from the heap for the console buffers
    console_buffer1.data = (char *)pvPortMalloc(CONSOLE_MAX_MESSAGE_LENGTH);
    console_buffer2.data = (char *)pvPortMalloc(CONSOLE_MAX_MESSAGE_LENGTH);

    // Initialize the produce and consume pointers
    produce_console_buffer = &console_buffer1;
    consume_console_buffer = &console_buffer2;

    // Set the initial length of the console buffers to zero
    produce_console_buffer->index = 0;
    consume_console_buffer->index = 0;

#if defined(HW05)
    // Create response queue for console commands
    console_response_queue = xQueueCreate(1, sizeof(device_response_msg_t));
    if (console_response_queue == NULL)
    {
        return false;
    }
#endif

    // create the console Rx task
    rslt = xTaskCreate(task_console_rx,
                       "Task Console Rx",
                       configMINIMAL_STACK_SIZE * 5,
                       NULL,
                       tskIDLE_PRIORITY + 1,
                       &TaskHandle_Console_Rx);

    return (rslt == pdPASS); // Resources initialized successfully
}
#endif