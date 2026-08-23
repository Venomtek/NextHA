#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../ha/ha_client.h"
#include "../transport/transport.h"
#include "../ha_errors.h"
#include "errors.h"
#include "bank.h"

static ha_config cfg;
static ha_ctx ctx;
static char statebuf[64];
static unsigned bank_pos;             /* payload bytes written so far */
static unsigned char bank_full;       /* set by line_to_bank when the bank fills mid-stream */
static unsigned char esp_started;

static void cleanup(void)
{
    if (esp_started) { ha_close(&ctx); tp_shutdown(); }
    bank_unmap();
}

static void fail(int rc)
{
    unsigned char *m;
    bank_unmap();                      /* never print while MMU6/7 are hijacked */
    switch (rc) {
    case ERR_TP_ESP_NONE:   m = err_esp_none; break;
    case ERR_TP_ESP_INIT:   m = err_esp_init; break;
    case ERR_TP_CONNECT:    m = err_connect; break;
    case ERR_TP_TIMEOUT:    m = err_timeout; break;
    case ERR_TP_SEND:       m = err_send; break;
    case ERR_TP_BREAK:      m = err_break; break;
    case ERR_HTTP_STATUS: case ERR_HTTP_TRUNCATED: case ERR_HTTP_CHUNKED: case ERR_HTTP_BADLEN:
                            m = err_http; break;
    case ERR_HA_AUTH:       m = err_auth; break;
    case ERR_HA_STATUS:     m = err_status; break;
    case ERR_HA_TEMPLATE:   m = err_template; break;
    case ERR_CFG_MISSING:   m = err_cfg_missing; break;
    case ERR_CFG_KEY:       m = err_cfg_key; break;
    case ERR_CFG_TOOLONG:   m = err_cfg_toolong; break;
    case ERR_ARG_BANK:      m = err_bank; break;
    case ERR_ARG_LEN:       m = err_len; break;
    default:                m = err_usage; break;
    }
    if (rc == ERR_HA_STATUS) printf("HTTP %u\n", ctx.last_status);
    exit((int)m);
}

/* parse "-b n" and "-l n" anywhere after the verb; returns count of positional args left in pos[] */
static int opt_bank = -1; static long opt_len = -1;
static int parse_opts(int argc, char **argv, char **pos, int maxpos)
{
    int i, np = 0;
    for (i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "-b") || !strcmp(argv[i], "-l")) {
            char *e; long v;
            if (i + 1 >= argc) fail(ERR_ARG_USAGE);
            v = strtol(argv[i + 1], &e, 10);
            if (*e) fail(argv[i][1] == 'b' ? ERR_ARG_BANK : ERR_ARG_LEN);
            if (argv[i][1] == 'b') {
                if (v < 0 || v > 111) fail(ERR_ARG_BANK);
                opt_bank = (int)v;
            } else {
                if (v < 0) fail(ERR_ARG_LEN);
                opt_len = v;
            }
            i++;
        } else if (np < maxpos) pos[np++] = argv[i];
        else fail(ERR_ARG_USAGE);
    }
    return np;
}

static void start_esp(void)
{
    int rc = tp_init();
    esp_started = 1;
    if (rc) fail(rc);
    ha_init(&ctx, &cfg);
}

/* bank writer used by state -b and tmpl */
static void bank_begin(void) { int rc = bank_map((unsigned char)opt_bank); if (rc) fail(rc); bank_pos = 0; bank_full = 0; }
static void bank_put_line(const char *s, unsigned n)
{
    if (bank_pos + 1 >= BANK_CAP) return;
    if (bank_pos + n + 1 > BANK_CAP) n = BANK_CAP - bank_pos - 1;
    memcpy(BANK_BASE + 2 + bank_pos, s, n); bank_pos += n;
    BANK_BASE[2 + bank_pos++] = '\r';
}
static void bank_end(void) { BANK_BASE[0] = (unsigned char)(bank_pos & 0xff); BANK_BASE[1] = (unsigned char)(bank_pos >> 8); }
static int line_to_bank(const char *l, unsigned n, void *u) { (void)u; bank_put_line(l, n); bank_full = (unsigned char)(bank_pos + 1 >= BANK_CAP); return bank_full; }

int main(int argc, char **argv)
{
    char *pos[3]; int np, rc;
    if (argc < 2) { printf("NextHA .ha v0.1\n.ha on|off|toggle <entity>\n.ha state <entity> [-b n]\n.ha call <dom> <svc> <entity>\n.ha tmpl -b n [-l len]\n"); return 0; }
    atexit(cleanup);
    np = parse_opts(argc, argv, pos, 3);
    rc = ha_config_load(&cfg); if (rc) fail(rc);

    if ((!strcmp(argv[1], "on") || !strcmp(argv[1], "off") || !strcmp(argv[1], "toggle")) && np == 1) {
        const char *svc = argv[1][1] == 'n' ? "turn_on" : (argv[1][1] == 'f' ? "turn_off" : "toggle");
        start_esp();
        rc = ha_call(&ctx, "homeassistant", svc, pos[0]); if (rc) fail(rc);
    } else if (!strcmp(argv[1], "call") && np == 3) {
        start_esp();
        rc = ha_call(&ctx, pos[0], pos[1], pos[2]); if (rc) fail(rc);
    } else if (!strcmp(argv[1], "state") && np == 1) {
        start_esp();
        rc = ha_state(&ctx, pos[0], statebuf, sizeof statebuf); if (rc) fail(rc);
        printf("%s\n", statebuf);
        if (opt_bank >= 0) { bank_begin(); bank_put_line(statebuf, (unsigned)strlen(statebuf)); bank_end(); bank_unmap(); }
    } else if (!strcmp(argv[1], "tmpl") && np == 0 && opt_bank >= 0) {
        unsigned n;
        bank_begin();                                        /* read template from the bank first */
        n = (opt_len >= 0) ? (unsigned)opt_len : (unsigned)strlen((char *)BANK_BASE);
        if (n >= sizeof cfg.template_) fail(ERR_CFG_TOOLONG); /* never render a silently cut template */
        /* A bank template overrides the one from ha.cfg, so it is staged in cfg.template_
           itself rather than in a second 512-byte buffer the dot cannot afford. */
        if (n) { memcpy(cfg.template_, BANK_BASE, n); cfg.template_[n] = 0; }
        start_esp();
        bank_pos = 0;
        rc = ha_template(&ctx, cfg.template_, line_to_bank, 0); if (rc) fail(rc);
        bank_end();
        bank_unmap();
        if (bank_full) printf("%u bytes (bank full)\n", bank_pos); else printf("%u bytes\n", bank_pos);
    } else fail(ERR_ARG_USAGE);
    return 0;
}
