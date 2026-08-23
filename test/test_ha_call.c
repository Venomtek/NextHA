#include <string.h>
#include "ztest.h"
#include "mock_transport.h"
#include "ha/ha_client.h"
#include "ha_errors.h"
#include "http/http_client.h"

static ha_config cfg; static ha_ctx ctx;
static void setup(void) {
    http_close(); mock_tp_reset();
    strcpy(cfg.host, "ha.local"); cfg.port = 8123; strcpy(cfg.token, "T0K"); cfg.template_[0] = 0;
    ha_init(&ctx, &cfg);
}
static void script(const char *s) { mock_tp_script_response(s, (unsigned)strlen(s)); }

void test_ha_call_turn_on_bytes(void)
{
    unsigned wl; const char *w; setup();
    script("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n[]");
    ZT_EQ_INT(ha_call(&ctx, "homeassistant", "turn_on", "light.lounge"), HA_OK);
    ZT_EQ_INT(ctx.last_status, 200);
    w = mock_tp_written(&wl);
    ZT_EQ_STR(w, "POST /api/services/homeassistant/turn_on HTTP/1.1\r\nHost: ha.local:8123\r\n"
                 "Authorization: Bearer T0K\r\nContent-Type: application/json\r\n"
                 "Content-Length: 28\r\nConnection: keep-alive\r\n\r\n{\"entity_id\":\"light.lounge\"}");
    ha_close(&ctx); ZT_EQ_INT(mock_tp_close_calls, 1);
}
void test_ha_call_401_maps_to_auth(void)
{
    setup(); script("HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\n\r\n");
    ZT_EQ_INT(ha_call(&ctx, "homeassistant", "toggle", "switch.x"), ERR_HA_AUTH);
    ZT_EQ_INT(ctx.last_status, 401);
}
void test_ha_call_500_maps_to_status(void)
{
    setup(); script("HTTP/1.1 500 Oops\r\nContent-Length: 0\r\n\r\n");
    ZT_EQ_INT(ha_call(&ctx, "light", "turn_off", "light.x"), ERR_HA_STATUS);
    ZT_EQ_INT(ctx.last_status, 500);
}
void test_ha_call_json_passthrough(void)
{
    unsigned wl; const char *w; setup();
    script("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
    ZT_EQ_INT(ha_call_json(&ctx, "light", "turn_on", "{\"entity_id\":\"light.a\",\"brightness\":128}"), HA_OK);
    w = mock_tp_written(&wl);
    ZT_CHECK(strstr(w, "POST /api/services/light/turn_on ") != 0);
    ZT_CHECK(strstr(w, "\r\n\r\n{\"entity_id\":\"light.a\",\"brightness\":128}") != 0);
}
void test_ha_call_transport_error_passthrough(void)
{
    setup(); mock_tp_set_open_result(ERR_TP_CONNECT);
    ZT_EQ_INT(ha_call(&ctx, "homeassistant", "turn_on", "light.x"), ERR_TP_CONNECT);
}
