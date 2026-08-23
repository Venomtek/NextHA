#ifndef MOCK_UART_H
#define MOCK_UART_H

void mock_uart_reset(void);
void mock_uart_expect(const char *tx_expected, const char *rx_reply);
/* queue a step: when the DUT has written exactly tx_expected, the reply becomes readable.
   tx_expected may be "" for an unsolicited reply; rx_reply may be "" for no reply (-> timeouts). */
void mock_uart_feed(const char *rx);            /* make bytes readable immediately */
const char *mock_uart_all_tx(void);             /* everything written so far */
int  mock_uart_unmet_expectations(void);

#endif
