#include <stdio.h>
#include <string.h>
#include "../src/ha/ha_client.h"
#include "../src/transport/transport.h"
#include "../src/ha_errors.h"

static int print_line(const char *l, unsigned n, void *u) { (void)u; printf("%.*s\n", (int)n, l); return 0; }

int main(int argc, char **argv)
{
    static ha_config cfg; static ha_ctx ctx; static char st[64]; int rc;
    if (argc < 2) { fprintf(stderr, "usage: hacli on|off|toggle <entity> | state <entity> | call <domain> <service> <entity> | tmpl [text]\n"); return 2; }
    rc = ha_config_load(&cfg); if (rc) { fprintf(stderr, "config error %d\n", rc); return 1; }
    rc = tp_init(); if (rc) { fprintf(stderr, "transport init error %d\n", rc); return 1; }
    ha_init(&ctx, &cfg);
    if (!strcmp(argv[1], "on") && argc == 3)          rc = ha_call(&ctx, "homeassistant", "turn_on", argv[2]);
    else if (!strcmp(argv[1], "off") && argc == 3)    rc = ha_call(&ctx, "homeassistant", "turn_off", argv[2]);
    else if (!strcmp(argv[1], "toggle") && argc == 3) rc = ha_call(&ctx, "homeassistant", "toggle", argv[2]);
    else if (!strcmp(argv[1], "call") && argc == 5)   rc = ha_call(&ctx, argv[2], argv[3], argv[4]);
    else if (!strcmp(argv[1], "state") && argc == 3)  { rc = ha_state(&ctx, argv[2], st, sizeof st); if (!rc) printf("%s\n", st); }
    else if (!strcmp(argv[1], "tmpl"))                rc = ha_template(&ctx, argc > 2 ? argv[2] : cfg.template_, print_line, 0);
    else { fprintf(stderr, "bad arguments\n"); rc = ERR_ARG_USAGE; }
    ha_close(&ctx); tp_shutdown();
    if (rc) { fprintf(stderr, "error %d (http %u)\n", rc, ctx.last_status); return 1; }
    return 0;
}
