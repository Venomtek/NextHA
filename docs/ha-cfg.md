# `ha.cfg` reference

`ha.cfg` tells `.HA` (and any program linking `ha.lib`) which Home
Assistant (HA) instance to talk to and how to authenticate. It is a
plain-text file of `key = value` lines.

A commented starting point is `ha.cfg.example` in the repository root
(copied into `dist\` by `build.cmd`): copy it to `/sys/ha.cfg` and edit
`host`, `port` and `token`.

## File format

- One `key = value` pair per line. Whitespace around the key and
  around the value is trimmed; `key=value` (no spaces) works too.
- Lines starting with `#` (after leading whitespace) are comments.
- Blank lines are ignored.
- Both `\n` and `\r\n` line endings are accepted.
- An unrecognised key is silently ignored (so extra keys do not break
  parsing).
- Any recognised key may be repeated; the last occurrence wins.

## Keys

| Key | Required | Limit | Default | Notes |
|---|---|---|---|---|
| `host` | yes | up to 63 characters | none | Hostname or IP address of the HA instance, e.g. `192.168.1.10` or `homeassistant.local`. Must be non-empty (a present but empty or whitespace-only value is treated as missing). |
| `token` | yes | up to 255 characters | none | A Home Assistant long-lived access token (see below). Must be non-empty. |
| `port` | no | 1-65535 | `8123` | HA's HTTP port. A non-numeric value, or a literal `0`, is ignored and the current default/previous value is kept. A value greater than 65535 is rejected (see below). |
| `template` | no | up to 511 characters (same limit for a bank template) | see "Default template" below | The Jinja2 template rendered by `.ha tmpl` when the bank passed to `-b` is empty (see `docs/cli.md`). |

If `host` or `token` is missing (or empty) after parsing the whole
file, `ha_config_load`/`ha_config_parse` returns `ERR_CFG_KEY`. If any
value is too long for its field - including a `port` value whose
digits would exceed 65535 - parsing stops immediately and returns
`ERR_CFG_TOOLONG`.

## Search order

`ha_config_load()` (used by `.HA` and by `examples/c/toggle.c`) looks
for the config file in this order and uses the first one found:

1. `ha.cfg` (resolved relative to wherever the dot command / program is
   run from).
2. `c:/sys/ha.cfg` (the conventional NextZXOS system config location).

If neither file can be opened, it returns `ERR_CFG_MISSING`. Programs
that already have the file's text in memory (or want to unit-test
config handling) can call `ha_config_parse(cfg, text, len)` directly
instead.

## Example `ha.cfg`

```
# NextHA config
host = 192.168.1.10
port = 8123
token = eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...
```

## Creating a long-lived access token

1. In the Home Assistant web UI, click your profile (bottom left).
2. Open the **Security** tab.
3. Under **Long-lived access tokens**, click **Create token**.
4. Name it (e.g. `nextha`) and copy the token shown - HA only displays
   it once.
5. Paste it as the `token` value in `ha.cfg`.

Treat the token like a password: anyone with it has full API access to
your HA instance for as long as the token is not revoked.

## Default template

If `template` is not set in `ha.cfg` (or `.ha tmpl -b n` is run against
an empty bank with no `template` override), the built-in default is
used:

```
{% for s in states if s.domain in ['light','switch','fan','cover','lock','input_boolean'] %}{{ s.entity_id }}|{{ s.state }}|{{ s.name }}
{% endfor %}
```

(the line break between `{{ s.name }}` and `{% endfor %}` above is a
real newline character in the template's source, not two literal
characters `\` and `n`.)

For every entity in the `light`, `switch`, `fan`, `cover`, `lock` and
`input_boolean` domains, this renders one `entity_id|state|name`
record followed by a newline, so HA's response is one line per entity
- which is what `.ha tmpl -b n` needs, since it splits the response on
real newline characters and writes one bank-format line per entity
(see `docs/cli.md`).

A `template =` line in `ha.cfg` is itself a single line of text, so it
cannot contain a raw newline character the way the built-in default
does. If you write your own multi-line-output template in `ha.cfg`,
put the newline inside a Jinja expression instead, where Jinja does
interpret the `\n` escape, for example:

```
template = {% for s in states.light %}{{ s.entity_id }}|{{ s.state }}{{ "\n" }}{% endfor %}
```

Alternatively, poke a template containing a real newline byte directly
into a bank and run `.ha tmpl -b n` against it (see `docs/cli.md`) -
that path is not limited to one line of `ha.cfg` text. The 511-character
limit still applies: a bank template of 512 characters or more is
rejected with `ERR_CFG_TOOLONG` (`L ha.cfg value too long`) rather than
rendered cut short.

`.ha tmpl -b n` streams the rendered response line by line into bank
`n`; see `docs/cli.md` for the bank format. The 511-character limit
applies to every template `.ha` renders - the one configured here, and
one poked into a bank.

The whole `ha.cfg` file must also be smaller than 1024 bytes: the
loader reads at most 1023 bytes, and a file at or over that size is
rejected with `ERR_CFG_TOOLONG` instead of being parsed from a
truncated copy.
