#include <string.h>
#include "ztest.h"
#include "mock_transport.h"
#include "http/http_client.h"
#include "ha_errors.h"

static char body[512]; static unsigned body_len;
static int cap_cb(const char *c, unsigned n, void *u) { (void)u; memcpy(body + body_len, c, n); body_len += n; return 0; }
static void script(const char *s) { mock_tp_script_response(s, (unsigned)strlen(s)); }
static void setup(http_req *r) { http_req z = { "GET", "h", 80, "/", 0, 0, 0, 0 }; *r = z; mock_tp_reset(); body_len = 0; }

void test_http_body_streams_exact_length(void)
{
    http_req r; setup(&r);
    script("HTTP/1.1 200 OK\r\nServer: x\r\ncontent-length: 11\r\n\r\nhello worldEXTRA");
    ZT_EQ_INT(http_request(&r, cap_cb, 0), 200);
    ZT_EQ_INT(body_len, 11); ZT_EQ_MEM(body, "hello world", 11);
}
void test_http_split_reads(void)
{
    http_req r; setup(&r); mock_tp_set_read_chunk(3);
    script("HTTP/1.1 404 Not Found\r\nContent-Length: 5\r\n\r\nabcde");
    ZT_EQ_INT(http_request(&r, cap_cb, 0), 404);
    ZT_EQ_INT(body_len, 5); ZT_EQ_MEM(body, "abcde", 5);
}
void test_http_truncated_body(void)
{
    http_req r; setup(&r);
    script("HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nabc");
    ZT_EQ_INT(http_request(&r, cap_cb, 0), ERR_HTTP_TRUNCATED);
    ZT_EQ_INT(mock_tp_close_calls, 1);
}
void test_http_chunked_rejected(void)
{
    http_req r; setup(&r);
    script("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n0\r\n\r\n");
    ZT_EQ_INT(http_request(&r, cap_cb, 0), ERR_HTTP_CHUNKED);
}
void test_http_bad_status_line(void)
{
    http_req r; setup(&r);
    script("garbage\r\n\r\n");
    ZT_EQ_INT(http_request(&r, cap_cb, 0), ERR_HTTP_STATUS);
}
void test_http_no_length_reads_to_close(void)
{
    http_req r; setup(&r);
    script("HTTP/1.1 200 OK\r\n\r\nuntil-close");
    ZT_EQ_INT(http_request(&r, cap_cb, 0), 200);
    ZT_EQ_INT(body_len, 11);
}
void test_http_keepalive_reuses_connection(void)
{
    http_req r; setup(&r); r.keep_alive = 1;
    script("HTTP/1.1 200 OK\r\nContent-Length: 1\r\n\r\nA");
    script("HTTP/1.1 200 OK\r\nContent-Length: 1\r\n\r\nB");
    ZT_EQ_INT(http_request(&r, cap_cb, 0), 200);
    ZT_EQ_INT(http_request(&r, cap_cb, 0), 200);
    ZT_EQ_INT(mock_tp_open_calls, 1); ZT_EQ_INT(mock_tp_close_calls, 0);
    ZT_EQ_MEM(body, "AB", 2);
    http_close(); ZT_EQ_INT(mock_tp_close_calls, 1);
}
void test_http_server_connection_close_drops_keepalive(void)
{
    http_req r; setup(&r); r.keep_alive = 1;
    script("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
    ZT_EQ_INT(http_request(&r, cap_cb, 0), 200);
    ZT_EQ_INT(mock_tp_close_calls, 1);
}
void test_http_absurd_length(void)
{
    http_req r; setup(&r);
    script("HTTP/1.1 200 OK\r\nContent-Length: 99999999999\r\n\r\n");
    ZT_EQ_INT(http_request(&r, cap_cb, 0), ERR_HTTP_BADLEN);
}

static int abort_after_5_cb(const char *c, unsigned n, void *u)
{
    (void)u; memcpy(body + body_len, c, n); body_len += n;
    return body_len >= 5 ? 1 : 0;
}
void test_http_callback_abort_truncates(void)
{
    http_req r; setup(&r);
    script("HTTP/1.1 200 OK\r\nContent-Length: 20\r\n\r\n01234567890123456789");
    ZT_EQ_INT(http_request(&r, abort_after_5_cb, 0), ERR_HTTP_TRUNCATED);
    ZT_EQ_INT(mock_tp_close_calls, 1);
}
