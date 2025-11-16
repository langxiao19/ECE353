/**
 * @file io_expander.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-09-01
 *
 * @copyright Copyright (c) 2023
 *
 */
#include "main.h"
#if defined(ECE353_FREERTOS)
#include "cyhal_gpio.h"
#include "task_io_expander.h"
#include "task_console.h"
#include "rtos_events.h"
#include "devices.h"

#define TASK_IO_EXPANDER_STACK_SIZE (configMINIMAL_STACK_SIZE)
#define TASK_IO_EXPANDER_PRIORITY   (tskIDLE_PRIORITY + 1)

/******************************************************************************/
/* Function Declarations                                                      */
/******************************************************************************/
static void task_io_expander(void *param);
static void handler_io_expander_button(void *arg, cyhal_gpio_event_t event);

/******************************************************************************/
/* Global Variables                                                           */
/******************************************************************************/
static cyhal_i2c_t *I2C_Obj;
static SemaphoreHandle_t *I2C_Semaphore = NULL;

/* Queue used to send commands used to io expander */
QueueHandle_t Queue_IO_Expander_Requests;

/******************************************************************************/
/* Static Function Definitions                                                */
/******************************************************************************/

/******************************************************************************/
/* Public Function Definitions                                                */
/******************************************************************************/

bool system_sensors_io_expander_write(QueueHandle_t return_queue, uint8_t address, uint8_t value)
{
	bool status = false;
	device_request_msg_t msg;
	device_response_msg_t response;

	/* Populate the message */
	msg.device = DEVICE_IO_EXP;
	msg.operation = DEVICE_OP_WRITE;
	msg.address = address;
	msg.value = value;
	msg.response_queue = return_queue;

	/* Send the message to the IO Expander task */
	if (xQueueSend(Queue_IO_Expander_Requests, &msg, portMAX_DELAY) != pdPASS)
	{
		return false;
	}

	/* Wait for the response if a return queue was provided */
	if (return_queue != NULL)
	{
		if (xQueueReceive(return_queue, &response, portMAX_DELAY) == pdPASS)
		{
			status = (response.status == DEVICE_OPERATION_STATUS_WRITE_SUCCESS);
		}
	}
	else
	{
		status = true;  // Request sent successfully, not waiting for response
	}

	return status;
}

bool system_sensors_io_expander_read(QueueHandle_t return_queue, uint8_t address, uint8_t *value)
{
	bool status = true;
	device_request_msg_t msg;
	device_response_msg_t response;

	/* Populate the message */
	msg.device = DEVICE_IO_EXP;
	msg.operation = DEVICE_OP_READ;
	msg.address = address;
	msg.response_queue = return_queue;

	/* Send the message to the IO Expander task */
	if (xQueueSend(Queue_IO_Expander_Requests, &msg, portMAX_DELAY) != pdPASS)
	{
		status = false;
	}

	/* Wait for the response if a return queue was provided */
	if (return_queue != NULL && status)
	{
		if (xQueueReceive(return_queue, &response, portMAX_DELAY) == pdPASS)
		{
			if (response.status == DEVICE_OPERATION_STATUS_READ_SUCCESS)
			{
				*value = response.payload.io_expander;
			}
			else
			{
				status = false;
			}
		}
		else
		{
			status = false;
		}
	}

	return status;
}

/**
 * @brief
 * Task used to monitor the reception of command packets sent the io expander
 * @param param
 * Unused
 */
void task_io_expander(void *param)
{
	device_request_msg_t request;
	device_response_msg_t response;
	cy_rslt_t result;

	while (1)
	{
		/* Wait for a message from the queue */
		if (xQueueReceive(Queue_IO_Expander_Requests, &request, portMAX_DELAY) == pdPASS)
		{
			/* Check if the device type is correct */
			if (request.device != DEVICE_IO_EXP)
			{
				task_console_printf("Error: Invalid device type for IO Expander task\r\n");
				continue;
			}

			/* Validate the register address */
			if (request.address != IOXP_ADDR_INPUT_PORT &&
			    request.address != IOXP_ADDR_OUTPUT_PORT &&
			    request.address != IOXP_ADDR_POLARITY &&
			    request.address != IOXP_ADDR_CONFIG)
			{
				task_console_printf("Error: Invalid register address 0x%02X for IO Expander\r\n", request.address);
				continue;
			}

			/* Take the I2C semaphore */
			if (xSemaphoreTake(*I2C_Semaphore, portMAX_DELAY) == pdTRUE)
			{
				if (request.operation == DEVICE_OP_WRITE)
				{
					/* Write to the IO Expander */
					result = i2c_write_u8(I2C_Obj, TCA9534_SUBORDINATE_ADDR, request.address, request.value);
					
					/* Send response if a response queue was provided */
					if (request.response_queue != NULL)
					{
						response.device = DEVICE_IO_EXP;
						response.status = (result == CY_RSLT_SUCCESS) ? 
						                  DEVICE_OPERATION_STATUS_WRITE_SUCCESS : 
						                  DEVICE_OPERATION_STATUS_WRITE_FAILURE;
						xQueueSend(request.response_queue, &response, portMAX_DELAY);
					}
				}
				else if (request.operation == DEVICE_OP_READ)
				{
					/* Read from the IO Expander */
					uint8_t read_value = 0;
					result = i2c_read_u8(I2C_Obj, TCA9534_SUBORDINATE_ADDR, request.address, &read_value);
					
					/* Send response if a response queue was provided */
					if (request.response_queue != NULL)
					{
						response.device = DEVICE_IO_EXP;
						response.status = (result == CY_RSLT_SUCCESS) ? 
						                  DEVICE_OPERATION_STATUS_READ_SUCCESS : 
						                  DEVICE_OPERATION_STATUS_READ_FAILURE;
						response.payload.io_expander = read_value;
						xQueueSend(request.response_queue, &response, portMAX_DELAY);
					}
				}
				else
				{
					task_console_printf("Error: Invalid operation type for IO Expander\r\n");
				}

				/* Release the I2C semaphore */
				xSemaphoreGive(*I2C_Semaphore);
			}
		}
	}
}

/**
 * @brief
 * Initializes software resources related to the operation of
 * the IO Expander.  This function expects that the I2C bus had already
 * been initialized prior to the start of FreeRTOS.
 */
bool task_io_expander_resources_init(cyhal_i2c_t *i2c_obj, SemaphoreHandle_t *i2c_semaphore)
{
	/* Save the I2C object and semaphore */
	I2C_Obj = i2c_obj;
	I2C_Semaphore = i2c_semaphore;
	if (I2C_Semaphore == NULL)
	{
		return false;
	}

	/* Create the Queue used to control blinking of the status LED*/
	Queue_IO_Expander_Requests = xQueueCreate(1, sizeof(device_request_msg_t));
	if (Queue_IO_Expander_Requests == NULL)
	{
		return false;
	}

	/* Create the task that will control the status LED */
	if(xTaskCreate(
		task_io_expander,
		"Task IO Exp",
		TASK_IO_EXPANDER_STACK_SIZE,
		i2c_semaphore,
		TASK_IO_EXPANDER_PRIORITY,
		NULL) != pdPASS)
	{
		return false;
	}
	else 
	{
		return true;
	}	
}	
#endif