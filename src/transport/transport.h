#ifndef NEXTHA_TRANSPORT_H
#define NEXTHA_TRANSPORT_H

int tp_init(void);
int tp_open(const char *host, unsigned port);
int tp_write(const void *buf, unsigned len);                 /* 0 ok / negative */
/* >0 bytes, 0 = peer closed, <0 error.
   On the Next, timeout_ms bounds the wait for the next single byte; the whole exchange is
   also bounded by an overall 30-second deadline that starts when the last tp_write finished,
   so a server that dribbles bytes forever still ends in ERR_TP_TIMEOUT. Holding
   CAPS SHIFT + SPACE (BREAK) at any point aborts with ERR_TP_BREAK. */
int tp_read(void *buf, unsigned len, unsigned timeout_ms);
int tp_close(void);
void tp_shutdown(void);                                      /* release anything tp_init acquired (e.g. restore CPU speed); no-op on host */

#endif
