#ifndef INCLUDE_MSPIO_UARTIO_H
#define INCLUDE_MSPIO_UARTIO_H

#include <stdint.h>
#define BAUD_SUCCESS 1
#define BAUD_FAIL 0

/* This library provides a general view of using the uart to send and recieve
 *     information. It is not extremely efficient since it's general.
 *
 *    To enable LPM when waiting, define UARTSLEEP
 *
 *    Each module has a different interrupt handler. To enable one, define
 *       UARTx_INT_ENABLE, where x is 0-3.
 *
 *    NOTE: ENABLE AT BUILD TIME.
 *
 */

#define PUTC(p, c) uartio_putchar(p, c)

uint8_t uartio_open(uint8_t port);
void uartio_close(uint8_t port);
uint8_t uartio_baud_set(uint8_t port, int baud);

void uartio_send_sync(uint8_t port, uint8_t *payload, unsigned len);

int uartio_putchar(uint8_t port, int c);
int uartio_puts_no_newline(uint8_t port, const char *ptr);
int uartio_puts(uint8_t port, const char *ptr);



#endif /* INCLUDE_MSPIO_UARTIO_H */