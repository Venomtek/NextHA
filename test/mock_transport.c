#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mock_transport.h"
#include "transport/transport.h"
#include "ha_errors.h"

#define MOCK_CAP 8192
static char resp[MOCK_CAP + 1]; static unsigned resp_len, resp_pos, chunk;
static char written[MOCK_CAP + 1]; static unsigned written_len;
static int open_rc;
int mock_tp_open_calls, mock_tp_close_calls, mock_tp_write_calls;
char mock_tp_last_host[64]; unsigned mock_tp_last_port;

void mock_tp_reset(void) {
    resp_len = resp_pos = chunk = written_len = 0; open_rc = 0;
    mock_tp_open_calls = mock_tp_close_calls = mock_tp_write_calls = 0;
    mock_tp_last_host[0] = 0; mock_tp_last_port = 0;
}
void mock_tp_script_response(const char *b, unsigned n) {
    if (resp_len + n > MOCK_CAP) {
        fprintf(stderr, "mock_transport: buffer overflow (%u + %u > %u)\n", resp_len, n, MOCK_CAP);
        abort();
    }
    memcpy(resp + resp_len, b, n); resp_len += n;
}
void mock_tp_set_read_chunk(unsigned n) { chunk = n; }
void mock_tp_set_open_result(int rc) { open_rc = rc; }
const char *mock_tp_written(unsigned *len) { *len = written_len; written[written_len] = 0; return written; }

int tp_init(void) { return 0; }
int tp_open(const char *host, unsigned port) {
    mock_tp_open_calls++; strncpy(mock_tp_last_host, host, 63); mock_tp_last_host[63] = 0;
    mock_tp_last_port = port; return open_rc;
}
int tp_write(const void *b, unsigned n) {
    mock_tp_write_calls++;
    if (written_len + n > MOCK_CAP) {
        fprintf(stderr, "mock_transport: buffer overflow (%u + %u > %u)\n", written_len, n, MOCK_CAP);
        abort();
    }
    memcpy(written + written_len, b, n); written_len += n; return 0;
}
int tp_read(void *b, unsigned n, unsigned timeout_ms) {
    unsigned avail = resp_len - resp_pos; (void)timeout_ms;
    if (avail == 0) return 0;
    if (n > avail) n = avail;
    if (chunk && n > chunk) n = chunk;
    memcpy(b, resp + resp_pos, n); resp_pos += n; return (int)n;
}
int tp_close(void) { mock_tp_close_calls++; return 0; }
void tp_shutdown(void) { }
