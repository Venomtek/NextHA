/* THROWAWAY SPIKE: .AT_PROBE - set 28MHz, init UART, send "AT", print raw reply for ~2s. */
#include <stdio.h>
#include <arch/zxn.h>
#include "../src/transport/uart.h"

/* This z88dk install's zsdcc -mz80n target knows RTM_3MHZ/7MHZ/14MHZ but not
 * RTM_28MHZ (missing from its baked-in register table, even though
 * libsrc/newlib/target/zxn/config_zxn.h defines __RTM_28MHZ 0x03). Supply the
 * canonical value locally rather than touching the toolchain. */
#ifndef RTM_28MHZ
#define RTM_28MHZ 0x03
#endif

static void puts_uart(const char *s) { while (*s) uart_putc((unsigned char)*s++); }

int main(int argc, char **argv)
{
    unsigned char old_turbo = ZXN_READ_REG(REG_TURBO_MODE);
    int c, n = 0;
    (void)argc; (void)argv;
    ZXN_WRITE_REG(REG_TURBO_MODE, RTM_28MHZ);
    uart_init();
    puts_uart("AT\r\n");
    while ((c = uart_getc(100)) >= 0 && n < 200) {   /* 2s idle timeout */
        if (c >= 32 && c < 127) putchar(c); else if (c == '\n') putchar('\n');
        n++;
    }
    printf("\n[%d bytes]\n", n);
    ZXN_WRITE_REG(REG_TURBO_MODE, old_turbo);
    return 0;
}
