#include <string.h>
#include "transport.h"
#include "uart.h"
#include "../ha_errors.h"
#ifndef HOST_BUILD
#include <arch/zxn.h>
#ifndef RTM_28MHZ
#define RTM_28MHZ 0x03
#endif
static unsigned char saved_turbo;
static unsigned char turbo_saved;      /* tp_init actually ran and saved the register */
#endif

#define F_CMD   100     /* 2s */
#define F_LONG  500     /* 10s: CIPSTART, RST, SEND OK */
#define F_PROMPT 200

static unsigned char esp_ready;      /* prologue done */
static unsigned char conn_closed;    /* CLOSED seen or never opened */
static unsigned ipd_left;            /* unread payload bytes in the current +IPD */
static char lbuf[48]; static unsigned ln;

/* Overall read deadline. The per-byte uart_getc timeouts alone cannot bound a session that
   keeps dribbling bytes, so every read is also measured against the frame counter as it
   stood when the request finished going out: 1500 frames = 30 s at 50 Hz. */
#define DEADLINE_FRAMES 1500
static unsigned deadline_start;

static int deadline_expired(void) { return (unsigned)(uart_frames() - deadline_start) > DEADLINE_FRAMES; }

static void send_str(const char *s) { while (*s) uart_putc((unsigned char)*s++); }
static void send_cmd(const char *s) { send_str(s); send_str("\r\n"); }

/* Read lines until one equals ok1 or starts with ok2 (-> 1) or err (-> 0), timeout (-> -1)
   or BREAK (-> -2). ok2 is a prefix match so that one caller can accept a family of replies:
   tp_open passes "ALREADY CONNECT", which covers the NonOS firmware's "ALREADY CONNECT" and
   the SDK's "ALREADY CONNECTED". Lines longer than lbuf are truncated. */
static int wait_reply(const char *ok1, const char *ok2, unsigned frames)
{
    int c; ln = 0;
    for (;;) {
        c = uart_getc(frames);
        if (c == -2) return -2;                    /* BREAK */
        if (c < 0) return -1;
        if (c == '\n') {
            if (ln && lbuf[ln-1] == '\r') ln--;
            lbuf[ln] = 0;
            if (ln) {
                if (!strcmp(lbuf, ok1) || (ok2 && !strncmp(lbuf, ok2, strlen(ok2)))) return 1;
                if (!strcmp(lbuf, "ERROR") || !strcmp(lbuf, "FAIL") || !strcmp(lbuf, "SEND FAIL")) return 0;
            }
            ln = 0;
        } else if (ln < sizeof lbuf - 1) lbuf[ln++] = (char)c;
    }
}

static int prologue(void)
{
    int r;
    uart_flush_rx();
    send_cmd("ATE0");
    r = wait_reply("OK", 0, F_CMD);
    if (r == -2) return ERR_TP_BREAK;
    if (r < 0) return ERR_TP_ESP_NONE;
    send_cmd("AT+CIPCLOSE");
    if (wait_reply("OK", 0, F_CMD) == -2) return ERR_TP_BREAK;   /* result otherwise ignored */
    send_cmd("AT+CIPMUX=0");
    r = wait_reply("OK", 0, F_CMD);
    if (r == -2) return ERR_TP_BREAK;
    if (r != 1) return ERR_TP_ESP_INIT;
    /* AT+CIPDINFO=1 persists in the ESP's flash and would prefix every frame with the peer
       address ("+IPD,<ip>,<port>,<n>:"), which the framer does not parse. Force it off;
       firmware that does not know the command answers ERROR, which is harmless here. */
    send_cmd("AT+CIPDINFO=0");
    if (wait_reply("OK", 0, F_CMD) == -2) return ERR_TP_BREAK;   /* result otherwise ignored */
    return 0;
}

