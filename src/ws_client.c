#include <libwebsockets.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define PORT 8000
#define SERVER_ADDR "localhost"

static struct lws_context *context;
static int interrupted;

struct my_conn {
    lws_sorted_usec_list_t sul;
    struct lws *wsi;
    uint16_t retry_count;
};

static struct my_conn mco;

static const uint32_t backoff_ms[] = { 1000, 2000, 3000, 4000, 5000 };

static const lws_retry_bo_t retry = {
    .retry_ms_table = backoff_ms,
    .retry_ms_table_count = LWS_ARRAY_SIZE(backoff_ms),
    .conceal_count = LWS_ARRAY_SIZE(backoff_ms),
    .secs_since_valid_ping = 3,
    .secs_since_valid_hangup = 10,
    .jitter_percent = 20,
};

static void connect_client(lws_sorted_usec_list_t *sul)
{
    struct my_conn *m = lws_container_of(sul, struct my_conn, sul);
    struct lws_client_connect_info i;

    memset(&i, 0, sizeof(i));

    i.context = context;
    i.port = PORT;
    i.address = SERVER_ADDR;
    i.path = "/";
    i.host = i.address;
    i.origin = i.address;
    i.ssl_connection = 0;
    i.protocol = "ws-broadcast";
    i.local_protocol_name = "ws-broadcast-client";
    i.pwsi = &m->wsi;
    i.retry_and_idle_policy = &retry;
    i.userdata = m;

    if (!lws_client_connect_via_info(&i)) {
        if (lws_retry_sul_schedule(context, 0, sul, &retry,
                                   connect_client, &m->retry_count)) {
            lwsl_err("Connection attempts exhausted\n");
            interrupted = 1;
        }
    }
}

static int callback_ws_client(struct lws *wsi, enum lws_callback_reasons reason,
                              void *user, void *in, size_t len)
{
    struct my_conn *m = (struct my_conn *)user;

    switch (reason) {
    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        lwsl_err("CLIENT_CONNECTION_ERROR: %s\n",
                 in ? (char *)in : "(null)");
        goto do_retry;

    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        lwsl_user("Connected to server ws://%s:%d\n", SERVER_ADDR, PORT);
        lws_callback_on_writable(wsi);
        break;

    case LWS_CALLBACK_CLIENT_RECEIVE:
        printf("Received: %.*s\n", (int)len, (char *)in);
        break;

    case LWS_CALLBACK_CLIENT_CLOSED:
        lwsl_user("Disconnected from server\n");
        goto do_retry;

    case LWS_CALLBACK_CLIENT_WRITEABLE: {
        char buf[256];
        int n;
        printf("Enter message (or 'quit' to exit): ");
        fflush(stdout);

        if (fgets(buf, sizeof(buf), stdin) == NULL) {
            interrupted = 1;
            break;
        }

        n = strlen(buf);
        if (n > 0 && buf[n - 1] == '\n')
            buf[n - 1] = '\0';
        n = strlen(buf);

        if (strcmp(buf, "quit") == 0) {
            interrupted = 1;
            break;
        }

        if (n > 0) {
            unsigned char outbuf[LWS_PRE + 256];
            memcpy(outbuf + LWS_PRE, buf, n);
            int m = lws_write(wsi, outbuf + LWS_PRE, n, LWS_WRITE_TEXT);
            if (m < n) {
                lwsl_err("ERROR %d writing to ws\n", m);
                return -1;
            }
            lws_callback_on_writable(wsi);
        } else {
            lws_callback_on_writable(wsi);
        }
        break;
    }

    default:
        break;
    }

    return lws_callback_http_dummy(wsi, reason, user, in, len);

do_retry:
    if (lws_retry_sul_schedule_retry_wsi(wsi, &m->sul, connect_client,
                                          &m->retry_count)) {
        lwsl_err("Connection attempts exhausted\n");
        interrupted = 1;
    }
    return 0;
}

static const struct lws_protocols protocols[] = {
    { "ws-broadcast-client", callback_ws_client,
      sizeof(struct my_conn), 4096, 0, NULL, 0 },
    LWS_PROTOCOL_LIST_TERM
};

void sigint_handler(int sig)
{
    interrupted = 1;
}

int main(int argc, const char **argv)
{
    struct lws_context_creation_info info;
    int n = 0;

    signal(SIGINT, sigint_handler);

    int logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;
    lws_set_log_level(logs, NULL);

    lwsl_user("WebSocket Client connecting to ws://%s:%d\n", SERVER_ADDR, PORT);

    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    info.fd_limit_per_thread = 1 + 1 + 1;

    context = lws_create_context(&info);
    if (!context) {
        lwsl_err("lws init failed\n");
        return 1;
    }

    lws_sul_schedule(context, 0, &mco.sul, connect_client, 1);

    while (n >= 0 && !interrupted)
        n = lws_service(context, 0);

    lws_context_destroy(context);
    lwsl_user("Client stopped\n");

    return 0;
}
