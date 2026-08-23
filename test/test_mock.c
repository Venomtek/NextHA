#include "ztest.h"
#include "mock_transport.h"
#include "transport/transport.h"

void test_mock_transport(void)
{
    char buf[8]; unsigned wl; const char *w; int n;
    mock_tp_reset();
    mock_tp_script_response("HELLO", 5);
    mock_tp_set_read_chunk(2);
    ZT_EQ_INT(tp_open("h", 80), 0);
    ZT_EQ_INT(mock_tp_open_calls, 1);
    ZT_EQ_STR(mock_tp_last_host, "h");
    ZT_EQ_INT(tp_write("ab", 2), 0);
    w = mock_tp_written(&wl);
    ZT_EQ_INT(wl, 2); ZT_EQ_MEM(w, "ab", 2);
    n = tp_read(buf, 8, 100); ZT_EQ_INT(n, 2); ZT_EQ_MEM(buf, "HE", 2);
    n = tp_read(buf, 8, 100); ZT_EQ_INT(n, 2);
    n = tp_read(buf, 8, 100); ZT_EQ_INT(n, 1); ZT_EQ_MEM(buf, "O", 1);
    n = tp_read(buf, 8, 100); ZT_EQ_INT(n, 0);       /* stream exhausted = peer closed */
    ZT_EQ_INT(tp_close(), 0); ZT_EQ_INT(mock_tp_close_calls, 1);
}

void test_mock_large_fixture(void)
{
    char buf[128]; int n;
    unsigned i, payload[100];
    for (i = 0; i < 100; i++) payload[i] = i;
    mock_tp_reset();
    mock_tp_script_response((const char *)payload, 100);
    mock_tp_set_read_chunk(64);
    n = tp_read(buf, 128, 100); ZT_EQ_INT(n, 64);
    n = tp_read(buf, 128, 100); ZT_EQ_INT(n, 36);
}
