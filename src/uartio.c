#include <msp430.h>
#include <uart.h>
#include <clock.h>

#include <uartio.h>

static uint8_t *tx_data_sync;
static unsigned tx_len_sync;
static volatile bool tx_finished_sync;
static unsigned tx_len_sync_real;

uint8_t uartio_open(uint8_t port)
{
   switch (port)
   {
   case 0:
      uart0_pins();
      break;
   case 1:
      uart1_pins();
      break;
   case 2:
      uart2_pins();
      break;
   case 3:
      uart3_pins();
      break;
   default:
      break;
   }

   // determine specifics based on 19200 baud rate
   return uartio_baud_set(port, 19200);
}

void uartio_close(uint8_t port)
{
   switch (port)
   {
   case 0:
      uart_close(0);
      break;
   case 1:
      uart_close(1);
      break;
   case 2:
      uart_close(2);
      break;
   case 3:
      uart_close(3);
      break;
   default:
      break;
   }
}

/* Tries to set the baud rate for a port.
 * Tries because it only can determine baudrate for a limited set of frequencies
 *    and only for baudrate 19200 at the moment.
 *    Acceptable SMCLK freq:
 *       8  MHz -> ovs: 1 brw: 26 brf: 0x0  brs: 0xB6
 *       4  MHz -> ovs: 1 brw: 13 brf: 0x0  brs: 0x84
 *       2  MHz -> ovs: 1 brw:  6 brf: 0x8  brs: 0x20
 *       1  MHz -> ovs: 1 brw:  3 brf: 0x4  brs: 0x02
 *       .5 MHz -> ovs: 1 brw:  1 brf: 0xA  brs: 0x00
 *       .25MHz -> ovs: 0 brw: 13 brf: 0x0  brs: 0x00
 *
 * Note: UART setup with this function uses SMCLK, but does not change it.
 *       If successful, it returns BAUD_SUCCESS, else BAUD_FAIL
 */
uint8_t uartio_baud_set(uint8_t port, uint16_t baud)
{
   uint8_t ovs = 0;
   uint16_t brw = 0;
   uint16_t brf = 0;
   uint16_t brs = 0;

   switch (sclock_freq())
   {
   case 8000000:
      ovs = 1;
      brw = 26;
      brs = 0xB6;
      break;
   case 4000000:
      ovs = 1;
      brw = 13;
      brs = 0x84;
      break;
   case 2000000:
      ovs = 1;
      brw = 6;
      brf = 0x8;
      brs = 0x20;
      break;
   case 1000000:
      ovs = 1;
      brw = 3;
      brf = 0x4;
      brs = 0x2;
      break;
   case 500000:
      ovs = 1;
      brw = 1;
      brf = 0xA;
      break;
   case 250000:
      brw = 13;
      break;
   default:
      return BAUD_FAIL;
      break;
   }

   switch (port)
   {
   case 0:
      uart_open(0, ovs, brw, brf, brs);
      break;
   case 1:
      uart_open(1, ovs, brw, brf, brs);
      break;
   case 2:
      uart_open(2, ovs, brw, brf, brs);
      break;
   case 3:
      uart_open(3, ovs, brw, brf, brs);
      break;

   default:
      break;
   }

   return BAUD_SUCCESS;
}

/* Sends payload via uart along module UART port (i.e. 0, 1, 2, 3). */
size_t uartio_send_sync(uint8_t port, uint8_t *payload, size_t len)
{
   if (len == 0)
      return 0;

   // inaccessible port
   if (port >= MODULE_NUM)
      return 0;

   // Setup pointers for the ISR
   tx_data_sync = payload;
   tx_len_sync = len;
   tx_finished_sync = false;
   tx_len_sync_real = 0;

   // fires up an interrupt cause the buffer is empty
   switch (port)
   {
   case 0:
      uart_intr_en(0, UCTXIE);
      break;
   case 1:
      uart_intr_en(1, UCTXIE);
      break;
   case 2:
      uart_intr_en(2, UCTXIE);
      break;
   case 3:
      uart_intr_en(3, UCTXIE);
      break;

   default:
      break;
   }

#ifdef UARTSLEEP
   __bis_SR_register(LPM1_bits | GIE);
#else
   _enable_interrupts();
   while (!tx_finished_sync)
      ;
#endif
   _disable_interrupts();
   return tx_len_sync_real;
}

/* sends one character via uart */
size_t uartio_putchar(uint8_t port, uint8_t c)
{
   uint8_t ch = c;
   uartio_send_sync(port, &ch, 1);
   return 1;
}

