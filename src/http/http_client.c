#include <string.h>
#include "http_client.h"
#include "../transport/transport.h"
#include "../ha_errors.h"

#define READ_TIMEOUT_MS 5000

static unsigned char conn_open;                 /* keep-alive state */
static char conn_host[64]; static unsigned conn_port;
static char num[8];

/* Request accumulator. Every tp_write on the Next costs a whole AT+CIPSEND round trip
   (command, '>' prompt, payload, SEND OK), so the request line, the Host: header and the
   caller headers are staged here and flushed as one write; the byte stream on the wire is
   identical to writing each piece separately. read_line() borrows the same buffer for
   response header lines - the request is always fully flushed before the first response
   byte is read, so the two uses never overlap. */
static char req[640];
static unsigned req_n;

static int req_flush(void)
{
    int rc = 0;
    if (req_n) { rc = tp_write(req, req_n); req_n = 0; }
    return rc;
}

/* Append s, flushing first if it will not fit. Returns 0 or a tp error. */
static int req_put(const char *s)
{
    unsigned len = (unsigned)strlen(s); int rc;
    if (len > (unsigned)sizeof req - req_n && (rc = req_flush())) return rc;
    if (len > sizeof req) return tp_write(s, len);      /* one piece bigger than the buffer */
    memcpy(req + req_n, s, len); req_n += len;
    return 0;
}

static char *utoa_dec(unsigned v) {
    char *p = num + 7; *p = 0;
    do { *--p = (char)('0' + v % 10); v /= 10; } while (v);
    return p;
}

void http_close(void) { if (conn_open) { tp_close(); conn_open = 0; } }

static int ensure_open(const http_req *r)
{
    int rc;
    /* host must fit conn_host; longer names would alias on keep-alive reuse */
    if (strlen(r->host) >= sizeof conn_host) return ERR_TP_CONNECT;
    if (conn_open && conn_port == r->port && strcmp(conn_host, r->host) == 0) return 0;
    http_close();
    rc = tp_open(r->host, r->port);
    if (rc < 0) return rc;
    conn_open = 1; conn_port = r->port;
    strncpy(conn_host, r->host, sizeof conn_host - 1); conn_host[sizeof conn_host - 1] = 0;
    return 0;
}

static int send_request(const http_req *r)
{
    const char *const *h; int rc;
    req_n = 0;
    if ((rc = req_put(r->method)) || (rc = req_put(" ")) || (rc = req_put(r->path)) ||
        (rc = req_put(" HTTP/1.1\r\nHost: ")) || (rc = req_put(r->host))) return rc;
    if (r->port != 80) { if ((rc = req_put(":")) || (rc = req_put(utoa_dec(r->port)))) return rc; }
    if ((rc = req_put("\r\n"))) return rc;
    for (h = r->headers; h && *h; h++) if ((rc = req_put(*h)) || (rc = req_put("\r\n"))) return rc;
    if (r->body_len) { if ((rc = req_put("Content-Length: ")) || (rc = req_put(utoa_dec(r->body_len))) || (rc = req_put("\r\n"))) return rc; }
    if ((rc = req_put(r->keep_alive ? "Connection: keep-alive\r\n\r\n" : "Connection: close\r\n\r\n"))) return rc;
    if (r->body_len && r->body_len <= (unsigned)sizeof req - req_n) {   /* body rides along with the headers */
        memcpy(req + req_n, r->body, r->body_len); req_n += r->body_len;
        return req_flush();
    }
    if ((rc = req_flush())) return rc;
    if (r->body_len) return tp_write(r->body, r->body_len);
    return 0;
}

/* Reads one CRLF-terminated line (CR LF stripped) one byte at a time into req[].
   Returns length, ERR_HTTP_TRUNCATED on close, or a tp error. Over-long lines are truncated but consumed. */
static int read_line(void)
{
    unsigned n = 0; char c; int rc;
    for (;;) {
        rc = tp_read(&c, 1, READ_TIMEOUT_MS);
        if (rc == 0) return ERR_HTTP_TRUNCATED;
        if (rc < 0) return rc;
        if (c == '\n') { if (n && req[n-1] == '\r') n--; req[n] = 0; return (int)n; }
        if (n < sizeof req - 1) req[n++] = c;
    }
}

static int ieq_prefix(const char *s, const char *prefix)   /* case-insensitive prefix match */
{
    while (*prefix) { char a = *s++, b = *prefix++;
        if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
        if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
        if (a != b) return 0; }
    return 1;
}

static int read_response(http_body_cb cb, void *user, unsigned char *server_closes)
{
    int rc, status; unsigned long clen = 0; unsigned char have_len = 0, chunked = 0;
    static char buf[64];

    rc = read_line(); if (rc < 0) return rc;
    if (!(rc >= 12 && ieq_prefix(req, "HTTP/1.") && req[8] == ' ' &&
          req[9] >= '0' && req[9] <= '9' && req[10] >= '0' && req[10] <= '9' && req[11] >= '0' && req[11] <= '9'))
        return ERR_HTTP_STATUS;
    status = (req[9]-'0')*100 + (req[10]-'0')*10 + (req[11]-'0');

    for (;;) {
        rc = read_line(); if (rc < 0) return rc;
        if (rc == 0) break;                                   /* blank line: end of headers */
        if (ieq_prefix(req, "content-length:")) {
            const char *p = req + 15; have_len = 1; clen = 0;
            while (*p == ' ') p++;
            if (*p < '0' || *p > '9') return ERR_HTTP_BADLEN;
            while (*p >= '0' && *p <= '9') {
                if (clen > 6553500UL) return ERR_HTTP_BADLEN;   /* cap ~6.5MB; HA text is tiny */
                clen = clen * 10 + (unsigned)(*p++ - '0');
            }
        } else if (ieq_prefix(req, "transfer-encoding:")) {
            if (strstr(req, "chunked") || strstr(req, "Chunked")) chunked = 1;
        } else if (ieq_prefix(req, "connection:")) {
            if (strstr(req, "close") || strstr(req, "Close")) *server_closes = 1;
        }
    }
    if (chunked) return ERR_HTTP_CHUNKED;

    if (have_len) {
        while (clen) {
            unsigned want = clen > sizeof buf ? (unsigned)sizeof buf : (unsigned)clen;
            rc = tp_read(buf, want, READ_TIMEOUT_MS);
            if (rc == 0) return ERR_HTTP_TRUNCATED;
            if (rc < 0) return rc;
            if (cb(buf, (unsigned)rc, user)) return ERR_HTTP_TRUNCATED;
            clen -= (unsigned)rc;
        }
    } else {                                                   /* no length: read to close */
        *server_closes = 1;
        for (;;) {
            rc = tp_read(buf, sizeof buf, READ_TIMEOUT_MS);
            if (rc == 0) break;
            if (rc < 0) return rc;
            if (cb(buf, (unsigned)rc, user)) return ERR_HTTP_TRUNCATED;
        }
    }
    return status;
}

int http_request(const http_req *r, http_body_cb cb, void *user)
{
    int rc; unsigned char server_closes = 0;
    rc = ensure_open(r); if (rc < 0) return rc;
    rc = send_request(r);
    if (rc >= 0) rc = read_response(cb, user, &server_closes);
    if (rc < 0 || !r->keep_alive || server_closes) http_close();
    return rc;
}
