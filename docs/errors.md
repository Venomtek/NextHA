# Error reference

Every function in `ha.lib` returns `HA_OK` (0) on success or a negative
`ERR_*` code from `src/ha_errors.h` on failure. `.HA` never prints
these codes directly: it maps each one to a one-letter esxdos custom
error and a short message (`src/dot/errors.asm`), and exits through
NextZXOS's custom-error mechanism so the message is shown the way any
other esxdos error would be.

A C program linking `ha.lib` directly gets the raw `int` return value
from each call and can act on it (or print it) however it wants -
`docs/c-api.md` covers the library side; this page is the shared
reference for what each code means and why it typically happens.

| Constant | Value | `.ha` letter | `.ha` message | Likely cause |
|---|---|---|---|---|
| `HA_OK` | 0 | - | (no output on success for `on`/`off`/`toggle`/`call`) | success |
| `ERR_TP_ESP_NONE` | -1 | `1` | No response from ESP | The ESP-01 module did not answer `ATE0` at all, even after one `AT+RST` recovery attempt - not wired/powered, wrong UART pins, or the module is unresponsive. |
| `ERR_TP_ESP_INIT` | -2 | `6` | WiFi init failed | The ESP responded but `AT+CIPMUX=0` still failed after an `AT+RST` recovery attempt and waiting for `WIFI GOT IP`/`ready` - the ESP could not associate to its configured Wi-Fi network. |
| `ERR_TP_CONNECT` | -3 | `2` | Cannot reach HA host | `AT+CIPSTART` failed - wrong `host`/`port` in `ha.cfg`, HA is down, or the ESP cannot route to it. |
| `ERR_TP_TIMEOUT` | -4 | `B` | Timeout talking to HA | No reply arrived from the ESP or HA within the per-byte timeout, or the exchange ran past the overall 30-second deadline that starts when the request finished being sent - a dead link, an overloaded HA instance, or a hung TCP connection. |
| `ERR_TP_SEND` | -5 | `7` | Send to HA failed | The `AT+CIPSEND` / `>` prompt / `SEND OK` sequence failed on the UART link while writing a request. |
| `ERR_TP_BREAK` | -6 | `D` | BREAK into program | CAPS SHIFT + SPACE was held while the transport was waiting for the ESP, so the operation was abandoned. Not an error condition on the link - the user asked to stop. |
| `ERR_HTTP_STATUS` | -10 | `8` | Bad HTTP response | The response's first line was not a well-formed `HTTP/1.x nnn ...` status line. |
| `ERR_HTTP_TRUNCATED` | -11 | `8` | Bad HTTP response | The connection closed (or a body callback aborted the stream) before the declared `Content-Length` bytes were fully read. |
| `ERR_HTTP_CHUNKED` | -12 | `8` | Bad HTTP response | The server replied with `Transfer-Encoding: chunked`, which v1 detects and rejects rather than decodes. |
| `ERR_HTTP_BADLEN` | -13 | `8` | Bad HTTP response | The `Content-Length` header had no digits, or specified an unreasonably large body (v1 caps this at ~6.5 MB). |
| `ERR_HA_AUTH` | -20 | `U` | Check token in ha.cfg | HA returned HTTP 401 or 403 - the token is missing, wrong, or has been revoked in HA. |
| `ERR_HA_STATUS` | -21 | `S` | HA rejected request (`.ha` also prints `HTTP nnn` first, for any verb) | HA returned a non-2xx status that is not 401/403 (auth) and not the template-specific 400 case below - typically an unknown entity, domain, or service. |
| `ERR_HA_TEMPLATE` | -22 | `T` | HA rejected template | `POST /api/template` returned HTTP 400 - the rendered or submitted Jinja2 template was malformed. |
| `ERR_CFG_MISSING` | -30 | `C` | ha.cfg not found | Neither `ha.cfg` (current directory) nor `c:/sys/ha.cfg` could be opened. |
| `ERR_CFG_KEY` | -31 | `K` | ha.cfg needs host+token | `host` and/or `token` is missing, or present but empty/whitespace-only, in `ha.cfg`. |
| `ERR_CFG_TOOLONG` | -32 | `L` | ha.cfg value too long | A `host`, `token`, or `template` value exceeds its field's limit (63/255/511 characters), a bank template passed to `.ha tmpl` exceeds 511 bytes, the `ha.cfg` file itself is 1023 bytes or larger, or `port` exceeds 65535. |
| `ERR_ARG_USAGE` | -40 | `H` | Usage: .ha on\|off\|toggle\|state\|call\|tmpl | Wrong verb, wrong number of positional arguments for the verb, or `-b`/`-l` given with no value following it. Also the fallback for any return code `.HA` does not otherwise recognise. |
| `ERR_ARG_BANK` | -41 | `E` | Bank must be 0-111 | `-b` value is not numeric, or is outside 0-111. |
| `ERR_ARG_LEN` | -42 | `E` | Length must be a number | `-l` value is not numeric, or is negative. |

Notes:

- `ERR_HTTP_STATUS`, `ERR_HTTP_TRUNCATED`, `ERR_HTTP_CHUNKED` and
  `ERR_HTTP_BADLEN` all map to the same `.ha` message ("Bad HTTP
  response", letter `8`) - the distinction only matters to a C caller
  inspecting the raw return value.
- `ERR_ARG_BANK` and `ERR_ARG_LEN` share the same letter (`E`) but
  different message text; `.ha`'s exit code (the message pointer) still
  distinguishes them even though the leading letter is the same.
- HTTP 403 maps to `ERR_HA_AUTH` (not `ERR_HA_STATUS`) on purpose: HA
  answers 403 when the caller's IP has been banned by its login-attempt
  protection, and a stale or wrong token is by far the most common way
  to get there, so "Check token in ha.cfg" is the useful message for
  both 401 and 403.
- `ha_ctx.last_status` holds the last HTTP status HA returned (when one
  was received at all); `.ha` prints it before the error on
  `ERR_HA_STATUS`, for any verb whose HA request returned an
  unexpected status - not just `state` - and a C caller can read it
  directly for any other outcome.
