#include <string.h>
#include <stdio.h>
#include "ztest.h"
#include "ha/ha_config.h"
#include "ha_errors.h"

static int parse(ha_config *c, const char *s) { return ha_config_parse(c, s, (unsigned)strlen(s)); }

void test_cfg_basic(void)
{
    ha_config c;
    ZT_EQ_INT(parse(&c, "# comment\r\nhost = 192.168.1.10\r\nport=8123\r\ntoken=abc.def\r\n"), HA_OK);
    ZT_EQ_STR(c.host, "192.168.1.10"); ZT_EQ_INT(c.port, 8123); ZT_EQ_STR(c.token, "abc.def");
    ZT_EQ_STR(c.template_, HA_DEFAULT_TEMPLATE);
}
void test_cfg_default_port_and_lf(void)
{
    ha_config c;
    ZT_EQ_INT(parse(&c, "host=h\ntoken=t\n"), HA_OK);
    ZT_EQ_INT(c.port, 8123);
}
void test_cfg_missing_token(void)
{
    ha_config c;
    ZT_EQ_INT(parse(&c, "host=h\n"), ERR_CFG_KEY);
}
void test_cfg_toolong(void)
{
    ha_config c; static char big[400]; memset(big, 'x', 300); big[300] = 0;
    char text[420]; strcpy(text, "host=h\ntoken="); strcat(text, big); strcat(text, "\n");
    ZT_EQ_INT(parse(&c, text), ERR_CFG_TOOLONG);
}
void test_cfg_template_override_and_unknown_key(void)
{
    ha_config c;
    ZT_EQ_INT(parse(&c, "host=h\ntoken=t\ncolour=blue\ntemplate={{ states('light.a') }}\n"), HA_OK);
    ZT_EQ_STR(c.template_, "{{ states('light.a') }}");
}
void test_cfg_load_missing_file(void)
{
    ha_config c;
    ZT_EQ_INT(ha_config_load(&c), ERR_CFG_MISSING);   /* run from test/ dir: no ha.cfg there */
}
void test_cfg_empty_required_value(void)
{
    ha_config c;
    ZT_EQ_INT(parse(&c, "host=\ntoken=t\n"), ERR_CFG_KEY);
    ZT_EQ_INT(parse(&c, "host=h\ntoken=   \n"), ERR_CFG_KEY);
}
void test_cfg_port_bounds(void)
{
    ha_config c;
    ZT_EQ_INT(parse(&c, "host=h\ntoken=t\nport=65535\n"), HA_OK);
    ZT_EQ_INT(c.port, 65535);
    ZT_EQ_INT(parse(&c, "host=h\ntoken=t\nport=65536\n"), ERR_CFG_TOOLONG);
    ZT_EQ_INT(parse(&c, "host=h\ntoken=t\nport=99999999999\n"), ERR_CFG_TOOLONG);
}
void test_cfg_load_file_and_truncation(void)
{
    ha_config c; FILE *f; int i;

    f = fopen("ha.cfg", "wb");
    fputs("host=10.0.0.5\nport=8000\ntoken=tok123\n", f);
    fclose(f);
    ZT_EQ_INT(ha_config_load(&c), HA_OK);
    ZT_EQ_STR(c.host, "10.0.0.5"); ZT_EQ_INT(c.port, 8000); ZT_EQ_STR(c.token, "tok123");
    remove("ha.cfg");

    /* a file too big for the load buffer is rejected, not parsed from a cut-off copy */
    f = fopen("ha.cfg", "wb");
    fputc('#', f);
    for (i = 0; i < 1100; i++) fputc('x', f);
    fputc('\n', f);
    fputs("host=h\ntoken=t\n", f);
    fclose(f);
    ZT_EQ_INT(ha_config_load(&c), ERR_CFG_TOOLONG);
    remove("ha.cfg");
}
