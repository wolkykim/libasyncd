#include <event2/buffer.h>
#include "asyncd/asyncd.h"

/*---------------------------------------------------------------------------*\
|                          THIS IS AN EXAMPLEIN                               |
\*---------------------------------------------------------------------------*/

int my_conn_handler(short event, ad_conn_t *conn, void *userdata) {
    if (event & AD_EVENT_WRITE) {
        evbuffer_add_printf(conn->out, "Hello World.\n");
        return AD_CLOSE;
    }
    return AD_OK;
}

int main(int argc, char **argv) {
    ad_log_level(AD_LOG_DEBUG);
    ad_server_t *server = ad_server_new();
    ad_server_set_option(server, "server.port", "2222");
    ad_server_set_option(server, "server.enable_ssl", "1");
    ad_server_set_option(server, "server.ssl_cert", "ssl.cert");
    ad_server_set_option(server, "server.ssl_pkey", "ssl.pkey");
    ad_server_register_hook(server, my_conn_handler, NULL);
    return ad_server_start(server);
}
