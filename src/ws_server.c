#include <libwebsockets.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#define PORT 8000

static int interrupted;

struct msg {
    void *payload;
    size_t len;
};

struct pss {
    struct pss *pss_list;
    struct lws *wsi;
    int last;
};

struct vhd {
    struct lws_context *context;
    struct lws_vhost *vhost;
    const struct lws_protocols *protocol;
    struct pss *pss_list;
    struct msg amsg;
    int current;
};

static void destroy_message(void *_msg)
{
    struct msg *m = _msg;
    free(m->payload);
    m->payload = NULL;
    m->len = 0;
}

static int callback_broadcast(struct lws *wsi, enum lws_callback_reasons reason,
                              void *user, void *in, size_t len)
{
    struct pss *pss = (struct pss *)user;
    struct vhd *vhd = (struct vhd *)lws_protocol_vh_priv_get(
        lws_get_vhost(wsi), lws_get_protocol(wsi));
    int m;

    switch (reason) {
    case LWS_CALLBACK_PROTOCOL_INIT:
        vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
                lws_get_protocol(wsi), sizeof(struct vhd));
        vhd->context = lws_get_context(wsi);
        vhd->protocol = lws_get_protocol(wsi);
        vhd->vhost = lws_get_vhost(wsi);
        break;

    case LWS_CALLBACK_ESTABLISHED:
        lwsl_user("Client connected\n");
        pss->pss_list = vhd->pss_list;
        vhd->pss_list = pss;
        pss->wsi = wsi;
        pss->last = vhd->current;
        break;

    case LWS_CALLBACK_CLOSED:
        lwsl_user("Client disconnected\n");
        lws_start_foreach_llp(struct pss **, ppss, vhd->pss_list) {
            if (*ppss == pss) {
                *ppss = pss->pss_list;
                break;
            }
        } lws_end_foreach_llp(ppss, pss_list);
        break;

    case LWS_CALLBACK_SERVER_WRITEABLE:
        if (!vhd->amsg.payload)
            break;
        if (pss->last == vhd->current)
            break;

        m = lws_write(wsi, ((unsigned char *)vhd->amsg.payload) + LWS_PRE,
                      vhd->amsg.len, LWS_WRITE_TEXT);
        if (m < (int)vhd->amsg.len) {
            lwsl_err("ERROR %d writing to ws\n", m);
            return -1;
        }
        pss->last = vhd->current;
        break;

    case LWS_CALLBACK_RECEIVE:
        if (vhd->amsg.payload)
            destroy_message(&vhd->amsg);

        vhd->amsg.len = len;
        vhd->amsg.payload = malloc(LWS_PRE + len);
        if (!vhd->amsg.payload) {
            lwsl_user("OOM: dropping\n");
            break;
        }

        memcpy((char *)vhd->amsg.payload + LWS_PRE, in, len);
        vhd->current++;

        lwsl_user("Received: %.*s\n", (int)len, (char *)in);

        lws_start_foreach_llp(struct pss **, ppss, vhd->pss_list) {
            lws_callback_on_writable((*ppss)->wsi);
        } lws_end_foreach_llp(ppss, pss_list);
        break;

    default:
        break;
    }

    return 0;
}

static struct lws_protocols protocols[] = {
    { "http", lws_callback_http_dummy, 0, 0, 0, NULL, 0 },
    { "ws-broadcast", callback_broadcast, sizeof(struct pss), 4096, 0, NULL, 0 },
    LWS_PROTOCOL_LIST_TERM
};

void sigint_handler(int sig)
{
    interrupted = 1;
}

int main(int argc, const char **argv)
{
    struct lws_context_creation_info info;
    struct lws_context *context;
    int n = 0;

    signal(SIGINT, sigint_handler);

    int logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;
    lws_set_log_level(logs, NULL);

    lwsl_user("WebSocket Server starting on port %d\n", PORT);

    memset(&info, 0, sizeof(info));
    info.port = PORT;
    info.protocols = protocols;
    info.options = LWS_SERVER_OPTION_VALIDATE_UTF8;

    context = lws_create_context(&info);
    if (!context) {
        lwsl_err("lws init failed\n");
        return 1;
    }

    lwsl_user("Server running at ws://localhost:%d\n", PORT);

    while (n >= 0 && !interrupted)
        n = lws_service(context, 0);

    lws_context_destroy(context);
    lwsl_user("Server stopped\n");

    return 0;
}
