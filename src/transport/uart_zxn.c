#include <arch/zxn.h>
#include <arch/zxn/sysvar.h>
#include <z80.h>
#include "uart.h"
#include "../ha_errors.h"

__sfr __banked __at 0x153b IO_UART_SELECT;

/* 115200 baud prescaler per video timing mode 0..7 (nextreg 0x11 & 7). From .HTTP uart.asm. */
static const unsigned int baud_115200[8] = { 243, 248, 256, 260, 269, 278, 286, 234 };

/* FRAMES is incremented by the 50 Hz interrupt, so the two halves can disagree while it
   carries: read hi, lo, hi and re-read both if the high byte moved under us.
   The read must go through a volatile view - z88dk declares SYSVAR_FRAMES as a plain
   unsigned char[3], so at -SO3 zsdcc otherwise caches the first high-byte load and
   compares the value against itself, which optimises the guard away entirely. */
static volatile unsigned char *const fr = SYSVAR_FRAMES;

static unsigned int frames16(void)
{
    unsigned char hi, lo;
    hi = fr[1];
    lo = fr[0];
    while (fr[1] != hi) { hi = fr[1]; lo = fr[0]; }
    return (unsigned int)lo | ((unsigned int)hi << 8);
}

unsigned uart_frames(void) { return frames16(); }

/* CAPS SHIFT + SPACE, the Spectrum's BREAK: half-row 0xfefe bit 0 and 0x7ffe bit 0, both low. */
static unsigned char break_pressed(void)
{
    return (unsigned char)(!(z80_inp(0xfefe) & 1) && !(z80_inp(0x7ffe) & 1));
}

void uart_init(void)
{
    unsigned char timing = ZXN_READ_REG(REG_VIDEO_TIMING) & 7;
    unsigned int  pre    = baud_115200[timing];

    IO_UART_SELECT = 0x20;                          /* select ESP uart, as .HTTP does */
    IO_UART_BAUD_RATE = (unsigned char)(pre & 0x7f);          /* low 7 bits */
    IO_UART_BAUD_RATE = (unsigned char)(0x80 | (pre >> 7));   /* high bits, bit7 = upper write */
    uart_flush_rx();
}

int uart_putc(unsigned char c)
{
    unsigned int start = frames16();
    while (IO_UART_STATUS & IUS_TX_BUSY) {
        if ((unsigned int)(frames16() - start) > 50) return ERR_TP_TIMEOUT;
    }
    IO_UART_TX = c;
    return 0;
}

int uart_getc(unsigned timeout_frames)
{
    unsigned int start = frames16();
    for (;;) {
        if (IO_UART_STATUS & IUS_RX_AVAIL) return IO_UART_RX;
        if (break_pressed()) return -2;
        if ((unsigned int)(frames16() - start) > timeout_frames) return -1;
    }
}

void uart_flush_rx(void)
{
    while (IO_UART_STATUS & IUS_RX_AVAIL) (void)IO_UART_RX;
}
