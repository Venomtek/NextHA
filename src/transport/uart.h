#ifndef NEXTHA_UART_H
#define NEXTHA_UART_H
/* Byte-level UART primitives. Next impl: uart_zxn.c. Host tests: test/mock_uart.c */
void uart_init(void);
int  uart_putc(unsigned char c);
int  uart_getc(unsigned timeout_frames);   /* byte 0-255, -1 timeout, -2 BREAK (CAPS SHIFT + SPACE) */
void uart_flush_rx(void);
unsigned uart_frames(void);                /* free-running 50 Hz frame counter, for overall deadlines */
#endif
