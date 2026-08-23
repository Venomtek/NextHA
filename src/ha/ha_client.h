#ifndef NEXTHA_HA_CLIENT_H
#define NEXTHA_HA_CLIENT_H

#include "ha_config.h"

typedef struct { const ha_config *cfg; unsigned char connected; unsigned last_status;
                 char hdr_auth[HA_CFG_TOKEN_MAX + 24]; } ha_ctx;   /* "Authorization: Bearer " + token */
typedef int (*ha_line_cb)(const char *line, unsigned len, void *user);
/* return nonzero to stop streaming; ha_template then returns HA_OK and closes the connection */

int  ha_init(ha_ctx *ctx, const ha_config *cfg);                              /* HA_OK */
int  ha_call(ha_ctx *ctx, const char *domain, const char *service, const char *entity);
int  ha_call_json(ha_ctx *ctx, const char *domain, const char *service, const char *json_body);
int  ha_state(ha_ctx *ctx, const char *entity, char *buf, unsigned len);
int  ha_template(ha_ctx *ctx, const char *tmpl, ha_line_cb cb, void *user);
void ha_close(ha_ctx *ctx);

#endif
