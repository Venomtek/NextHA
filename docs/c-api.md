# NextHA C API (`ha.lib`)

`ha.lib` is the engine behind `.HA`: a transport layer, an HTTP client,
and a Home Assistant (HA) client, built as a single z88dk library. Any
z88dk C program targeting the ZX Spectrum Next can link it directly
instead of shelling out to `.HA`.

Rules that apply to every function below:

- **No heap.** The library never calls `malloc`. Every buffer (`ha_ctx`,
  `ha_config`, output buffers) is owned and allocated by the caller -
  typically as `static` storage, since the Next's C stack is small.
- **No whole-response buffering.** HTTP response bodies are delivered
  through a callback as bytes arrive over the wire; nothing the size of
  a full HA response is ever held in memory at once.
- **The library never prints.** Every function returns `HA_OK` (0) or a
  negative `ERR_*` code from `ha_errors.h`; turning that into on-screen
  text is the caller's job (see `docs/errors.md` for a suggested
  mapping, and `src/dot/main.c` for a worked example).

## Headers

```c
#include "ha_errors.h"          /* error codes, shared by every layer */
#include "transport/transport.h"
#include "http/http_client.h"
#include "ha/ha_client.h"       /* includes ha_config.h */
```

After `build.cmd` runs, these are copied into `dist/include/...` (see
"Building against `ha.lib`" below) alongside `dist/ha.lib`.

## Transport (`transport.h`)

```c
int  tp_init(void);
int  tp_open(const char *host, unsigned port);
int  tp_write(const void *buf, unsigned len);                 /* 0 ok / negative */
int  tp_read(void *buf, unsigned len, unsigned timeout_ms);   /* >0 bytes, 0 = peer closed, <0 error */
int  tp_close(void);
void tp_shutdown(void);                                       /* release anything tp_init acquired (e.g. restore CPU speed); no-op on host */
```

On the Next build, `tp_init()` puts the machine into 28 MHz turbo mode
for the duration of the session (saving the previous `REG_TURBO_MODE`
value) and brings the ESP-01 up: it sets the UART baud rate for the
current video timing, then runs the `ATE0` / `AT+CIPCLOSE` /
`AT+CIPMUX=0` / `AT+CIPDINFO=0` prologue. (`AT+CIPDINFO=0` is sent
because the setting persists in the ESP's flash: left on by some other
program, it would prefix every `+IPD` frame with the peer address and
break framing. Its reply is ignored, so firmware that does not know the
command is fine.)

If the prologue fails for any reason - no reply to `ATE0`
(`ERR_TP_ESP_NONE`) or a failed `AT+CIPMUX=0` (`ERR_TP_ESP_INIT`) -
`tp_init()` makes one `AT+RST` recovery attempt: it waits for
`ready`/`WIFI GOT IP` and then runs the prologue once more. If the
reset produces no reply at all, or the second prologue fails the same
way, the original error code is returned.

Call `tp_shutdown()` once, after you are done with the transport
(typically right before your program exits), to restore the CPU speed
that was in effect before `tp_init()`. It is safe to call even if
`tp_init()` never ran or failed. `tp_shutdown()` does not close an open
TCP connection - use `tp_close()` (directly, or via `ha_close()`) for
that first.

`tp_read`'s `timeout_ms` bounds the wait for the *next single byte*.
On top of that, the Next transport keeps an overall 30-second deadline
that restarts every time a `tp_write` completes: once it passes, every
further `tp_read` in that exchange returns `ERR_TP_TIMEOUT`, so a peer
that dribbles bytes indefinitely cannot hang the machine. Each new
request resets the deadline, so a long sequence of HA calls is not
affected - only one stalled request/response is.

