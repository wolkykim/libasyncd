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
 * ad_http header file
 *
 * @file ad_http.h
 */

#ifndef _AD_HTTP_H
#define _AD_HTTP_H

#include <event2/bufferevent.h>
#include "qlibc.h"
#include "ad_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/*----------------------------------------------------------------------------*\
|                                 TYPEDEFS                                     |
 \*----------------------------------------------------------------------------*/
typedef struct ad_http_s ad_http_t;

/*!< Hook type */
#define AD_HOOK_ALL               (0)         /*!< call on each and every phases */
#define AD_HOOK_ON_CONNECT        (1)         /*!< call right after the establishment of connection */
#define AD_HOOK_AFTER_REQUESTLINE (1 << 2)    /*!< call after parsing request line */
#define AD_HOOK_AFTER_HEADER      (1 << 3)    /*!< call after parsing all headers */
#define AD_HOOK_ON_BODY           (1 << 4)    /*!< call on every time body data received */
#define AD_HOOK_ON_REQUEST        (1 << 5)    /*!< call with complete request */
#define AD_HOOK_ON_CLOSE          (1 << 6)    /*!< call right before closing or next request */

/* HTTP PROTOCOL CODES */
#define HTTP_PROTOCOL_09    "HTTP/0.9"
#define HTTP_PROTOCOL_10    "HTTP/1.0"
#define HTTP_PROTOCOL_11    "HTTP/1.1"

#define DEFAULT_CONTENTTYPE "application/octet-stream"

/* Public Types */

/* Public Functions */
extern ad_http_t *ad_http_new(qhashtbl_t *options);
extern void ad_http_free(ad_http_t *http);
extern int ad_http_handler(ad_http_t *http, struct bufferevent *buffer, qlist_t *hooks);

enum ad_http_request_status_e {
    AD_HTTP_REQ_INIT = 0,        /*!< hasn't received the 1st byte yet */
    AD_HTTP_REQ_REQUESTLINE_DONE,/*!< hasn't received the 1st byte yet */
    AD_HTTP_REQ_HEADER_DONE,     /*!< received headers completely */
    AD_HTTP_REQ_READING_BODY,    /*!< receiving body */
    AD_HTTP_REQ_DONE,            /*!< received body completely. no more data expected */

    AD_HTTP_REQ_ERROR,           /*!< unrecoverable error found. */
};

enum ad_http_response_status_e {
    RES_INIT = 0,       /*!< hasn't sent out any data yet */
    RES_SENDING,        /*!< still sending data in out-buffer */
    RES_SENT,           /*!< out-buffer is empty, waiting to be filled in */
    RES_DONE,           /*!< out-buffer is closed. no more data is expected. */
};

struct ad_http_s {
    //
    // Status & Internal data
    //
    ad_server_t *server;        /*!< reference pointer to server */
    struct bufferevent *buffer; /*!< reference pointer to buffer */
    struct evbuffer *in;        /*!< in buffer */
    struct evbuffer *out;       /*!< out buffer */

    enum ad_http_request_status_e req_status;  /*!< request status. */
    enum ad_http_response_status_e res_status;  /*!< response status. */

    //
    // User Data
    //
    void *userdata;         /*!< user data */

    //
    // HTTP Request
    //

    // request line - available on REQ_REQUESTLINE_DONE.
    char *req_method;   /*!< request method ex) GET */
    char *req_uri;      /*!< url+query ex) /data%20path?query=the%20value */
    char *req_httpver;  /*!< version ex) HTTP/1.1 */
    char *req_path;     /*!< decoded path ex) /data path */
    char *req_query;    /*!< query string ex) query=the%20value */

    // request header - available on REQ_HEADER_DONE.
    qlisttbl_t *req_headers;  /*!< parsed request header entries */
    char *req_host;     /*!< host ex) www.domain.com or www.domain.com:8080 */
    char *req_domain;   /*!< domain name ex) www.domain.com (no port number) */
    size_t req_contentlength;  /*!< value of Content-Length header. -1 if not set */

    // request body
    char   *req_data;   /*!< request body */
    size_t req_datalen; /*!< bytes stored so far (different with content-length) */

    //
    // HTTP Response
    //

    // response headers
    int res_code;               /*!< response code */
    qlisttbl_t *res_headers;    /*!< response header entries */
};

#ifdef __cplusplus
}
#endif

#endif /*_AD_HTTP_H */
