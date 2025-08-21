#include <libmsp/mspbase.h>
#include <libmspio/i2cio.h>

/* Used to track the state of the software state machine*/
I2C_Mode ControllerMode = IDLE_MODE;

/* The Register Address/Command to use*/
uint16_t TransmitRegAddr = 0;

/* ReceiveBuffer: Buffer used to receive data in the ISR
 * RXByteCtr: Number of bytes left to receive
 * ReceiveIndex: The index of the next byte to be received in ReceiveBuffer
 * TransmitBuffer: Buffer used to transmit data in the ISR
 * TXByteCtr: Number of bytes left to transfer
 * TransmitIndex: The index of the next byte to be transmitted in TransmitBuffer
 * */
uint8_t ReceiveBuffer[MAX_BUFFER_SIZE] = {0};
uint8_t RXByteCtr = 0;
uint8_t ReceiveIndex = 0;
uint8_t TransmitBuffer[MAX_BUFFER_SIZE] = {0};
uint8_t TXByteCtr = 0;
uint8_t TransmitIndex = 0;

// TODO: Set this up for the pin you're using, we only support port 2 rn
void initI2C(uint8_t port) {
  // I2C pins
  uint16_t div = i2c_clock_div();
  i2c2_pins();
  i2c_init(2, DEFAULT_ADDR, div);
}

I2C_Mode I2C_ReadReg(uint8_t dev_addr, uint16_t reg_addr, uint8_t count) {
  /* Initialize state machine */
  ControllerMode = TX_REG_ADDRESS_LOW_MODE;
  TransmitRegAddr = reg_addr;
  RXByteCtr = count;
  TXByteCtr = 0;
  ReceiveIndex = 0;
  TransmitIndex = 0;

  /* Initialize target address and interrupts */
  i2c_addr(2, dev_addr);
  i2c_clear_int(2);                   // Clear any pending interrupts
  i2c_disable_rxint(2);               // Disable RX interrupt
  i2c_enable_txint(2);                // Enable TX interrupt
  i2c_tx_start(2);                    // I2C TX, start condition
  __bis_SR_register(LPM0_bits + GIE); // Enter LPM0 w/ interrupts

  return ControllerMode;
}

I2C_Mode I2C_WriteReg(uint8_t dev_addr, uint16_t reg_addr, uint8_t *reg_data,
                      uint8_t count) {
  /* Initialize state machine */
  ControllerMode = TX_REG_ADDRESS_LOW_MODE;
  TransmitRegAddr = reg_addr;

  // Copy register data to TransmitBuffer
  CopyArray(reg_data, TransmitBuffer, count);

  TXByteCtr = count;
  RXByteCtr = 0;
  ReceiveIndex = 0;
  TransmitIndex = 0;

  /* Initialize target address and interrupts */
  i2c_addr(2, dev_addr);
  i2c_clear_int(2);                   // Clear any pending interrupts
  i2c_disable_rxint(2);               // Disable RX interrupt
  i2c_enable_txint(2);                // Enable TX interrupt
  i2c_tx_start(2);                    // I2C TX, start condition
  __bis_SR_register(LPM0_bits + GIE); // Enter LPM0 w/ interrupts

  return ControllerMode;
}

void CopyArray(uint8_t *source, uint8_t *dest, uint8_t count) {
  uint8_t copyIndex = 0;
  for (copyIndex = 0; copyIndex < count; copyIndex++) {
    dest[copyIndex] = source[copyIndex];
  }
}

void __attribute__((interrupt(USCI_B2_VECTOR))) USCI_B2_ISR(void) {
  // Must read from UCB2RXBUF
  uint8_t rx_val = 0;
  switch (__even_in_range(UCB2IV, USCI_I2C_UCBIT9IFG)) {
  // switch (UCB2IV) {
  case USCI_NONE:
    break; // Vector 0: No interrupts
  case USCI_I2C_UCALIFG:
    break;                 // Vector 2: ALIFG
  case USCI_I2C_UCNACKIFG: // Vector 4: NACKIFG
    break;
  case USCI_I2C_UCSTTIFG:
    break; // Vector 6: STTIFG
  case USCI_I2C_UCSTPIFG:
    break; // Vector 8: STPIFG
  case USCI_I2C_UCRXIFG3:
    break; // Vector 10: RXIFG3
  case USCI_I2C_UCTXIFG3:
    break; // Vector 12: TXIFG3
  case USCI_I2C_UCRXIFG2:
    break; // Vector 14: RXIFG2
  case USCI_I2C_UCTXIFG2:
    break; // Vector 16: TXIFG2
  case USCI_I2C_UCRXIFG1:
    break; // Vector 18: RXIFG1
  case USCI_I2C_UCTXIFG1:
    break;                // Vector 20: TXIFG1
  case USCI_I2C_UCRXIFG0: // Vector 22: RXIFG0
    i2c_rxbuf(2, rx_val);
    if (RXByteCtr) {
      ReceiveBuffer[ReceiveIndex++] = rx_val;
      RXByteCtr--;
    }

    if (RXByteCtr == 1) {
      i2c_stop(2);
    } else if (RXByteCtr == 0) {
      i2c_disable_rxint(2);
      ControllerMode = IDLE_MODE;
      _bic_SR_register_on_exit(LPM0_bits); // Exit LPM0
    }
    break;
  case USCI_I2C_UCTXIFG0: // Vector 24: TXIFG0
    switch (ControllerMode) {
    case TX_REG_ADDRESS_LOW_MODE:
      i2c_txbuf(2, (uint8_t)((TransmitRegAddr >> 8) & 0xff));
      ControllerMode = TX_REG_ADDRESS_HIGH_MODE; // Continue to transmision of
                                                 // the high byte of the address
      break;

    case TX_REG_ADDRESS_HIGH_MODE:
      i2c_txbuf(2, (uint8_t)((TransmitRegAddr) & 0xff));
      if (RXByteCtr) {
        ControllerMode = SWITCH_TO_RX_MODE; // Need to start receiving now
      } else {
        ControllerMode = TX_DATA_MODE; // Continue to transmision with the data
      }
      // in Transmit Buffer
      break;

    case SWITCH_TO_RX_MODE:
      i2c_enable_rxint(2);
      i2c_disable_txint(2);
      i2c_rx(2);                     // Switch to receiver
      ControllerMode = RX_DATA_MODE; // State state is to receive data
      i2c_repeated(2);               // Send repeated start
      if (RXByteCtr == 1) {
        // Must send stop since this is the N-1 byte
        i2c_wait_sttbusy(2);
        i2c_stop(2); // Send stop condition
      }
      break;

    case TX_DATA_MODE:
      if (TXByteCtr) {
        i2c_txbuf(2, TransmitBuffer[TransmitIndex++]);
        TXByteCtr--;
      } else {
        // Done with transmission
        i2c_stop(2);                         // Send stop condition
        i2c_disable_txint(2);                // disable TX interrupt
        _bic_SR_register_on_exit(LPM0_bits); // Exit LPM0
        ControllerMode = IDLE_MODE;
      }
      break;

    default:
      __no_operation();
      break;
    }
    break;
  default:
    break;
  }
}