Holding **CAPS SHIFT + SPACE** (the Spectrum's BREAK) while the
transport is waiting for the ESP aborts the operation and returns
`ERR_TP_BREAK` (`.HA` shows `D BREAK into program`). This is checked in
the UART receive wait, so it works during any command, send or read -
it does not need an interrupt handler and it never fires on the host
build.

`tp_open`, `tp_write`, `tp_read` and `tp_close` are used internally by
the HTTP client; most C authors only call `tp_init()` and
`tp_shutdown()` directly.

## HTTP client (`http_client.h`)

```c
typedef int (*http_body_cb)(const char *chunk, unsigned len, void *user);  /* return 0 to continue; nonzero aborts -> ERR_HTTP_TRUNCATED */

typedef struct {
    const char *method; const char *host; unsigned port; const char *path;
    const char *const *headers;   /* NULL-terminated array of "Name: value" */
    const char *body; unsigned body_len;
    unsigned char keep_alive;
} http_req;

int  http_request(const http_req *req, http_body_cb on_chunk, void *user);  /* status or ERR_* */
void http_close(void);
```

`http_request` returns the HTTP status code (100-599) on a well-formed
response, or a negative `ERR_HTTP_*` / `ERR_TP_*` code. Chunked
transfer-encoding responses are detected and rejected with
`ERR_HTTP_CHUNKED` (not supported in v1). When `req->keep_alive` is set
and the server did not send `Connection: close`, the TCP connection is
left open and reused by the next `http_request` to the same host and
port; otherwise it is closed automatically. Call `http_close()`
directly to force the connection closed (the `ha_client` layer does
this in `ha_close()`).

Most C authors will not call `http_request` directly - the `ha_client`
layer below builds `http_req` values for you.

## HA client (`ha_client.h`)

```c
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
```

- `ha_init` wires `ctx` to a loaded `ha_config` and builds the
  `Authorization: Bearer <token>` header once, into `ctx->hdr_auth`.
  Always returns `HA_OK`.
- `ha_call` calls `<domain>.<service>` with a JSON body of
  `{"entity_id":"<entity>"}`. `ha_call_json` is the same call with a
  caller-supplied JSON body, for services that need more than
  `entity_id`.
- `ha_state` reads `/api/states/<entity>` and copies just the `state`
  value into `buf` (truncated to fit `len`, always NUL-terminated).
  It does not parse the rest of the JSON response. `len` must be at
  least 1; a `len` of 0 returns `ERR_ARG_USAGE` without making a
  request.
- `ha_template` POSTs `tmpl` to `/api/template` (after JSON-escaping
  it) and streams the rendered response back one line at a time
  through `cb`. Returning nonzero from `cb` stops the stream early;
  `ha_template` then returns `HA_OK` (not an error), but the TCP
  connection **is** closed, because the rest of the response body is
  still on the wire and cannot be skipped. The next call reconnects.
  A template rejected by HA (HTTP 400) is reported as
  `ERR_HA_TEMPLATE`, not the generic `ERR_HA_STATUS`.
- Every call sets `ctx->last_status` to the HTTP status HA returned
  (when a status was received at all - a transport-level failure
  leaves it unchanged). `ERR_HA_STATUS` in particular carries no
  detail beyond the code, so callers that want to show the HTTP status
  on a generic rejection should read `ctx->last_status`.
- `ha_init`/`ha_call`/`ha_state`/`ha_template` all use `Connection:
  keep-alive`, so a program making several calls in a row (e.g. a
  `toggle` followed by a `state` check) reuses one TCP connection
  instead of reconnecting each time. Call `ha_close(ctx)` once, when
  you are done making HA calls, to close that connection; do this
  before `tp_shutdown()`, not after.

### One call at a time

The library is not reentrant and holds no per-caller state beyond the
`ha_ctx` you pass in: the request buffer, the response line buffer, the
template body and the line splitter are all module statics shared by
every call. That means:

- Only one `ha_*` call may be in flight at a time.
- Never call an `ha_*` (or `http_*`, or `tp_*`) function from inside a
  line or body callback - the callback runs in the middle of the call
  that owns those buffers, and a nested call would overwrite them.
  Copy what you need out of the callback and act on it after the
  outer call returns.
- Use one `ha_ctx` at a time. Several `ha_ctx` values may exist (each
  keeps its own `Authorization` header and `last_status`), but they
  share the same transport, the same keep-alive connection and the
  same buffers, so they cannot be used concurrently or interleaved.

There is no threading on the Next, so this is only a constraint on
callbacks and on interrupt handlers: do not call into `ha.lib` from an
interrupt.

### `ha_config.h` (included by `ha_client.h`)

```c
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
```

See `docs/ha-cfg.md` for the `ha.cfg` file format and how to obtain a
token. `ha_config_load` is the normal entry point; `ha_config_parse` is
exposed for callers that already have the config text in memory (or
want to test their own config handling on the host - see
`test/test_config.c`).

## A minimal client program

```c
#include <stdio.h>
#include <stdlib.h>
#include "ha/ha_client.h"
#include "transport/transport.h"

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
```

This is `examples/c/toggle.c`. Note the shutdown order: `ha_close`
(closes the HTTP/TCP connection) before `tp_shutdown` (restores CPU
speed) - the reverse of init order (`ha_config_load`/`tp_init` then
`ha_init`).

## Building against `ha.lib`

Run `build.cmd` at the repository root first; besides `HA` it produces
`dist/ha.lib` and `dist/include/...` (the public headers above).

`examples/c/build.cmd` builds `toggle.c` against that `dist/` output:

```
zcc +zxn -v -startup=30 -clib=sdcc_iy -SO3 --max-allocs-per-node200000 --opt-code-size -subtype=dotn -pragma-include:..\..\zpragma.inc "-Ca-ID:\ZXNextDev\z88dk\lib\crt\newlib" -I..\..\dist\include -L..\..\dist -lha toggle.c -o TOGGLE -Cz"--clean" -create-app
```

Notes on that command line, for anyone adapting it to their own
program:

- `-I..\..\dist\include -L..\..\dist -lha` are what pull in `ha.lib`
  and its headers; everything else matches the root `build.cmd`'s
  `%ZCC_DOT%` line.
- `"-Ca-ID:\ZXNextDev\z88dk\lib\crt\newlib"` must be present (a z88dk
  newlib quirk: without it, the `printf` pragma's CRT include does not
  resolve). It is not optional even though the program itself does not
  reference that path.
- `-o TOGGLE` must be a bare name with no directory prefix and no more
  than 8 characters - z88dk-appmake truncates a longer `-o` path when
  producing the `dotn` binary, so `-o ..\out\toggle` or similar will
  not produce the file you expect.
- `-subtype=dotn` (a NextZXOS-only banked dot command) is required for
  any program that links `ha.lib`, for the same reason `.HA` itself is
  a `dotn`: the engine plus HTTP/ESP state does not fit in a plain
  48K-model dot's budget.
