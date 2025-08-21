#ifndef INCLUDE_MSPIO_I2CIO_H
#define INCLUDE_MSPIO_I2CIO_H

#include <libmsp/clock.h>
#include <libmsp/i2c.h>
#include <stddef.h>

//******************************************************************************
// General I2C State Machine ***************************************************
// Adopted from the EWSN 24 compteition
//******************************************************************************

#define MAX_BUFFER_SIZE 32
#define DEFAULT_ADDR 0x48

typedef enum I2C_ModeEnum {
  IDLE_MODE,
  NACK_MODE,
  TX_REG_ADDRESS_LOW_MODE,
  TX_REG_ADDRESS_HIGH_MODE,
  RX_REG_ADDRESS_MODE,
  TX_DATA_MODE,
  RX_DATA_MODE,
  SWITCH_TO_RX_MODE,
  SWITHC_TO_TX_MODE,
  TIMEOUT_MODE
} I2C_Mode;

/* I2C Write and Read Functions */

/* For target device with dev_addr, writes the data specified in *reg_data
 *
 * dev_addr: The target device address.
 *           Example: 0x50
 * reg_addr: The register to send to the target.
 *           Example: 0x0
 * *reg_data: The buffer to write
 *           Example: uint8_t buffer[20]
 * count: The length of *reg_data
 *           Example: 20
 *  */
I2C_Mode I2C_WriteReg(uint8_t dev_addr, uint16_t reg_addr, uint8_t *reg_data,
                      uint8_t count);

/* For target device with dev_addr, read the data specified in targets reg_addr.
 * The received data is available in ReceiveBuffer
 *
 * dev_addr: The target device address.
 *           Example: 0x50
 * reg_addr: The register to send to the target.
 *           Example: 0x0
 * count: The length of data to read
 *           Example: 1
 *  */
I2C_Mode I2C_ReadReg(uint8_t dev_addr, uint16_t reg_addr, uint8_t count);

void CopyArray(uint8_t *source, uint8_t *dest, uint8_t count);

void initI2C(uint8_t port);

/* ReceiveBuffer: Buffer used to receive data in the ISR */
extern uint8_t ReceiveBuffer[];
/* Used to track the state of the software state machine*/
extern I2C_Mode ControllerMode;

#endif /* INCLUDE_MSPIO_I2CIO_H */