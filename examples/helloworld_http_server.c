#include "asyncd/asyncd.h"

/*---------------------------------------------------------------------------*\
|                          THIS IS AN EXAMPLEIN                               |
\*---------------------------------------------------------------------------*/

int my_http_get_handler(short event, ad_conn_t *conn, void *userdata) {
    if (ad_http_get_status(conn) == AD_HTTP_REQ_DONE) {
        ad_http_response(conn, 200, "text/html", "Hello World", 11);
        return AD_DONE;
    }
    return AD_OK;
}

int my_http_default_handler(short event, ad_conn_t *conn, void *userdata) {
    if (ad_http_get_status(conn) == AD_HTTP_REQ_DONE) {
        ad_http_response(conn, 200, "text/html", "hello world", 11);
        return AD_DONE;
    }
    return AD_OK;
}

int main(int argc, char **argv) {
    ad_log_level(AD_LOG_DEBUG);
    ad_server_t *server = ad_server_new();
    ad_server_set_option(server, "server.port", "8888");
    ad_server_register_hook(server, ad_http_handler, NULL); // HTTP Parser is also a hook.
    ad_server_register_hook_on_method(server, "GET", my_http_get_handler, NULL);
    ad_server_register_hook(server, my_http_default_handler, NULL);
    return ad_server_start(server);
}
