#include <string.h>
#include "ztest.h"
#include "mock_transport.h"
#include "http/http_client.h"
#include "ha_errors.h"

static int drop_cb(const char *c, unsigned n, void *u) { (void)c;(void)n;(void)u; return 0; }
static void script(const char *s) { mock_tp_script_response(s, (unsigned)strlen(s)); }

void test_http_get_bytes(void)
{
    static const char *hdrs[] = { "Authorization: Bearer tok", "Accept: text/plain", 0 };
    http_req r = { "GET", "ha.local", 8123, "/api/", hdrs, 0, 0, 0 };
    unsigned wl; const char *w; int st;
    mock_tp_reset();
    script("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
    st = http_request(&r, drop_cb, 0);
    ZT_EQ_INT(st, 200);
    w = mock_tp_written(&wl);
    ZT_EQ_STR(w, "GET /api/ HTTP/1.1\r\nHost: ha.local:8123\r\n"
                 "Authorization: Bearer tok\r\nAccept: text/plain\r\n"
                 "Connection: close\r\n\r\n");
    ZT_EQ_INT(mock_tp_open_calls, 1); ZT_EQ_INT(mock_tp_last_port, 8123);
    ZT_EQ_INT(mock_tp_close_calls, 1);
    ZT_CHECK(mock_tp_write_calls <= 2);       /* request line + headers batched, not one write per piece */
}

void test_http_post_body_and_port80(void)
{
    static const char *hdrs[] = { "Content-Type: application/json", 0 };
    http_req r = { "POST", "h", 80, "/x", hdrs, "{\"a\":1}", 7, 0 };
    unsigned wl; const char *w;
    mock_tp_reset();
    script("HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\r\n");
    ZT_EQ_INT(http_request(&r, drop_cb, 0), 201);
    w = mock_tp_written(&wl);
    ZT_EQ_STR(w, "POST /x HTTP/1.1\r\nHost: h\r\nContent-Type: application/json\r\n"
                 "Content-Length: 7\r\nConnection: close\r\n\r\n{\"a\":1}");
    ZT_CHECK(mock_tp_write_calls <= 2);       /* body appended to the header buffer, or one write of its own */
}

void test_http_open_failure_propagates(void)
{
    http_req r = { "GET", "h", 80, "/", 0, 0, 0, 0 };
    mock_tp_reset(); mock_tp_set_open_result(ERR_TP_CONNECT);
    ZT_EQ_INT(http_request(&r, drop_cb, 0), ERR_TP_CONNECT);
    ZT_EQ_INT(mock_tp_close_calls, 0);
}

void test_http_host_too_long(void)
{
    static const char long_host[65] =
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    http_req r = { "GET", long_host, 80, "/", 0, 0, 0, 0 };
    mock_tp_reset();
    ZT_EQ_INT(http_request(&r, drop_cb, 0), ERR_TP_CONNECT);
    ZT_EQ_INT(mock_tp_open_calls, 0);
}
