#include <string.h>
#include "ztest.h"
#include "mock_uart.h"
#include "transport/transport.h"
#include "ha_errors.h"

static void prologue_ok(void) {
    mock_uart_expect("ATE0\r\n", "ATE0\r\r\n\r\nOK\r\n");
    mock_uart_expect("AT+CIPCLOSE\r\n", "\r\nERROR\r\n");
    mock_uart_expect("AT+CIPMUX=0\r\n", "\r\nOK\r\n");
    mock_uart_expect("AT+CIPDINFO=0\r\n", "\r\nOK\r\n");
}

void test_esp_init_ok(void)
{
    mock_uart_reset(); prologue_ok();
    ZT_EQ_INT(tp_init(), 0);
    ZT_EQ_INT(mock_uart_unmet_expectations(), 0);
    ZT_EQ_STR(mock_uart_all_tx(), "ATE0\r\nAT+CIPCLOSE\r\nAT+CIPMUX=0\r\nAT+CIPDINFO=0\r\n");
}
void test_esp_init_no_esp(void)
{
    mock_uart_reset();                       /* nothing ever answers */
    ZT_EQ_INT(tp_init(), ERR_TP_ESP_NONE);
    ZT_EQ_STR(mock_uart_all_tx(), "ATE0\r\nAT+RST\r\n");   /* one reset attempt, still nothing */
}
void test_esp_init_no_reply_then_reset_recovery(void)
{
    mock_uart_reset();
    mock_uart_expect("ATE0\r\n", "");                      /* ESP asleep: no reply at all */
    mock_uart_expect("AT+RST\r\n", "\r\nready\r\nWIFI GOT IP\r\n");
    prologue_ok();
    ZT_EQ_INT(tp_init(), 0);
    ZT_EQ_INT(mock_uart_unmet_expectations(), 0);
}
void test_esp_init_reset_recovery(void)
{
    mock_uart_reset();
    mock_uart_expect("ATE0\r\n", "\r\nOK\r\n");
    mock_uart_expect("AT+CIPCLOSE\r\n", "\r\nERROR\r\n");
    mock_uart_expect("AT+CIPMUX=0\r\n", "\r\nERROR\r\n");
    mock_uart_expect("AT+RST\r\n", "\r\nOK\r\nready\r\nWIFI CONNECTED\r\nWIFI GOT IP\r\n");
    prologue_ok();
    ZT_EQ_INT(tp_init(), 0);
    ZT_EQ_INT(mock_uart_unmet_expectations(), 0);
}
void test_esp_open_send_read_close(void)
{
    char buf[32]; int n;
    mock_uart_reset(); prologue_ok(); tp_init();
    mock_uart_expect("AT+CIPSTART=\"TCP\",\"ha.local\",8123\r\n", "CONNECT\r\n\r\nOK\r\n");
    ZT_EQ_INT(tp_open("ha.local", 8123), 0);
    mock_uart_expect("AT+CIPSEND=5\r\n", "\r\nOK\r\n> ");
    mock_uart_expect("HELLO", "\r\nRecv 5 bytes\r\n\r\nSEND OK\r\n\r\n+IPD,11:hello worldCLOSED\r\n");
    ZT_EQ_INT(tp_write("HELLO", 5), 0);
    n = tp_read(buf, 4, 1000);  ZT_EQ_INT(n, 4);  ZT_EQ_MEM(buf, "hell", 4);
    n = tp_read(buf, 32, 1000); ZT_EQ_INT(n, 7);  ZT_EQ_MEM(buf, "o world", 7);
    n = tp_read(buf, 32, 1000); ZT_EQ_INT(n, 0);            /* CLOSED */
    mock_uart_expect("AT+CIPCLOSE\r\n", "\r\nOK\r\n");
    ZT_EQ_INT(tp_close(), 0);
    ZT_EQ_INT(mock_uart_unmet_expectations(), 0);
}
void test_esp_open_fails(void)
{
    mock_uart_reset(); prologue_ok(); tp_init();
    mock_uart_expect("AT+CIPSTART=\"TCP\",\"nohost\",80\r\n", "DNS Fail\r\n\r\nERROR\r\n");
    ZT_EQ_INT(tp_open("nohost", 80), ERR_TP_CONNECT);
}
void test_esp_send_fail_and_read_timeout(void)
{
    char buf[8];
    mock_uart_reset(); prologue_ok(); tp_init();
    mock_uart_expect("AT+CIPSTART=\"TCP\",\"h\",80\r\n", "\r\nOK\r\n");
    tp_open("h", 80);
    mock_uart_expect("AT+CIPSEND=2\r\n", "\r\nERROR\r\n");
    ZT_EQ_INT(tp_write("ab", 2), ERR_TP_SEND);
    ZT_EQ_INT(tp_read(buf, 8, 100), ERR_TP_TIMEOUT);       /* nothing arrives */
}
void test_esp_ipd_split_frames(void)
{
    char buf[32]; int n;
    mock_uart_reset(); prologue_ok(); tp_init();
    mock_uart_expect("AT+CIPSTART=\"TCP\",\"h\",80\r\n", "\r\nOK\r\n");
    tp_open("h", 80);
    mock_uart_feed("\r\n+IPD,3:abc\r\n+IPD,2:de");
    n = tp_read(buf, 32, 100); ZT_EQ_INT(n, 3); ZT_EQ_MEM(buf, "abc", 3);
    n = tp_read(buf, 32, 100); ZT_EQ_INT(n, 2); ZT_EQ_MEM(buf, "de", 2);
}
void test_esp_ipd_zero_length_skipped(void)
{
    char buf[32]; int n;
    mock_uart_reset(); prologue_ok(); tp_init();
    mock_uart_expect("AT+CIPSTART=\"TCP\",\"h\",80\r\n", "\r\nOK\r\n");
    tp_open("h", 80);
    mock_uart_feed("+IPD,0:+IPD,2:ok");
    n = tp_read(buf, 32, 100); ZT_EQ_INT(n, 2); ZT_EQ_MEM(buf, "ok", 2);
}
void test_esp_ipd_garbled_length_skipped(void)
{
    char buf[32]; int n;
    mock_uart_reset(); prologue_ok(); tp_init();
    mock_uart_expect("AT+CIPSTART=\"TCP\",\"h\",80\r\n", "\r\nOK\r\n");
    tp_open("h", 80);
    mock_uart_feed("+IPD,99999:+IPD,1:x");
    n = tp_read(buf, 32, 100); ZT_EQ_INT(n, 1); ZT_EQ_MEM(buf, "x", 1);
}
void test_esp_ipd_before_send_ok(void)
{
    char buf[32]; int n;
    mock_uart_reset(); prologue_ok(); tp_init();
    mock_uart_expect("AT+CIPSTART=\"TCP\",\"h\",80\r\n", "\r\nOK\r\n");
    tp_open("h", 80);
    mock_uart_expect("AT+CIPSEND=5\r\n", "\r\nOK\r\n> ");
    /* the ESP's recv callback beats its sent callback: +IPD arrives before SEND OK */
    mock_uart_expect("HELLO", "\r\nRecv 5 bytes\r\n\r\n+IPD,5:hello\r\nSEND OK\r\n");
    ZT_EQ_INT(tp_write("HELLO", 5), 0);
    n = tp_read(buf, 32, 1000); ZT_EQ_INT(n, 5); ZT_EQ_MEM(buf, "hello", 5);
}
void test_esp_read_overall_deadline(void)
{
    static char big[2100]; char buf[64]; int n = 0, total = 0, i;
    mock_uart_reset(); prologue_ok(); tp_init();
    mock_uart_expect("AT+CIPSTART=\"TCP\",\"h\",80\r\n", "\r\nOK\r\n");
    tp_open("h", 80);
    mock_uart_expect("AT+CIPSEND=1\r\n", "\r\nOK\r\n> ");
    mock_uart_expect("X", "\r\nRecv 1 bytes\r\n\r\nSEND OK\r\n");
    ZT_EQ_INT(tp_write("X", 1), 0);
    /* a frame far longer than the 1500-frame overall deadline: the mock clock ticks once
       per byte read, so the read must stop part-way instead of dribbling on forever */
    strcpy(big, "\r\n+IPD,2000:");
    for (i = 0; i < 2000; i++) big[12 + i] = 'a';
    big[12 + 2000] = 0;
    mock_uart_feed(big);
    while ((n = tp_read(buf, sizeof buf, 1000)) > 0) total += n;
    ZT_EQ_INT(n, ERR_TP_TIMEOUT);
    ZT_CHECK(total > 1300 && total < 1500);
}
void test_esp_open_already_connect(void)
{
    /* NonOS firmware answers "ALREADY CONNECT"; the SDK answers "ALREADY CONNECTED" */
    mock_uart_reset(); prologue_ok(); tp_init();
    mock_uart_expect("AT+CIPSTART=\"TCP\",\"h\",80\r\n", "ALREADY CONNECT\r\n");
    ZT_EQ_INT(tp_open("h", 80), 0);
    mock_uart_reset(); prologue_ok(); tp_init();
    mock_uart_expect("AT+CIPSTART=\"TCP\",\"h\",80\r\n", "ALREADY CONNECTED\r\n");
    ZT_EQ_INT(tp_open("h", 80), 0);
}
