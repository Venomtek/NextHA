#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET sock_t;
#define BAD_SOCK INVALID_SOCKET
#else
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/select.h>
typedef int sock_t;
#define BAD_SOCK (-1)
#define closesocket close
#endif
#include <stdio.h>
#include <string.h>
#include "transport.h"
#include "../ha_errors.h"

static sock_t s = BAD_SOCK;

int tp_init(void) {
#ifdef _WIN32
    WSADATA w; return WSAStartup(MAKEWORD(2,2), &w) ? ERR_TP_ESP_NONE : 0;
#else
    return 0;
#endif
}
void tp_shutdown(void) { }

int tp_open(const char *host, unsigned port) {
    struct addrinfo hints, *res, *p; char ps[12];
    memset(&hints, 0, sizeof hints); hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    sprintf(ps, "%u", port);
    if (getaddrinfo(host, ps, &hints, &res)) return ERR_TP_CONNECT;
    for (p = res; p; p = p->ai_next) {
        s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s == BAD_SOCK) continue;
        if (connect(s, p->ai_addr, (int)p->ai_addrlen) == 0) break;
        closesocket(s); s = BAD_SOCK;
    }
    freeaddrinfo(res);
    return s == BAD_SOCK ? ERR_TP_CONNECT : 0;
}
int tp_write(const void *buf, unsigned len) {
    const char *b = (const char *)buf;
    while (len) { int n = send(s, b, (int)len, 0); if (n <= 0) return ERR_TP_SEND; b += n; len -= (unsigned)n; }
    return 0;
}
int tp_read(void *buf, unsigned len, unsigned timeout_ms) {
    fd_set fs; struct timeval tv; int n;
    FD_ZERO(&fs); FD_SET(s, &fs); tv.tv_sec = timeout_ms / 1000; tv.tv_usec = (timeout_ms % 1000) * 1000;
    n = select((int)s + 1, &fs, 0, 0, &tv);
    if (n == 0) return ERR_TP_TIMEOUT;
    if (n < 0) return ERR_TP_CONNECT;
    n = recv(s, (char *)buf, (int)len, 0);
    if (n == 0) return 0;
    return n < 0 ? ERR_TP_CONNECT : n;
}
int tp_close(void) { if (s != BAD_SOCK) { closesocket(s); s = BAD_SOCK; } return 0; }
