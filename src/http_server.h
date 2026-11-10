/* Nami - minimal HTTP server (M0 pipeline proof) */
#ifndef NAMI_HTTP_SERVER_H
#define NAMI_HTTP_SERVER_H

/* Serve a single static HTML body on `port` to every GET request.
 * Blocks forever (keeps the payload resident). Returns negative on
 * socket setup failure. */
int http_server_run(int port, const char *html_body);

#endif /* NAMI_HTTP_SERVER_H */
