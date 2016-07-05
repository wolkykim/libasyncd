/******************************************************************************
 * libasyncd
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
 * ad_http_handler header file
 *
 * @file ad_http_handler.h
 */

#ifndef _AD_HTTP_HANDLER_H
#define _AD_HTTP_HANDLER_H

#include "qlibc/qlibc.h"

#ifdef __cplusplus
extern "C" {
#endif

/*----------------------------------------------------------------------------*\
|                           HTTP PROTOCOL SPECIFICS                            |
\*----------------------------------------------------------------------------*/

/* HTTP PROTOCOL CODES */
#define HTTP_PROTOCOL_09    "HTTP/0.9"
#define HTTP_PROTOCOL_10    "HTTP/1.0"
#define HTTP_PROTOCOL_11    "HTTP/1.1"

/* HTTP RESPONSE CODES */
#define HTTP_NO_RESPONSE                (0)
#define HTTP_CODE_CONTINUE              (100)
#define HTTP_CODE_OK                    (200)
#define HTTP_CODE_CREATED               (201)
#define HTTP_CODE_NO_CONTENT            (204)
#define HTTP_CODE_PARTIAL_CONTENT       (206)
#define HTTP_CODE_MULTI_STATUS          (207)
#define HTTP_CODE_MOVED_TEMPORARILY     (302)
#define HTTP_CODE_NOT_MODIFIED          (304)
#define HTTP_CODE_BAD_REQUEST           (400)
#define HTTP_CODE_UNAUTHORIZED          (401)
#define HTTP_CODE_FORBIDDEN             (403)
#define HTTP_CODE_NOT_FOUND             (404)
#define HTTP_CODE_METHOD_NOT_ALLOWED    (405)
#define HTTP_CODE_REQUEST_TIME_OUT      (408)
#define HTTP_CODE_GONE                  (410)
#define HTTP_CODE_REQUEST_URI_TOO_LONG  (414)
#define HTTP_CODE_LOCKED                (423)
#define HTTP_CODE_INTERNAL_SERVER_ERROR (500)
#define HTTP_CODE_NOT_IMPLEMENTED       (501)
#define HTTP_CODE_SERVICE_UNAVAILABLE   (503)

/* DEFAULT BEHAVIORS */
#define HTTP_CRLF "\r\n"
#define HTTP_DEF_CONTENTTYPE "application/octet-stream"

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

enum ad_http_request_status_e {
    AD_HTTP_REQ_INIT = 0,        /*!< initial state */
    AD_HTTP_REQ_REQUESTLINE_DONE,/*!< received 1st line */
    AD_HTTP_REQ_HEADER_DONE,     /*!< received headers completely */
    AD_HTTP_REQ_DONE,            /*!< received body completely. no more data expected */

    AD_HTTP_ERROR,               /*!< unrecoverable error found. */
};

/*----------------------------------------------------------------------------*\
|                             PUBLIC FUNCTIONS                                 |
\*----------------------------------------------------------------------------*/
extern int ad_http_handler(short event, ad_conn_t *conn, void *userdata);

extern enum ad_http_request_status_e ad_http_get_status(ad_conn_t *conn);
extern struct evbuffer *ad_http_get_inbuf(ad_conn_t *conn);
extern struct evbuffer *ad_http_get_outbuf(ad_conn_t *conn);

extern const char *ad_http_get_request_header(ad_conn_t *conn, const char *name);
extern off_t ad_http_get_content_length(ad_conn_t *conn);
extern size_t ad_http_get_content_length_stored(ad_conn_t *conn);
extern void *ad_http_get_content(ad_conn_t *conn, size_t maxsize, size_t *storedsize);
extern int ad_http_is_keepalive_request(ad_conn_t *conn);

extern int ad_http_set_response_header(ad_conn_t *conn, const char *name, const char *value);
extern const char *ad_http_get_response_header(ad_conn_t *conn, const char *name);
extern int ad_http_set_response_code(ad_conn_t *conn, int code, const char *reason);
extern int ad_http_set_response_content(ad_conn_t *conn, const char *contenttype, off_t size);

extern size_t ad_http_response(ad_conn_t *conn, int code, const char *contenttype, const void *data, off_t size);
extern size_t ad_http_send_header(ad_conn_t *conn);
extern size_t ad_http_send_data(ad_conn_t *conn, const void *data, size_t size);
extern size_t ad_http_send_chunk(ad_conn_t *conn, const void *data, size_t size);

extern const char *ad_http_get_reason(int code);

/*---------------------------------------------------------------------------*\
|                            DATA STRUCTURES                                  |
\*---------------------------------------------------------------------------*/
struct ad_http_s {
    // HTTP Request
    struct {
        enum ad_http_request_status_e status;  /*!< request status. */
        struct evbuffer *inbuf;  /*!< input data buffer. */

        // request line - available on REQ_REQUESTLINE_DONE.
        char *method;   /*!< request method ex) GET */
        char *uri;      /*!< url+query ex) /data%20path?query=the%20value */
        char *httpver;  /*!< version ex) HTTP/1.1 */
        char *path;     /*!< decoded path ex) /data path */
        char *query;    /*!< query string ex) query=the%20value */

        // request header - available on REQ_HEADER_DONE.
        qlisttbl_t *headers;  /*!< parsed request header entries */
        char *host;           /*!< host ex) www.domain.com or www.domain.com:8080 */
        char *domain;         /*!< domain name ex) www.domain.com (no port number) */
        off_t contentlength;  /*!< value of Content-Length header.*/
        size_t bodyin;        /*!< bytes moved to in-buff */
    } request;

    // HTTP Response
    struct {
        struct evbuffer *outbuf;  /*!< output data buffer. */
        bool frozen_header;       /*!< indicator whether we sent header out or not */

        // response headers
        int code;               /*!< response status-code */
        char *reason;           /*!< reason-phrase */
        qlisttbl_t *headers;    /*!< response header entries */
        off_t contentlength;    /*!< content length in response */
        size_t bodyout;         /*!< bytes added to out-buffer */
    } response;
};

#ifdef __cplusplus
}
#endif

#endif /*_AD_HTTP_HANDLER_H */
