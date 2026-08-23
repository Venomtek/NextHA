#include <stdio.h>
#include <stdlib.h>
#include "ha/ha_client.h"
#include "transport/transport.h"

/* Minimal example: links ha.lib from dist/ and toggles one entity.
   Build with examples/c/build.cmd; run as ".toggle light.lounge" from NextBASIC. */
int main(int argc, char **argv)
{
    static ha_config cfg; static ha_ctx ctx; int rc;
    if (argc != 2) { printf("usage: .toggle <entity>\n"); return 0; }
    if ((rc = ha_config_load(&cfg)) || (rc = tp_init())) { printf("error %d\n", rc); tp_shutdown(); return 0; }
    ha_init(&ctx, &cfg);
    rc = ha_call(&ctx, "homeassistant", "toggle", argv[1]);
    printf(rc ? "error %d (http %u)\n" : "ok\n", rc, ctx.last_status);
    ha_close(&ctx); tp_shutdown();
    return 0;
}
