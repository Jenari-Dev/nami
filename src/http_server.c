/* Nami - minimal HTTP server (M0 pipeline proof)
 *
 * Intentionally tiny: one static page, one thread, close after each
 * response. This exists only to prove the console can serve the PC a
 * page. It gets replaced by a real router + the desktop web UI at M5.
 */
#include "http_server.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char RESPONSE_FMT[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Content-Length: %zu\r\n"
    "Connection: close\r\n"
    "\r\n"
    "%s";

int
http_server_run(int port, const char *html_body) {
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) {
        return -1;
    }

    int one = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((unsigned short)port);

    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(srv);
        return -2;
    }
    if (listen(srv, 8) < 0) {
        close(srv);
        return -3;
    }

    size_t body_len = strlen(html_body);
    size_t resp_cap = body_len + sizeof(RESPONSE_FMT) + 32;
    char *resp = (char *)malloc(resp_cap);
    if (!resp) {
        close(srv);
        return -4;
    }
    int resp_len = snprintf(resp, resp_cap, RESPONSE_FMT, body_len, html_body);
    if (resp_len < 0) {
        free(resp);
        close(srv);
        return -5;
    }

    char reqbuf[2048];
    for (;;) {
        int cli = accept(srv, NULL, NULL);
        if (cli < 0) {
            continue;
        }
        /* Drain the request line/headers; we don't route yet. */
        recv(cli, reqbuf, sizeof(reqbuf), 0);
        send(cli, resp, (size_t)resp_len, 0);
        close(cli);
    }

    /* unreachable */
    free(resp);
    close(srv);
    return 0;
}