int tp_init(void)
{
    int rc, r;
#ifndef HOST_BUILD
    saved_turbo = ZXN_READ_REG(REG_TURBO_MODE);
    ZXN_WRITE_REG(REG_TURBO_MODE, RTM_28MHZ);
    turbo_saved = 1;
#endif
    uart_init();
    esp_ready = 0; conn_closed = 1; ipd_left = 0; deadline_start = uart_frames();
    rc = prologue();
    if (rc == ERR_TP_ESP_INIT || rc == ERR_TP_ESP_NONE) {
        int first = rc;                      /* spec 2.1: one AT+RST recovery on any prologue failure */
        send_cmd("AT+RST");
        r = wait_reply("WIFI GOT IP", "ready", F_LONG);
        if (r < 0) return first;
        /* ready comes first; give DHCP a chance, then carry on regardless */
        if (strcmp(lbuf, "ready") == 0 && wait_reply("WIFI GOT IP", 0, F_LONG) == -2) return ERR_TP_BREAK;
        rc = prologue();
    }
    if (rc == 0) esp_ready = 1;
    return rc;
}

void tp_shutdown(void)
{
#ifndef HOST_BUILD
    if (turbo_saved) ZXN_WRITE_REG(REG_TURBO_MODE, saved_turbo);
#endif
}

static char numbuf[8];
static const char *u2s(unsigned v) { char *p = numbuf + 7; *p = 0; do { *--p = (char)('0' + v % 10); v /= 10; } while (v); return p; }

int tp_open(const char *host, unsigned port)
{
    int r;
    if (!esp_ready) return ERR_TP_ESP_INIT;
    uart_flush_rx();
    send_str("AT+CIPSTART=\"TCP\",\""); send_str(host); send_str("\","); send_str(u2s(port)); send_str("\r\n");
    r = wait_reply("OK", "ALREADY CONNECT", F_LONG);
    if (r == -2) return ERR_TP_BREAK;
    if (r != 1) return ERR_TP_CONNECT;
    conn_closed = 0; ipd_left = 0;
    return 0;
}

#define IPD_MAX_LEN 2048

/* Scan for "+IPD,<n>:" (1 <= n <= IPD_MAX_LEN) or "CLOSED", one byte at a time. A malformed
   header (no digits, n == 0, or n too large) is discarded and scanning resumes from idle, so
   a garbled length cannot wedge frame sync or be mistaken for peer-closed (n == 0).
   ipd_st: 0 idle, 1 '+', 2 'I', 3 'P', 4 'D', 5 ',' seen (accumulating digits). */
static unsigned ipd_st, ipd_n, ipd_digits, ipd_cm;
static void ipd_reset(void) { ipd_st = 0; ipd_n = 0; ipd_digits = 0; ipd_cm = 0; }

/* Feed one byte. 0 = keep scanning, 1 = header parsed (ipd_left set), 2 = CLOSED seen
   (conn_closed set). Used both by next_frame and by the SEND OK wait in tp_write. */
static int ipd_step(int c)
{
    static const char closed[] = "CLOSED";
    if (ipd_st == 0 && c == '+') { ipd_st = 1; return 0; }
    if (ipd_st == 1 && c == 'I') { ipd_st = 2; return 0; }
    if (ipd_st == 2 && c == 'P') { ipd_st = 3; return 0; }
    if (ipd_st == 3 && c == 'D') { ipd_st = 4; return 0; }
    if (ipd_st == 4 && c == ',') { ipd_st = 5; ipd_n = 0; ipd_digits = 0; return 0; }
    if (ipd_st == 5 && c >= '0' && c <= '9') {
        ipd_n = ipd_n * 10 + (unsigned)(c - '0'); ipd_digits++;
        if (ipd_n > IPD_MAX_LEN) ipd_st = 0;    /* out of range: give up on this header */
        return 0;
    }
    if (ipd_st == 5 && c == ':') {
        if (ipd_digits >= 1 && ipd_n >= 1 && ipd_n <= IPD_MAX_LEN) { ipd_left = ipd_n; ipd_reset(); return 1; }
        ipd_st = 0;                             /* malformed header: keep scanning */
        return 0;
    }
    ipd_st = 0;
    ipd_cm = (c == closed[ipd_cm]) ? ipd_cm + 1 : (c == 'C' ? 1 : 0);
    if (ipd_cm == 6) { conn_closed = 1; ipd_reset(); return 2; }
    return 0;
}

