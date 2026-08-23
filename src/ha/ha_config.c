#include <string.h>
#include "ha_config.h"
#include "cfg_io.h"
#include "../ha_errors.h"

const char HA_DEFAULT_TEMPLATE[] =
  "{% for s in states if s.domain in ['light','switch','fan','cover','lock','input_boolean'] %}"
  "{{ s.entity_id }}|{{ s.state }}|{{ s.name }}\n{% endfor %}";

static char filebuf[1024];   /* static: dot stack is small */

static int copy_val(char *dst, unsigned cap, const char *v, unsigned n)
{
    while (n && (v[n-1] == ' ' || v[n-1] == '\t')) n--;   /* rtrim */
    if (n >= cap) return ERR_CFG_TOOLONG;
    memcpy(dst, v, n); dst[n] = 0; return HA_OK;
}

int ha_config_parse(ha_config *cfg, const char *text, unsigned len)
{
    unsigned i = 0; unsigned char have_host = 0, have_token = 0; int rc;
    cfg->host[0] = 0; cfg->token[0] = 0; cfg->port = 8123;
    strcpy(cfg->template_, HA_DEFAULT_TEMPLATE);
    while (i < len) {
        const char *ls = text + i, *eq = 0, *p; unsigned ll = 0, klen;
        while (i < len && text[i] != '\n') { if (text[i] == '=' && !eq) eq = text + i; i++; ll++; }
        if (i < len) i++;                                  /* skip LF */
        if (ll && ls[ll-1] == '\r') ll--;
        p = ls; while (p < ls + ll && (*p == ' ' || *p == '\t')) p++;
        if (p >= ls + ll || *p == '#' || !eq) continue;
        klen = (unsigned)(eq - p); while (klen && (p[klen-1] == ' ' || p[klen-1] == '\t')) klen--;
        { const char *v = eq + 1; unsigned vn = (unsigned)(ls + ll - v);
          while (vn && (*v == ' ' || *v == '\t')) { v++; vn--; }
          if (klen == 4 && !strncmp(p, "host", 4)) { if ((rc = copy_val(cfg->host, HA_CFG_HOST_MAX, v, vn))) return rc; have_host = cfg->host[0] != 0; }
          else if (klen == 5 && !strncmp(p, "token", 5)) { if ((rc = copy_val(cfg->token, HA_CFG_TOKEN_MAX, v, vn))) return rc; have_token = cfg->token[0] != 0; }
          else if (klen == 8 && !strncmp(p, "template", 8)) { if ((rc = copy_val(cfg->template_, HA_CFG_TEMPLATE_MAX, v, vn))) return rc; }
          else if (klen == 4 && !strncmp(p, "port", 4)) {
              /* reject before the multiply-add can overflow: on the zxn target "unsigned" is
                 16 bits, so checking port > 65535 after the fact would be dead code that a
                 wrapped value could sail past silently; this bound never lets port*10+digit
                 exceed 65535 in the first place, so it is correct on both 16- and 32-bit unsigned */
              unsigned port = 0;
              while (vn && *v >= '0' && *v <= '9') {
                  unsigned digit = (unsigned)(*v - '0');
                  if (port > 6553u || (port == 6553u && digit > 5u)) return ERR_CFG_TOOLONG;
                  port = port*10 + digit; v++; vn--;
              }
              if (port) cfg->port = port;
          }
        }
    }
    return (have_host && have_token) ? HA_OK : ERR_CFG_KEY;
}

int ha_config_load(ha_config *cfg)
{
    int n = cfg_io_read_file("ha.cfg", filebuf, sizeof filebuf - 1);
    if (n < 0) n = cfg_io_read_file("c:/sys/ha.cfg", filebuf, sizeof filebuf - 1);
    if (n < 0) return ERR_CFG_MISSING;
    /* a file that fills the buffer was almost certainly cut short: refuse rather than
       parse half a line (and half a token) as if it were the whole file */
    if (n >= (int)(sizeof filebuf - 1)) return ERR_CFG_TOOLONG;
    return ha_config_parse(cfg, filebuf, (unsigned)n);
}
