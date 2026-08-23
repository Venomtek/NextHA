#include <string.h>
#include "ztest.h"
#include "mock_transport.h"
#include "ha/ha_client.h"
#include "ha_errors.h"
#include "http/http_client.h"

unsigned ha_json_escape(char *dst, unsigned cap, const char *src);

static ha_config cfg; static ha_ctx ctx;
static char lines[16][128]; static unsigned nlines;
static int line_cb(const char *l, unsigned n, void *u) { (void)u; memcpy(lines[nlines], l, n); lines[nlines][n] = 0; nlines++; return 0; }
static int abort_after_first_cb(const char *l, unsigned n, void *u) { (void)u; memcpy(lines[nlines], l, n); lines[nlines][n] = 0; nlines++; return 1; }
static void setup(void) {
    http_close(); mock_tp_reset();
    strcpy(cfg.host, "h"); cfg.port = 8123; strcpy(cfg.token, "T"); strcpy(cfg.template_, "x");
    ha_init(&ctx, &cfg); nlines = 0;
}
static void script(const char *s) { mock_tp_script_response(s, (unsigned)strlen(s)); }

void test_json_escape(void)
{
    char out[64];
    ZT_EQ_INT(ha_json_escape(out, sizeof out, "a\"b\\c\nd\re\tf"), 16);
    ZT_EQ_STR(out, "a\\\"b\\\\c\\nd\\re\\tf");
    ZT_EQ_INT(ha_json_escape(out, 4, "abcdef"), 0);       /* overflow -> 0 */
}
void test_template_request_and_lines(void)
{
    unsigned wl; const char *w; setup();
    script("HTTP/1.1 200 OK\r\nContent-Length: 38\r\n\r\nlight.a|on|Lamp A\nswitch.b|off|Plug B\n");
    ZT_EQ_INT(ha_template(&ctx, "{{ \"q\" }}", line_cb, 0), HA_OK);
    ZT_EQ_INT(nlines, 2);
    ZT_EQ_STR(lines[0], "light.a|on|Lamp A"); ZT_EQ_STR(lines[1], "switch.b|off|Plug B");
    w = mock_tp_written(&wl);
    ZT_CHECK(strstr(w, "POST /api/template HTTP/1.1") != 0);
    ZT_CHECK(strstr(w, "\r\n\r\n{\"template\":\"{{ \\\"q\\\" }}\"}") != 0);
}
void test_template_lines_split_across_chunks(void)
{
    setup(); mock_tp_set_read_chunk(5);
    script("HTTP/1.1 200 OK\r\nContent-Length: 17\r\n\r\nab|on|X\r\ncd|off|Y");   /* CRLF + unterminated last line */
    ZT_EQ_INT(ha_template(&ctx, "t", line_cb, 0), HA_OK);
    ZT_EQ_INT(nlines, 2); ZT_EQ_STR(lines[0], "ab|on|X"); ZT_EQ_STR(lines[1], "cd|off|Y");
}
void test_template_400_maps(void)
{
    setup(); script("HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n");
    ZT_EQ_INT(ha_template(&ctx, "{{ broken", line_cb, 0), ERR_HA_TEMPLATE);
}
void test_template_callback_abort(void)
{
    setup();
    script("HTTP/1.1 200 OK\r\nContent-Length: 14\r\n\r\none\ntwo\nthree\n");
    ZT_EQ_INT(ha_template(&ctx, "t", abort_after_first_cb, 0), HA_OK);
    ZT_EQ_INT(nlines, 1);
    ZT_EQ_STR(lines[0], "one");
    ZT_EQ_INT(mock_tp_close_calls, 1);
}
void test_state_extracts_value(void)
{
    char st[16]; setup();
    script("HTTP/1.1 200 OK\r\nContent-Length: 63\r\n\r\n{\"entity_id\":\"light.a\",\"state\":\"on\",\"attributes\":{\"x\":\"state\"}}");
    ZT_EQ_INT(ha_state(&ctx, "light.a", st, sizeof st), HA_OK);
    ZT_EQ_STR(st, "on");
    setup();                                     /* a zero-length buffer has nowhere to put the NUL */
    ZT_EQ_INT(ha_state(&ctx, "light.a", st, 0), ERR_ARG_USAGE);
    ZT_EQ_INT(mock_tp_open_calls, 0);
}
void test_state_split_chunks_and_404(void)
{
    char st[16]; setup(); mock_tp_set_read_chunk(4);
    script("HTTP/1.1 200 OK\r\nContent-Length: 35\r\n\r\n{\"state\":\"unavailable\",\"foo\":\"bar\"}");
    ZT_EQ_INT(ha_state(&ctx, "x.y", st, sizeof st), HA_OK);
    ZT_EQ_STR(st, "unavailable");
    setup(); script("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n");
    ZT_EQ_INT(ha_state(&ctx, "x.y", st, sizeof st), ERR_HA_STATUS);
    ZT_EQ_INT(ctx.last_status, 404);
}
