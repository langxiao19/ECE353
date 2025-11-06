/**
 * @file eeprom.c
 * @author Joe Krachey (jkrachey@wisc.edu)
 * @brief 
 * @version 0.1
 * @date 2023-10-24
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include "eeprom.h"
#include "cyhal_hw_types.h"
#include <sys/types.h>


/** Determine if the EEPROM is busy writing the last
 *  transaction to non-volatile storage
 *
 * @param
 *
 */
void eeprom_wait_for_write(cyhal_spi_t *spi_obj, cyhal_gpio_t cs_pin)
{
	uint8_t tx_buffer[2];
	uint8_t rx_buffer[2];
	uint8_t status;
	
	// Keep reading status register until WIP bit (bit 0) is cleared
	do {
		tx_buffer[0] = EEPROM_CMD_RDSR;  // Read Status Register command
		tx_buffer[1] = 0x00;             // Dummy byte to clock out status
		
		cyhal_gpio_write(cs_pin, 0);     // Assert CS
		cyhal_spi_transfer(spi_obj, tx_buffer, 2, rx_buffer, 2, 0xFF);
		cyhal_gpio_write(cs_pin, 1);     // Deassert CS
		
		status = rx_buffer[1];           // Status is in second byte received
	} while (status & 0x01);             // Check WIP bit (bit 0)
}

/** Enables Writes to the EEPROM
 *
 * @param
 *
 */
void eeprom_write_enable(cyhal_spi_t *spi_obj, cyhal_gpio_t cs_pin)
{
	uint8_t tx_buffer[1];
	
	tx_buffer[0] = EEPROM_CMD_WREN;  // Write Enable command
	
	cyhal_gpio_write(cs_pin, 0);     // Assert CS
	cyhal_spi_transfer(spi_obj, tx_buffer, 1, NULL, 0, 0xFF);
	cyhal_gpio_write(cs_pin, 1);     // Deassert CS
}

/** Disable Writes to the EEPROM
 *
 * @param
 *
 */
void eeprom_write_disable(cyhal_spi_t *spi_obj, cyhal_gpio_t cs_pin)
{
	uint8_t tx_buffer[1];
	
	tx_buffer[0] = EEPROM_CMD_WRDI;  // Write Disable command
	
	cyhal_gpio_write(cs_pin, 0);     // Assert CS
	cyhal_spi_transfer(spi_obj, tx_buffer, 1, NULL, 0, 0xFF);
	cyhal_gpio_write(cs_pin, 1);     // Deassert CS
}

/** Writes a single byte to the specified address
 *
 * @param address -- 16 bit address in the EEPROM
 * @param data    -- value to write into memory
 *
 */
void eeprom_write_byte(cyhal_spi_t *spi_obj, cyhal_gpio_t cs_pin, uint16_t address, uint8_t data)
{
	uint8_t tx_buffer[4];
	
	// Enable writes first
	eeprom_write_enable(spi_obj, cs_pin);
	
	// Build the write command: WRITE + 16-bit address + data byte
	tx_buffer[0] = EEPROM_CMD_WRITE;         // Write command
	tx_buffer[1] = (address >> 8) & 0xFF;    // Address MSB
	tx_buffer[2] = address & 0xFF;           // Address LSB
	tx_buffer[3] = data;                     // Data byte
	
	cyhal_gpio_write(cs_pin, 0);             // Assert CS
	cyhal_spi_transfer(spi_obj, tx_buffer, 4, NULL, 0, 0xFF);
	cyhal_gpio_write(cs_pin, 1);             // Deassert CS
	
	// Wait for write to complete
	eeprom_wait_for_write(spi_obj, cs_pin);
}

/** Reads a single byte to the specified address
 *
 * @param address -- 16 bit address in the EEPROM
 *
 */
uint8_t eeprom_read_byte(cyhal_spi_t *spi_obj, cyhal_gpio_t cs_pin, uint16_t address)
{
	uint8_t tx_buffer[4];
	uint8_t rx_buffer[4];
	
	// Build the read command: READ + 16-bit address + dummy byte
	tx_buffer[0] = EEPROM_CMD_READ;          // Read command
	tx_buffer[1] = (address >> 8) & 0xFF;    // Address MSB
	tx_buffer[2] = address & 0xFF;           // Address LSB
	tx_buffer[3] = 0x00;                     // Dummy byte to clock out data
	
	cyhal_gpio_write(cs_pin, 0);             // Assert CS
	cyhal_spi_transfer(spi_obj, tx_buffer, 4, rx_buffer, 4, 0xFF);
	cyhal_gpio_write(cs_pin, 1);             // Deassert CS
	
	return rx_buffer[3];                     // Data is in 4th byte received
}