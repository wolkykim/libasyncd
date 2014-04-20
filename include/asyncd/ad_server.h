/******************************************************************************
 * LibAsyncd
 *
 * Copyright (c) 2014 Seungyoung Kim.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/

/**
 * ad_server header file
 *
 * @file ad_server.h
 */

#ifndef _AD_SERVER_H
#define _AD_SERVER_H

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <openssl/ssl.h>
#include "qlibc/qlibc.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*\
|                                 TYPEDEFS                                    |
\*---------------------------------------------------------------------------*/
<<<<<<< HEAD
typedef struct ad_server_s ad_server_t;
typedef struct ad_conn_s ad_conn_t;
typedef enum ad_cb_return_e ad_cb_return_t;

=======
>>>>>>> c5d6a4d5c26cc3b3b66aad069ff7453e455ba631
/**
 * These flags are used for ad_log_level();
 */
enum ad_log_e{
    AD_LOG_DISABLE = 0,
    AD_LOG_ERROR,
    AD_LOG_WARN,
    AD_LOG_INFO,
    AD_LOG_DEBUG,
    AD_LOG_DEBUG2,
};

/**
 * Return values of user callback.
 */
enum ad_cb_return_e {
    /*!< I'm done with this request. Escalate to other hooks. */
    AD_OK = 0,
    /*!< I'll handle the buffer directly this time, skip next hook */
    AD_TAKEOVER,
    /*!< We're done with this request but keep the connection open. */
    AD_DONE,
    /*!< We're done with this request. Close as soon as we sent all data out. */
    AD_CLOSE,
};

typedef struct ad_server_s ad_server_t;
typedef struct ad_conn_s ad_conn_t;
typedef enum ad_cb_return_e ad_cb_return_t;
typedef struct ad_hook_s ad_hook_t;

/*---------------------------------------------------------------------------*\
|                              SERVER OPTIONS                                 |
\*---------------------------------------------------------------------------*/

/**
 * Server option names and default values.
 */
#define AD_SERVER_OPTIONS {  \
        { "server.port",        "8888" },                                   \
                                                                            \
        /* Addr format IPv4="1.2.3.4", IPv6="1:2:3:4:5:6", Unix="/path" */  \
        { "server.addr",        "0.0.0.0" },                                \
                                                                            \
        { "server.backlog",     "128" },                                    \
                                                                            \
        /* Set read timeout seconds. 0 means no timeout. */                 \
        { "server.timeout",     "0" },                                      \
                                                                            \
        /* SSL options */                                                   \
        { "server.enable_ssl", "0" },                                       \
        { "server.ssl_cert", "/usr/local/etc/ad_server/ad_server.crt" },    \
        { "server.ssl_pkey", "/usr/local/etc/ad_server/ad_server.key" },    \
                                                                            \
        /* Enable or disable request pipelining, this change AD_DONE's behavior */ \
        { "server.request_pipelining", "1" },                               \
                                                                            \
        /* Run server in a separate thread */                               \
        { "server.thread", "0" },                                           \
                                                                            \
        /* Collect resources after stop */                                  \
        { "server.free_on_stop", "1" },                                     \
                                                                            \
        /* End of array marker. Do not remove */                            \
        { "", "_END_" }                                                     \
};

/*---------------------------------------------------------------------------*\
|                               USER-CALLBACK                                 |
\*---------------------------------------------------------------------------*/

/**
 * User callback(hook) prototype.
 */
typedef int (*ad_callback)(short event, ad_conn_t *conn, void *userdata);
typedef void (*ad_userdata_free_cb)(ad_conn_t *conn, void *userdata);

/**
 * Event types
 */
#define AD_EVENT_INIT     (1)        /*!< Call once upon new connection. */
#define AD_EVENT_READ     (1 << 1)   /*!< Call on read */
#define AD_EVENT_WRITE    (1 << 2)   /*!< Call on write. */
#define AD_EVENT_CLOSE    (1 << 3)   /*!< Call before closing. */
#define AD_EVENT_TIMEOUT  (1 << 4)   /*!< Timeout indicator, this flag will be set with AD_EVENT_CLOSE. */
#define AD_EVENT_SHUTDOWN (1 << 5)   /*!< Shutdown indicator, this flag will be set with AD_EVENT_CLOSE. */

/**
 * Defaults
 */
#define AD_NUM_USERDATA (2)  /*!< Number of userdata. Currently 0 is for userdata, 1 is for extra. */

/*---------------------------------------------------------------------------*\
|                            DATA STRUCTURES                                  |
\*---------------------------------------------------------------------------*/

/**
 * Server info container.
 */
struct ad_server_s {
    int errcode;            /*!< exit status. 0 for normal exit, non zero for error. */
    pthread_t *thread;      /*!< thread object. not null if server runs as a thread */

    qhashtbl_t *options;            /*!< server options */
    qhashtbl_t *stats;              /*!< internal statistics */
    qlist_t *hooks;                 /*!< list of registered hooks */
    struct evconnlistener *listener; /*!< listener */
    struct event_base *evbase;      /*!< event base */
    SSL_CTX *sslctx;                /*!< SSL connection support */

    struct bufferevent *notify_buffer; /*!< internal notification channel */
};

/**
 * Connection structure.
 */
struct ad_conn_s {
    ad_server_t *server;        /*!< reference pointer to server */
    struct bufferevent *buffer; /*!< reference pointer to buffer */
    struct evbuffer *in;        /*!< in buffer */
    struct evbuffer *out;       /*!< out buffer */
    int status;                 /*!< hook status such as AD_OK */

    void *userdata[2];             /*!< userdata[0] for end user, userdata[1] for extra */
    ad_userdata_free_cb userdata_free_cb[2];  /*!< callback to release user data */
    char *method;               /*!< request method. set by protocol handler */
};

/*----------------------------------------------------------------------------*\
|                             PUBLIC FUNCTIONS                                 |
\*----------------------------------------------------------------------------*/
enum ad_log_e ad_log_level(enum ad_log_e log_level);

extern ad_server_t *ad_server_new(void);
extern int ad_server_start(ad_server_t *server);
extern void ad_server_stop(ad_server_t *server);
extern void ad_server_free(ad_server_t *server);
extern void ad_server_global_free(void);

extern void ad_server_set_option(ad_server_t *server, const char *key, const char *value);
extern char *ad_server_get_option(ad_server_t *server, const char *key);
extern int ad_server_get_option_int(ad_server_t *server, const char *key);
extern SSL_CTX *ad_server_get_ssl_ctx(ad_server_t *server);
extern qhashtbl_t *ad_server_get_stats(ad_server_t *server, const char *key);

extern void ad_server_register_hook(ad_server_t *server, ad_callback cb, void *userdata);
extern void ad_server_register_hook_on_method(ad_server_t *server, const char *method,
                                              ad_callback cb, void *userdata);

extern void *ad_conn_set_userdata(ad_conn_t *conn, const void *userdata, ad_userdata_free_cb free_cb);
extern void *ad_conn_get_userdata(ad_conn_t *conn);
extern void *ad_conn_set_extra(ad_conn_t *conn, const void *extra, ad_userdata_free_cb free_cb);
extern void *ad_conn_get_extra(ad_conn_t *conn);
extern char *ad_conn_set_method(ad_conn_t *conn, char *method);

/*---------------------------------------------------------------------------*\
|                             INTERNAL USE ONLY                               |
\*---------------------------------------------------------------------------*/
#ifndef _DOXYGEN_SKIP
#endif /* _DOXYGEN_SKIP */

#ifdef __cplusplus
}
#endif

#endif /*_AD_SERVER_H */
