#ifndef NEXTHA_HTTP_CLIENT_H
#define NEXTHA_HTTP_CLIENT_H

typedef int (*http_body_cb)(const char *chunk, unsigned len, void *user);  /* return 0 to continue; nonzero aborts -> ERR_HTTP_TRUNCATED */

typedef struct {
    const char *method; const char *host; unsigned port; const char *path;
    const char *const *headers;   /* NULL-terminated array of "Name: value" */
    const char *body; unsigned body_len;
    unsigned char keep_alive;
} http_req;

int  http_request(const http_req *req, http_body_cb on_chunk, void *user);  /* status or ERR_* */
void http_close(void);

#endif
