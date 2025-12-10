/*
 * @file rtos_events.c
 * @brief Define the global FreeRTOS event group handle used across the project
 */

#include "main.h"

#if defined(ECE353_FREERTOS)

/* Definition of the global event group handle declared as extern in headers */
EventGroupHandle_t ECE353_RTOS_Events = NULL;

#endif