/* Returns 1 when a frame header was parsed (ipd_left set), 0 on CLOSED, <0 on timeout/BREAK. */
static int next_frame(unsigned frames)
{
    int c, r;
    ipd_reset();
    for (;;) {
        if (deadline_expired()) return ERR_TP_TIMEOUT;
        c = uart_getc(frames);
        if (c == -2) return ERR_TP_BREAK;
        if (c < 0) return ERR_TP_TIMEOUT;
        r = ipd_step(c);
        if (r == 1) return 1;
        if (r == 2) return 0;
    }
}

int tp_write(const void *buf, unsigned len)
{
    const unsigned char *p = (const unsigned char *)buf;
    while (len) {
        unsigned n = len > 1024 ? 1024 : len, i; int c;
        send_str("AT+CIPSEND="); send_str(u2s(n)); send_str("\r\n");
        ln = 0;
        for (;;) {                                   /* wait for '>' prompt */
            c = uart_getc(F_PROMPT);
            if (c == -2) return ERR_TP_BREAK;
            if (c < 0) return ERR_TP_SEND;
            if (c == '>') break;
            if (c == '\n') { if (ln && lbuf[ln-1]=='\r') ln--; lbuf[ln] = 0; if (!strcmp(lbuf, "ERROR")) return ERR_TP_SEND; ln = 0; }
            else if (ln < sizeof lbuf - 1) lbuf[ln++] = (char)c;
        }
        for (i = 0; i < n; i++) if (uart_putc(p[i])) return ERR_TP_SEND;
        /* Wait for SEND OK, while also watching for a frame header: on a fast LAN the ESP's
           recv callback can fire before its sent callback, so "+IPD,<n>:" may arrive first.
           Data coming back means the send got through, so treat a parsed header as success
           and leave the payload in the UART for tp_read. CLOSED here means it did not. */
        ln = 0; ipd_reset();
        for (;;) {
            int r;
            c = uart_getc(F_LONG);
            if (c == -2) return ERR_TP_BREAK;
            if (c < 0) return ERR_TP_SEND;
            r = ipd_step(c);
            /* a header here means the send arrived - but only trust it if this was the
               last slice; with bytes still unsent the request is incomplete either way */
            if (r == 1) { if (len != n) return ERR_TP_SEND; deadline_start = uart_frames(); return 0; }
            if (r == 2) return ERR_TP_SEND;
            if (c == '\n') {
                if (ln && lbuf[ln-1]=='\r') ln--;
                lbuf[ln] = 0;
                if (!strcmp(lbuf, "SEND OK")) break;
                if (!strcmp(lbuf, "SEND FAIL") || !strcmp(lbuf, "ERROR")) return ERR_TP_SEND;
                ln = 0;
            }
            else if (ln < sizeof lbuf - 1) lbuf[ln++] = (char)c;
        }
        p += n; len -= n;
    }
    deadline_start = uart_frames();       /* the response clock starts when the request is out */
    return 0;
}

int tp_read(void *buf, unsigned len, unsigned timeout_ms)
{
    unsigned char *p = (unsigned char *)buf; unsigned n = 0; int c;
    unsigned frames = timeout_ms / 20 + 1;
    if (!ipd_left) {
        if (conn_closed) return 0;
        c = next_frame(frames);
        if (c <= 0) return c;
    }
    while (n < len && ipd_left) {
        if (deadline_expired()) return ERR_TP_TIMEOUT;
        c = uart_getc(frames);
        if (c == -2) return ERR_TP_BREAK;
        if (c < 0) return n ? (int)n : ERR_TP_TIMEOUT;
        p[n++] = (unsigned char)c; ipd_left--;
    }
    return (int)n;
}

int tp_close(void)
{
    /* Always sent: harmless ERROR if already CLOSED; keeps ESP state predictable. */
    send_cmd("AT+CIPCLOSE"); (void)wait_reply("OK", "CLOSED", F_CMD);
    conn_closed = 1; ipd_left = 0;
    return 0;
}
