# `.HA` dot command reference

`.HA` is a NextZXOS `dotn` command (a banked dot command; NextZXOS/esxdos
only). It calls a Home Assistant (HA) instance over Wi-Fi and either
prints the result to the screen or writes it into a 16K Next memory
bank for a NextBASIC or assembly program to read.

Configuration (host, port, token, template) comes from `ha.cfg` - see
`docs/ha-cfg.md`. `.HA` never prints a heap or template body larger than
what fits its own small stack buffers; the bank verbs exist so larger
results can be moved out to BASIC without the dot itself buffering them.

## Verbs

### `.ha on <entity>`
### `.ha off <entity>`
### `.ha toggle <entity>`

Calls the `homeassistant.turn_on` / `homeassistant.turn_off` /
`homeassistant.toggle` service with `entity_id` set to `<entity>`.
Prints nothing on success (exit code 0). On failure, exits with the
matching esxdos custom error (see `docs/errors.md`).

Example:

```
.ha toggle light.lounge
```

### `.ha call <domain> <service> <entity>`

Calls `<domain>.<service>` with `entity_id` set to `<entity>` (the
general form behind `on`/`off`/`toggle`, for any HA service that takes
a single `entity_id`). Prints nothing on success.

Example:

```
.ha call switch turn_on switch.kettle
```

### `.ha state <entity> [-b n]`

Reads `/api/states/<entity>` and prints the entity's `state` string to
the screen. If `-b n` is given, the same string is also written into
bank `n` in the bank format described below (see docs/cli.md#bank-format).

Example:

```
.ha state light.lounge
.ha state light.lounge -b 40
```

### `.ha tmpl -b n [-l len]`

Renders a Jinja2 template through HA's `/api/template` endpoint and
writes the (possibly multi-line) rendered text into bank `n`, one
bank-format line per line of output. `-b n` is required for this verb.

The template text itself is read from bank `n` *before* the request is
sent (see "Template input" below), so `-b` here does double duty: it
names the bank the template is read from and the bank the rendered
result is written back into.

Prints `N bytes` on success, or `N bytes (bank full)` if the bank
filled before all of the rendered output could be written (see "Bank
full" below).

Example:

```
.ha tmpl -b 40
.ha tmpl -b 40 -l 120
```

### No verb / unrecognised verb

`.ha` with no arguments prints a short usage summary to the screen and
exits 0. `.ha` with an unrecognised verb, or the wrong number of
positional arguments for a recognised verb, exits with the `H Usage:
...` esxdos error (see `docs/errors.md`).

## Options

| Option | Applies to | Range | Notes |
|---|---|---|---|
| `-b n` | `state`, `tmpl` | integer 0-111 | Selects a 16K Next memory bank (paged at 0xC000-0xFFFF for the duration of the operation). Banks 0-7 are the 128K system banks that NextZXOS and BASIC already use - pick a bank above 7 (NextBASIC's `BANK n` allocator hands out high banks) unless you know exactly what bank 0-7 holds. A non-numeric or out-of-range value exits with the `E Bank must be 0-111` error. `tmpl` requires `-b`; `state` without `-b` only prints to the screen. |
| `-l len` | `tmpl` | integer 0-511 | Overrides NUL-termination when reading the template from the bank: exactly `len` raw bytes are read instead of scanning for a NUL. A non-numeric or negative value exits with the `E Length must be a number` error; 512 or more exits with `L ha.cfg value too long`. Ignored by every other verb. |

Options may appear anywhere after the verb, in any order (they are
scanned out of `argv` before the remaining positional arguments are
matched against the verb).

## Bank format

Every bank `.ha` writes to (via `state -b` or `tmpl -b`) uses the same
2-byte length header followed by a payload, starting at the first byte
of the bank (offset 0 when the bank is mapped at 0xC000):

| Offset | Bytes | Contents |
|---|---|---|
| 0-1 | 2 | Payload length, little-endian (low byte at offset 0, high byte at offset 1). Counts only the payload bytes described below, not the header itself. |
| 2..1+length | `length` | Payload: one or more lines, each a run of text bytes followed by a single CR (0x0D, `\r`) terminator byte. The CR is included in `length`. |

- `state -b n` writes exactly one line: the entity's state string, CR-terminated.
- `tmpl -b n` writes one line per line of HA's rendered template output
  (split on `\n` by the client), each CR-terminated.
- The payload is **not NUL-terminated**. A NextBASIC program must use
  the length header (bytes 0-1) to know where the payload ends -
  reading past it will show whatever was left over from the bank's
  previous contents.
- Bank capacity is 16382 payload bytes (the bank is 16384 bytes; the
  2-byte header leaves 16382 for lines). If the rendered output would
  not fit, `tmpl` truncates the last line that does not fit whole (or
  drops it if there is no room left at all), stops streaming further
  HA output, and the dot prints `N bytes (bank full)` instead of
  `N bytes`. The bank still holds a valid, fully-written header and
  payload up to that point - it is not corrupt, just truncated.

### Template input (reading, not writing)

`tmpl -b n` first reads the template text to render *from* bank `n`,
starting at offset 0 (not offset 2 - that offset is only used for the
*output* format above). By default this is a NUL-terminated C string;
`-l len` overrides that and reads exactly `len` raw bytes instead. If
the effective length is 0 (an empty or all-zero bank, or `-l 0`), `.ha`
falls back to the template configured in `ha.cfg`, or the built-in
default template if `ha.cfg` does not set one.

A bank template is limited to 511 bytes, the same limit as a `template =`
line in `ha.cfg`. A longer one (or a bank with no NUL in it, when `-l`
is not given) exits with `L ha.cfg value too long` rather than being
rendered cut short.

Because a bank previously used to hold *output* (a length header plus
non-NUL-terminated CR-terminated lines) is not a valid NUL-terminated
*input* string, a bank must not be reused untouched between an
`.ha state -b`/`.ha tmpl -b` run and a following `.ha tmpl -b` run on
the same bank. The safe sequence is:

1. `BANK n ERASE` (or otherwise write your own NUL-terminated template
   text into bank `n` starting at offset 0).
2. `.ha tmpl -b n [-l len]`.
3. Read the result from offset 2 onward using the length header at
   offset 0-1, as described above - *before* reusing bank `n` for
   anything else.

`examples/basic/ha-demo.bas` shows why step 1 matters even when bank
`n` was already erased earlier in the program: it first runs
`.ha state light.lounge -b 40` (writing a length header plus one
CR-terminated line into bank 40), prints that back, and only then
erases bank 40 again (line 85) immediately before `.ha tmpl -b 40`
(line 90) - without that second erase, `tmpl` would try to render the
leftover state bytes as if they were a template.

## Exit behaviour

On success, `.ha` exits 0. `on`/`off`/`toggle`/`call` print nothing;
`state` prints the state string (and, if the state was truncated to
fit the caller's buffer, prints as much as fit); `tmpl` prints the byte
count written to the bank.

On failure, `.ha` exits through NextZXOS's esxdos custom-error
mechanism: the process exit code is a pointer to a message string (a
one-letter error code followed by human-readable text), which NextZXOS
displays the way it displays any other esxdos error. `.ha` prints
`HTTP nnn` before the error for any verb whose HA request returned an
unexpected status (mapped to the `S` error) - this is the shared
`fail()` path in `main.c`, not something specific to `state`.
See `docs/errors.md` for the full table of letters and messages.

The 16K bank mapped by `-b` is always unmapped (the MMU pages it
replaced are restored) before `.ha` prints anything or exits, whether
it succeeds or fails - this happens even when `.ha` exits through the
esxdos error path.