/* sends a string via uart w/ no new line*/
size_t uartio_puts_no_newline(uint8_t port, const uint8_t *ptr)
{
   size_t len = 0;
   const char *p = (const char*) ptr;

   while (*p++ != '\0')
      len++;

   size_t real_sent = uartio_send_sync(port, (uint8_t *)ptr, len);
   return real_sent;
   // return len;
}

/* sends a string via uart w/ new line */
size_t uartio_puts(uint8_t port, const uint8_t *ptr)
{
   size_t len;

   len = uartio_puts_no_newline(port, ptr);
   uartio_putchar(port, '\n');

   return len + 1;
}

#ifdef UART0_INT_ENABLE
__attribute__((interrupt(EUSCI_A0_VECTOR))) void EUSCI_A0_ISR(void)
{
   switch (__even_in_range(UCA0IV, 0x08))
   {
   case UCIV__NONE:
      break;
   case UCIV__UCRXIFG:
      break;
   case UCIV__UCTXIFG:
      UCA0TXBUF = *tx_data_sync++;
      tx_len_sync_real++;
      if (--tx_len_sync == 0)
      {
         UCA0IE &= ~UCTXIE;
         UCA0IFG &= ~UCTXCPTIFG;
         UCA0IE |= UCTXCPTIE;
      }
      break;
   case UCIV__UCSTTIFG:
      break;
   case UCIV__UCTXCPTIFG:
      UCA0IE &= ~UCTXCPTIE;
#ifdef UARTSLEEP
      LPM1_EXIT;
#else
      tx_finished_sync = true;
#endif // UARTSLEEP
      break;
   default:
      break;
   }
}
#endif // UART0_INT_ENABLE

#ifdef UART1_INT_ENABLE
__attribute__((interrupt(EUSCI_A1_VECTOR))) void EUSCI_A1_ISR(void)
{
   switch (__even_in_range(UCA1IV, 0x08))
   {
   case UCIV__NONE:
      break;
   case UCIV__UCRXIFG:
      break;
   case UCIV__UCTXIFG:
      UCA1TXBUF = *tx_data_sync++;
      if (--tx_len_sync == 0)
      {
         UCA1IE &= ~UCTXIE;
         UCA1IFG &= ~UCTXCPTIFG;
         UCA1IE |= UCTXCPTIE;
      }
      break;
   case UCIV__UCSTTIFG:
      break;
   case UCIV__UCTXCPTIFG:
      UCA1IE &= ~UCTXCPTIE;
#ifdef UARTSLEEP
      LPM1_EXIT;
#else
      tx_finished_sync = true;
#endif // UARTSLEEP
      break;
   default:
      break;
   }
}
#endif // UART1_INT_ENABLE

#ifdef UART2_INT_ENABLE
__attribute__((interrupt(EUSCI_A2_VECTOR))) void EUSCI_A2_ISR(void)
{
   switch (__even_in_range(UCA2IV, 0x08))
   {
   case UCIV__NONE:
      break;
   case UCIV__UCRXIFG:
      break;
   case UCIV__UCTXIFG:
      UCA2TXBUF = *tx_data_sync++;
      if (--tx_len_sync == 0)
      {
         UCA2IE &= ~UCTXIE;
         UCA2IFG &= ~UCTXCPTIFG;
         UCA2IE |= UCTXCPTIE;
      }
      break;
   case UCIV__UCSTTIFG:
      break;
   case UCIV__UCTXCPTIFG:
      UCA2IE &= ~UCTXCPTIE;
#ifdef UARTSLEEP
      LPM1_EXIT;
#else
      tx_finished_sync = true;
#endif // UARTSLEEP
      break;
   default:
      break;
   }
}
#endif // UART2_INT_ENABLE

#ifdef UART3_INT_ENABLE
__attribute__((interrupt(EUSCI_A3_VECTOR))) void EUSCI_A3_ISR(void)
{
   switch (__even_in_range(UCA3IV, 0x08))
   {
   case UCIV__NONE:
      break;
   case UCIV__UCRXIFG:
      break;
   case UCIV__UCTXIFG:
      UCA3TXBUF = *tx_data_sync++;
      if (--tx_len_sync == 0)
      {
         UCA3IE &= ~UCTXIE;
         UCA3IFG &= ~UCTXCPTIFG;
         UCA3IE |= UCTXCPTIE;
      }
      break;
   case UCIV__UCSTTIFG:
      break;
   case UCIV__UCTXCPTIFG:
      UCA3IE &= ~UCTXCPTIE;
#ifdef UARTSLEEP
      LPM1_EXIT;
#else
      tx_finished_sync = true;
#endif // UARTSLEEP
      break;
   default:
      break;
   }
}
#endif // UART3_INT_ENABLE
