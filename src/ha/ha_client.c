#include <string.h>
#include "ha_client.h"
#include "../http/http_client.h"
#include "../ha_errors.h"

static char path[96];
static char body[128];
/* shared by every request; each call site must set every slot it uses and the terminating NULL */
static const char *hdrs[3];

int ha_init(ha_ctx *ctx, const ha_config *cfg)
{
    ctx->cfg = cfg; ctx->connected = 0; ctx->last_status = 0;
    strcpy(ctx->hdr_auth, "Authorization: Bearer ");
    strcat(ctx->hdr_auth, cfg->token);
    return HA_OK;
}

void ha_close(ha_ctx *ctx) { http_close(); ctx->connected = 0; }

static int drop_cb(const char *c, unsigned n, void *u) { (void)c; (void)n; (void)u; return 0; }

static int map_status(ha_ctx *ctx, int st)
{
    if (st < 0) { ctx->connected = 0; return st; }
    ctx->last_status = (unsigned)st;
    if (st == 401 || st == 403) return ERR_HA_AUTH;
    if (st >= 200 && st < 300) return HA_OK;
    return ERR_HA_STATUS;
}

/* Shared POST with JSON body; cb receives the response body. */
static int post_json(ha_ctx *ctx, const char *p, const char *json, http_body_cb cb, void *user)
{
    http_req r; int st;
    hdrs[0] = ctx->hdr_auth; hdrs[1] = "Content-Type: application/json"; hdrs[2] = 0;
    r.method = "POST"; r.host = ctx->cfg->host; r.port = ctx->cfg->port; r.path = p;
    r.headers = hdrs; r.body = json; r.body_len = (unsigned)strlen(json); r.keep_alive = 1;
    st = http_request(&r, cb, user);
    if (st >= 0) ctx->connected = 1;
    return map_status(ctx, st);
}

int ha_call_json(ha_ctx *ctx, const char *domain, const char *service, const char *json_body)
{
    if (strlen(domain) + strlen(service) + 16 >= sizeof path) return ERR_ARG_USAGE;
    strcpy(path, "/api/services/"); strcat(path, domain); strcat(path, "/"); strcat(path, service);
    return post_json(ctx, path, json_body, drop_cb, 0);
}

int ha_call(ha_ctx *ctx, const char *domain, const char *service, const char *entity)
{
    if (strlen(entity) + 16 >= sizeof body) return ERR_ARG_USAGE;
    strcpy(body, "{\"entity_id\":\""); strcat(body, entity); strcat(body, "\"}");
    return ha_call_json(ctx, domain, service, body);
}

unsigned ha_json_escape(char *dst, unsigned cap, const char *src)
{
    unsigned n = 0;
    for (; *src; src++) {
        const char *rep = 0; char c = *src;
        switch (c) { case '"': rep = "\\\""; break; case '\\': rep = "\\\\"; break;
                     case '\n': rep = "\\n"; break; case '\r': rep = "\\r"; break; case '\t': rep = "\\t"; break; }
        if (rep) { if (n + 2 >= cap) return 0; dst[n++] = rep[0]; dst[n++] = rep[1]; }
        else     { if (n + 1 >= cap) return 0; dst[n++] = c; }
    }
    dst[n] = 0; return n;
}

/* ---- template: line splitter over streamed chunks ---- */
static char tbody[HA_CFG_TEMPLATE_MAX * 2 + 16];
static char tline[128]; static unsigned tline_n;
static ha_line_cb t_cb; static void *t_user;
static unsigned char t_aborted;

static int tmpl_chunk_cb(const char *c, unsigned n, void *u)
{
    (void)u;
    while (n--) {
        char ch = *c++;
        if (ch == '\n') {
            unsigned k = tline_n; if (k && tline[k-1] == '\r') k--; tline[k] = 0;
            if (t_cb(tline, k, t_user)) { t_aborted = 1; tline_n = 0; return 1; }
            tline_n = 0;
        }
        else if (tline_n < sizeof tline - 1) tline[tline_n++] = ch;
    }
    return 0;
}

int ha_template(ha_ctx *ctx, const char *tmpl, ha_line_cb cb, void *user)
{
    int rc; unsigned n;
    strcpy(tbody, "{\"template\":\"");
    n = ha_json_escape(tbody + 13, sizeof tbody - 16, tmpl);
    if (!n) return ERR_CFG_TOOLONG;
    strcat(tbody, "\"}");
    t_cb = cb; t_user = user; tline_n = 0; t_aborted = 0;
    rc = post_json(ctx, "/api/template", tbody, tmpl_chunk_cb, 0);
    if (rc == ERR_HTTP_TRUNCATED && t_aborted) rc = HA_OK;
    if (rc == HA_OK && tline_n && !t_aborted) { tline[tline_n] = 0; cb(tline, tline_n, user); tline_n = 0; }
    if (rc == ERR_HA_STATUS && ctx->last_status == 400) return ERR_HA_TEMPLATE;
    return rc;
}

/* ---- state: rolling scanner for "state":" ---- */
static const char STATE_KEY[] = "\"state\":\"";
static unsigned s_match; static char *s_out; static unsigned s_cap, s_n; static unsigned char s_done;

static int state_chunk_cb(const char *c, unsigned n, void *u)
{
    (void)u;
    while (n--) {
        char ch = *c++;
        if (s_done) continue;
        if (s_match < 9) { s_match = (ch == STATE_KEY[s_match]) ? s_match + 1 : (ch == '"' ? 1 : 0); continue; }
        if (ch == '"') { s_done = 1; continue; }
        if (s_n + 1 < s_cap) s_out[s_n++] = ch;
    }
    return 0;
}

int ha_state(ha_ctx *ctx, const char *entity, char *buf, unsigned len)
{
    http_req r; int st;
    if (!len) return ERR_ARG_USAGE;                 /* no room even for the NUL */
    if (strlen(entity) + 12 >= sizeof path) return ERR_ARG_USAGE;
    strcpy(path, "/api/states/"); strcat(path, entity);
    hdrs[0] = ctx->hdr_auth; hdrs[1] = 0;
    r.method = "GET"; r.host = ctx->cfg->host; r.port = ctx->cfg->port; r.path = path;
    r.headers = hdrs; r.body = 0; r.body_len = 0; r.keep_alive = 1;
    s_match = 0; s_out = buf; s_cap = len; s_n = 0; s_done = 0; buf[0] = 0;
    st = http_request(&r, state_chunk_cb, 0);
    if (st >= 0) ctx->connected = 1;
    buf[s_n] = 0;
    return map_status(ctx, st);
}
