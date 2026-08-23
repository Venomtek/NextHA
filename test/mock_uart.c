#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mock_uart.h"
#include "transport/uart.h"

#define MAXSTEPS 32
#define MOCK_CAP 4096
static struct { const char *tx; const char *rx; } steps[MAXSTEPS];
static unsigned nsteps, cur;
static char rxq[MOCK_CAP + 1]; static unsigned rx_len, rx_pos;
static char txlog[MOCK_CAP + 1]; static unsigned tx_len;
/* Mock clock: there is no 50 Hz interrupt here, so uart_frames() simply advances one
   "frame" per call. esp_at calls it once per byte read, which makes its 30 s (1500 frame)
   overall read deadline reachable deterministically in a test. Reset with the mock. */
static unsigned frame_ticks;

void mock_uart_reset(void) { nsteps = cur = rx_len = rx_pos = tx_len = frame_ticks = 0; txlog[0] = 0; }
void mock_uart_expect(const char *tx, const char *rx) { steps[nsteps].tx = tx; steps[nsteps].rx = rx; nsteps++; }
void mock_uart_feed(const char *rx) {
    size_t n = strlen(rx);
    if (rx_len + n > MOCK_CAP) {
        fprintf(stderr, "mock_uart: rxq overflow (%u + %u > %u)\n", rx_len, (unsigned)n, MOCK_CAP);
        abort();
    }
    memcpy(rxq + rx_len, rx, n); rx_len += (unsigned)n;
}
const char *mock_uart_all_tx(void) { txlog[tx_len] = 0; return txlog; }
int  mock_uart_unmet_expectations(void) { return (int)(nsteps - cur); }

static void check_steps(void)
{
    /* cur advances past every step whose tx is a prefix-match at the tx log's tail: a
       non-empty tx match feeds its rx and stops (one matched step per write); a run of
       empty ("") unsolicited tx steps immediately following it is drained in the same
       call, since they need no write of their own to become due. */
    while (cur < nsteps) {
        size_t tl = strlen(steps[cur].tx);
        if (tl == 0 || (tx_len >= tl && memcmp(txlog + tx_len - tl, steps[cur].tx, tl) == 0)) {
            mock_uart_feed(steps[cur].rx); cur++;
            if (tl) break;      /* one matched step per write; unsolicited ones drain in a row */
        } else break;
    }
}

void uart_init(void) { check_steps(); }
int  uart_putc(unsigned char c) {
    if (tx_len >= MOCK_CAP) {
        fprintf(stderr, "mock_uart: txlog overflow (>= %u)\n", MOCK_CAP);
        abort();
    }
    txlog[tx_len++] = (char)c; check_steps(); return 0;
}
int  uart_getc(unsigned timeout_frames) { (void)timeout_frames; if (rx_pos < rx_len) return (unsigned char)rxq[rx_pos++]; return -1; }  /* never returns -2: the mock has no keyboard */
unsigned uart_frames(void) { return frame_ticks++; }
void uart_flush_rx(void) { rx_pos = rx_len; }
