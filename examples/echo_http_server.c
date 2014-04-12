#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "asyncd/asyncd.h"
#include "macro.h"

/*---------------------------------------------------------------------------*\
|                          THIS IS AN EXAMPLEIN                               |
\*---------------------------------------------------------------------------*/

/**
 * User data for per-connection custom information for non-blocking operation.
 */
struct my_cdata {
    int counter;
};

void my_userdata_free_cb(ad_conn_t *conn, void *userdata) {
    free(userdata);
}

/**
 * User callback example.
 *
 * This is a simple echo handler.
 * It response on input line up to 3 times then close connection.
 *
 * @param event event type. see ad_server.h for details.
 * @param conn  connection object. type is vary based on
 *              "server.protocol_handler" option.
 * @userdata    given shared user-data.
 *
 * @return one of AD_OK | AD_DONE | AD_CLOSE | AD_TAKEOVER
 *
 * @note Please refer ad_server.h for more details.
 */
int my_conn_handler(short event, ad_conn_t *conn, void *userdata) {
    DEBUG("my_conn_callback: %x", event);

    /*
     * AD_EVENT_INIT event is like a constructor method.
     * It happens only once at the beginning of connection.
     * This is a good place to create a per-connection base
     * resources. You can attach it into this connection to
     * use at the next callback cycle.
     */
    if (event & AD_EVENT_INIT) {
        DEBUG("==> AD_EVENT_READ");
        // Allocate a counter container for this connection.
        struct my_cdata *cdata = (struct my_cdata *)calloc(1, sizeof(struct my_cdata));

        // Attach to this connection.
        ad_conn_set_userdata(conn, cdata, my_userdata_free_cb);
    }

    /*
     * AD_EVENT_READ event happens whenever data comes in.
     */
    else if (event & AD_EVENT_READ) {
        DEBUG("==> AD_EVENT_READ");
        if (ad_http_get_status(conn) == AD_HTTP_REQ_DONE) {
            // Get my per-connection data.
            struct my_cdata *cdata = (struct my_cdata *)ad_conn_get_userdata(conn);

            size_t size = 0;
            void *data = ad_http_get_content(conn, 0, &size);
            ad_http_response(conn, 200, "text/plain", data, size);

            cdata->counter++;
            if (cdata->counter > 3) {
                return AD_CLOSE;
            }
        }

        return AD_OK;
    }

    /*
     * AD_EVENT_WRITE event happens whenever out-buffer has lesser than certain
     * amount of data.
     *
     * Default watermark is 0 meaning this will happens when out-buffer is empty.
     * For reasonable size of message, you can send it all at once but for a large
     * amount of data, you need to send it out through out multiple callbacks.
     *
     * To maximize the performance, you will also want to set higher watermark
     * so whenever the level goes below the watermark you will be called for the
     * refill work before the buffer gets empty. So it's just faster if you can
     * fill up gas while you're driving without a stop.
     */
    else if (event & AD_EVENT_WRITE) {
        DEBUG("==> AD_EVENT_WRITE");
        // We've sent all the data in out-buffer.
    }

    /*
     * AD_EVENT_CLOSE event happens right before closing connection.
     * This will be the last callback for this connection.
     * So if you have created per-connection data then release the resource.
     */
    else if (event & AD_EVENT_CLOSE) {
        DEBUG("==> AD_EVENT_CLOSE=%x (TIMEOUT=%d, SHUTDOWN=%d)",
              event, event & AD_EVENT_TIMEOUT, event & AD_EVENT_SHUTDOWN);
        // You can release your user data explicitly here if you haven't
        // set the callback that release your user data.
    }

    // Return AD_OK will let the hook loop to continue.
    return AD_OK;
}

int main(int argc, char **argv) {
    // Example shared user data.
    char *userdata = "SHARED-USERDATA";

    // Create a server.
    ad_log_level(AD_LOG_DEBUG);
    ad_server_t *server = ad_server_new();

    // Set server options.
    ad_server_set_option(server, "server.port", "8888");
    ad_server_set_option(server, "server.addr", "0.0.0.0");
    ad_server_set_option(server, "server.timeout", "60");

    // Register protocol handler.
    ad_server_register_hook(server, ad_http_handler, NULL);

    // Register your hooks
    ad_server_register_hook(server, my_conn_handler, userdata);

    // SSL option.
    //ad_server_set_option(server, "server.enable_ssl", "1");
    //ad_server_set_option(server, "server.ssl_cert", "example.cert");
    //ad_server_set_option(server, "server.ssl_pkey", "example.pkey");

    // Start server.
    int retstatus = ad_server_start(server);

    // That is it!!!
    return retstatus;
}
