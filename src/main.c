/* Nami - PS5 homebrew shell (multiMAN-style).
 *
 * M0: prove the pipeline end to end.
 *   - pop an on-TV toast so we can see the payload actually ran
 *   - serve a status page to the PC over the network
 * No graphics yet; SDL2 + the XMB shell arrive at M1.
 */
#include <stdio.h>
#include <stdbool.h>

#include "http_server.h"

#define NAMI_WEB_PORT 8080
#define SCE_NOTIFICATION_LOCAL_USER_ID_SYSTEM 0xFE

/* Provided by libSceNotification (see Makefile -lSceNotification). */
int sceNotificationSend(int userId, bool isLogged, const char *payload);

/* On-screen toast. Body is injected twice: once for the full toast and
 * once for the preview-disabled view, matching the SDK notify sample. */
static void
nami_notify(const char *body) {
    char buf[1024];
    snprintf(buf, sizeof(buf),
        "{\"rawData\":{"
        "\"viewTemplateType\":\"InteractiveToastTemplateB\","
        "\"channelType\":\"Downloads\",\"useCaseId\":\"IDC\","
        "\"toastOverwriteType\":\"No\",\"isImmediate\":true,\"priority\":100,"
        "\"viewData\":{"
        "\"message\":{\"body\":\"%s\"},"
        "\"subMessage\":{\"body\":\"Nami M0\"}},"
        "\"platformViews\":{\"previewDisabled\":{\"viewData\":{"
        "\"icon\":{\"type\":\"Predefined\",\"parameters\":{\"icon\":\"download\"}},"
        "\"message\":{\"body\":\"%s\"}}}}},"
        "\"localNotificationId\":\"70010001\"}",
        body, body);
    sceNotificationSend(SCE_NOTIFICATION_LOCAL_USER_ID_SYSTEM, true, buf);
}

static const char INDEX_HTML[] =
    "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>Nami</title><style>"
    "*{box-sizing:border-box}"
    "body{margin:0;min-height:100vh;display:flex;align-items:center;"
    "justify-content:center;font-family:system-ui,sans-serif;color:#eaf2ff;"
    "background:radial-gradient(120% 120% at 50% 0%,#17365c,#0a1322 60%,#060a12)}"
    ".card{text-align:center;padding:48px 56px}"
    ".logo{font-size:64px;font-weight:800;letter-spacing:2px;margin:0;"
    "background:linear-gradient(90deg,#7fd0ff,#bda6ff);"
    "-webkit-background-clip:text;background-clip:text;color:transparent}"
    ".sub{opacity:.7;margin:.4em 0 1.6em;font-size:15px;letter-spacing:3px;"
    "text-transform:uppercase}"
    ".ok{display:inline-flex;align-items:center;gap:10px;padding:10px 20px;"
    "border-radius:999px;font-size:14px;"
    "border:1px solid #2bd47e;color:#9ff0c2;background:#0e2a1d}"
    ".dot{width:9px;height:9px;border-radius:50%;background:#2bd47e;"
    "box-shadow:0 0 12px #2bd47e}"
    ".ver{margin-top:22px;opacity:.4;font-size:12px}"
    "</style></head><body><div class=\"card\">"
    "<h1 class=\"logo\">波 Nami</h1>"
    "<div class=\"sub\">PS5 Shell</div>"
    "<div class=\"ok\"><span class=\"dot\"></span>Pipeline alive — console is serving this page</div>"
    "<div class=\"ver\">M0 · build -&gt; load -&gt; run -&gt; network OK</div>"
    "</div></body></html>";

int
main(void) {
    nami_notify("Nami is running - web UI on port 8080");
    /* Blocks forever serving the status page. */
    http_server_run(NAMI_WEB_PORT, INDEX_HTML);
    return 0;
}
