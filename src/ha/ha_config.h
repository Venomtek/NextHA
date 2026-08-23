#ifndef NEXTHA_HA_CONFIG_H
#define NEXTHA_HA_CONFIG_H

#define HA_CFG_HOST_MAX 64
#define HA_CFG_TOKEN_MAX 256
#define HA_CFG_TEMPLATE_MAX 512

typedef struct {
    char host[HA_CFG_HOST_MAX];
    unsigned port;
    char token[HA_CFG_TOKEN_MAX];
    char template_[HA_CFG_TEMPLATE_MAX];
} ha_config;

int ha_config_parse(ha_config *cfg, const char *text, unsigned len);  /* HA_OK / ERR_CFG_KEY / ERR_CFG_TOOLONG */
int ha_config_load(ha_config *cfg);  /* tries "ha.cfg" then "c:/sys/ha.cfg"; ERR_CFG_MISSING if neither */

extern const char HA_DEFAULT_TEMPLATE[];

#endif
