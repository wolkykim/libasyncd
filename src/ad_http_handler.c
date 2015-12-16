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
 * HTTP protocol request/response handler.
 *
 * @file ad_http_handler.c
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <limits.h>
#include <assert.h>
#include <errno.h>
#include <event2/buffer.h>
#include "qlibc/qlibc.h"
#include "ad_server.h"
#include "ad_http_handler.h"
#include "macro.h"

#ifndef _DOXYGEN_SKIP
static ad_http_t *http_new(struct evbuffer *out);
static void http_free(ad_http_t *http);
static void http_free_cb(ad_conn_t *conn, void *userdata);
static size_t http_add_inbuf(struct evbuffer *buffer, ad_http_t *http,
                             size_t maxsize);

static int http_parser(ad_http_t *http, struct evbuffer *in);
static int parse_requestline(ad_http_t *http, char *line);
static int parse_headers(ad_http_t *http, struct evbuffer *in);
static int parse_body(ad_http_t *http, struct evbuffer *in);
static ssize_t parse_chunked_body(ad_http_t *http, struct evbuffer *in);

static bool isValidPathname(const char *path);
static void correctPathname(char *path);
static char *evbuffer_peekln(struct evbuffer *buffer, size_t *n_read_out,
                             enum evbuffer_eol_style eol_style);
static ssize_t evbuffer_drainln(struct evbuffer *buffer, size_t *n_read_out,
                                enum evbuffer_eol_style eol_style);

#endif

/**
 * HTTP protocol handler hook.
 *
 * This hook provides an easy way to handle HTTP request/response.
 *
 * @note
 *   This hook must be registered at the top of hook chain.
 *
 * @code
 *   ad_server_t *server = ad_server_new();
 *   ad_server_register_hook(server, ad_http_handler, NULL);
 * @endcode
 */
int ad_http_handler(short event, ad_conn_t *conn, void *userdata) {
    if (event & AD_EVENT_INIT) {
        DEBUG("==> HTTP INIT");
        ad_http_t *http = http_new(conn->out);
        if (http == NULL)
            return AD_CLOSE;
        ad_conn_set_extra(conn, http, http_free_cb);
        return AD_OK;
    } else if (event & AD_EVENT_READ) {
        DEBUG("==> HTTP READ");
        ad_http_t *http = (ad_http_t *) ad_conn_get_extra(conn);
        int status = http_parser(http, conn->in);
        if (conn->method == NULL && http->request.method != NULL) {
            ad_conn_set_method(conn, http->request.method);
        }
        return status;
    } else if (event & AD_EVENT_WRITE) {
        DEBUG("==> HTTP WRITE");
        return AD_OK;
    } else if (event & AD_EVENT_CLOSE) {
        DEBUG("==> HTTP CLOSE=%x (TIMEOUT=%d, SHUTDOWN=%d)",
                event, event & AD_EVENT_TIMEOUT, event & AD_EVENT_SHUTDOWN);
        return AD_OK;
    }

    BUG_EXIT();
    return AD_CLOSE;
}

/**
 * Return the request status.
 */
enum ad_http_request_status_e ad_http_get_status(ad_conn_t *conn) {
    ad_http_t *http = (ad_http_t *) ad_conn_get_extra(conn);
    if (http == NULL)
        return AD_HTTP_ERROR;
    return http->request.status;
}

struct evbuffer *ad_http_get_inbuf(ad_conn_t *conn) {
    ad_http_t *http = (ad_http_t *) ad_conn_get_extra(conn);
    return http->request.inbuf;
}

struct evbuffer *ad_http_get_outbuf(ad_conn_t *conn) {
    ad_http_t *http = (ad_http_t *) ad_conn_get_extra(conn);
    return http->response.outbuf;
}

/**
 * Get request header.
 *
 * @param name name of header.
 *
 * @return value of string if found, otherwise NULL.
 */
const char *ad_http_get_request_header(ad_conn_t *conn, const char *name) {
    ad_http_t *http = (ad_http_t *) ad_conn_get_extra(conn);
    return http->request.headers->getstr(http->request.headers, name, false);
}

/**
 * Return the size of content from the request.
 */
off_t ad_http_get_content_length(ad_conn_t *conn) {
    ad_http_t *http = (ad_http_t *) ad_conn_get_extra(conn);
    return http->request.contentlength;
}

/**
 * Remove content from the in-buffer.
 *
 * @param maxsize maximum length of data to pull up. 0 to pull up everything.
 */
void *ad_http_get_content(ad_conn_t *conn, size_t maxsize, size_t *storedsize) {
    ad_http_t *http = (ad_http_t *) ad_conn_get_extra(conn);

    size_t inbuflen = evbuffer_get_length(http->request.inbuf);
    size_t readlen =
            (maxsize == 0) ?
                    inbuflen : ((inbuflen < maxsize) ? inbuflen : maxsize);
    if (readlen == 0)
        return NULL;

    void *data = malloc(readlen);
    if (data == NULL)
        return NULL;

    size_t removedlen = evbuffer_remove(http->request.inbuf, data, readlen);
    if (storedsize)
        *storedsize = removedlen;

    return data;
}

/**
 * Return whether the request is keep-alive request or not.
 *
 * @return 1 if keep-alive request, otherwise 0.
 */
int ad_http_is_keepalive_request(ad_conn_t *conn) {
    ad_http_t *http = (ad_http_t *) ad_conn_get_extra(conn);
    if (http->request.httpver == NULL) {
        return 0;
    }

    const char *connection = ad_http_get_request_header(conn, "Connection");
    if (!strcmp(http->request.httpver, HTTP_PROTOCOL_11)) {
        // In HTTP/1.1, Keep-Alive is on by default unless explicitly specified.
        if (connection != NULL && !strcmp(connection, "close")) {
            return 0;
        }
        return 1;
    } else {
        // In older version, Keep-Alive is off by default unless requested.
        if (connection != NULL
                && (!strcmp(connection, "Keep-Alive")
                        || !strcmp(connection, "TE"))) {
            return 1;
        }
        return 0;
    }
}

/**
 * Set response header.
 *
 * @param name name of header.
 * @param value value string to set. NULL to remove the header.
 *
 * @return 0 on success, -1 if we already sent it out.
 */
int ad_http_set_response_header(ad_conn_t *conn, const char *name,
                                const char *value) {
    ad_http_t *http = (ad_http_t *) ad_conn_get_extra(conn);
    if (http->response.frozen_header) {
        return -1;
    }

    if (value != NULL) {
        http->response.headers->putstr(http->response.headers, name, value);
    } else {
        http->response.headers->remove(http->response.headers, name);
    }

    return 0;
}

/**
 * Get response header.
 *
 * @param name name of header.
 *
 * @return value of string if found, otherwise NULL.
 */
const char *ad_http_get_response_header(ad_conn_t *conn, const char *name) {
    ad_http_t *http = (ad_http_t *) ad_conn_get_extra(conn);
    return http->response.headers->getstr(http->response.headers, name, false);
}

/**
 *
 * @return 0 on success, -1 if we already sent it out.
 */
int ad_http_set_response_code(ad_conn_t *conn, int code, const char *reason) {
    ad_http_t *http = (ad_http_t *) ad_conn_get_extra(conn);
    if (http->response.frozen_header) {
        return -1;
    }

    http->response.code = code;
    if (reason)
        http->response.reason = strdup(reason);

    return 0;
}

/**
 *
 * @param size content size. -1 for chunked transfer encoding.
 * @return 0 on success, -1 if we already sent it out.
 */
int ad_http_set_response_content(ad_conn_t *conn, const char *contenttype,
                                 off_t size) {
    ad_http_t *http = (ad_http_t *) ad_conn_get_extra(conn);
    if (http->response.frozen_header) {
        return -1;
    }

    // Set Content-Type header.
    ad_http_set_response_header(
            conn, "Content-Type",
            (contenttype) ? contenttype : HTTP_DEF_CONTENTTYPE);
    if (size >= 0) {
        char clenval[20 + 1];
        sprintf(clenval, "%jd", size);
        ad_http_set_response_header(conn, "Content-Length", clenval);
        http->response.contentlength = size;
    } else {
        ad_http_set_response_header(conn, "Transfer-Encoding", "chunked");
        http->response.contentlength = -1;
    }

    return 0;
}

/**
 * @return total bytes sent, 0 on error.
 */
size_t ad_http_response(ad_conn_t *conn, int code, const char *contenttype,
                        const void *data, off_t size) {
    ad_http_t *http = (ad_http_t *) ad_conn_get_extra(conn);
    if (http->response.frozen_header) {
        return 0;
    }

    // Set response headers.
    if (ad_http_get_response_header(conn, "Connection") == NULL) {
        ad_http_set_response_header(
                conn, "Connection",
                (ad_http_is_keepalive_request(conn)) ? "Keep-Alive" : "close");
    }

    ad_http_set_response_code(conn, code, ad_http_get_reason(code));
    ad_http_set_response_content(conn, contenttype, size);
    return ad_http_send_data(conn, data, size);
}

/**
 *
 * @return 0 total bytes put in out buffer, -1 if we already sent it out.
 */
size_t ad_http_send_header(ad_conn_t *conn) {
    ad_http_t *http = (ad_http_t *) ad_conn_get_extra(conn);
    if (http->response.frozen_header) {
        return 0;
    }
    http->response.frozen_header = true;

    // Send status line.
    const char *reason =
            (http->response.reason) ?
                    http->response.reason :
                    ad_http_get_reason(http->response.code);
    evbuffer_add_printf(http->response.outbuf, "%s %d %s" HTTP_CRLF,
                        http->request.httpver, http->response.code, reason);

    // Send headers.
    qlisttbl_obj_t obj;
    bzero((void*) &obj, sizeof(obj));
    qlisttbl_t *tbl = http->response.headers;
    tbl->lock(tbl);
    while (tbl->getnext(tbl, &obj, NULL, false)) {
        evbuffer_add_printf(http->response.outbuf, "%s: %s" HTTP_CRLF,
                            (char*) obj.name, (char*) obj.data);
    }
    tbl->unlock(tbl);

    // Send empty line, indicator of end of header.
    evbuffer_add(http->response.outbuf, HTTP_CRLF, CONST_STRLEN(HTTP_CRLF));

    return evbuffer_get_length(http->response.outbuf);
}

/**
 *
 * @return 0 on success, -1 if we already sent it out.
 */
size_t ad_http_send_data(ad_conn_t *conn, const void *data, size_t size) {
    ad_http_t *http = (ad_http_t *) ad_conn_get_extra(conn);

    if (http->response.contentlength < 0) {
        WARN("Content-Length is not set. Invalid usage.");
        return 0;
    }

    if ((http->response.bodyout + size) > http->response.contentlength) {
        WARN("Trying to send more data than supposed to");
        return 0;
    }

    size_t beforesize = evbuffer_get_length(http->response.outbuf);
    if (!http->response.frozen_header) {
        ad_http_send_header(conn);
    }

    if (data != NULL && size > 0) {
        if (evbuffer_add(http->response.outbuf, data, size))
            return 0;
    }

    http->response.bodyout += size;
    return (evbuffer_get_length(http->response.outbuf) - beforesize);
}

size_t ad_http_send_chunk(ad_conn_t *conn, const void *data, size_t size) {
    ad_http_t *http = (ad_http_t *) ad_conn_get_extra(conn);

    if (http->response.contentlength >= 0) {
        WARN("Content-Length is set. Invalid usage.");
        return 0;
    }

    if (!http->response.frozen_header) {
        ad_http_send_header(conn);
    }

    size_t beforesize = evbuffer_get_length(http->response.outbuf);
    int status = 0;
    if (size > 0) {
        status += evbuffer_add_printf(http->response.outbuf, "%zu" HTTP_CRLF,
                                      size);
        status += evbuffer_add(http->response.outbuf, data, size);
        status += evbuffer_add(http->response.outbuf, HTTP_CRLF,
                               CONST_STRLEN(HTTP_CRLF));
    } else {
        status += evbuffer_add_printf(http->response.outbuf,
                                      "0" HTTP_CRLF HTTP_CRLF);
    }
    if (status != 0) {
        WARN("Failed to add data to out-buffer. (size:%jd)", size);
        return 0;
    }

    size_t bytesout = evbuffer_get_length(http->response.outbuf) - beforesize;
    http->response.bodyout += bytesout;
    return bytesout;
}

const char *ad_http_get_reason(int code) {
    switch (code) {
        case HTTP_CODE_CONTINUE:
            return "Continue";
        case HTTP_CODE_OK:
            return "OK";
        case HTTP_CODE_CREATED:
            return "Created";
        case HTTP_CODE_NO_CONTENT:
            return "No content";
        case HTTP_CODE_PARTIAL_CONTENT:
            return "Partial Content";
        case HTTP_CODE_MULTI_STATUS:
            return "Multi Status";
        case HTTP_CODE_MOVED_TEMPORARILY:
            return "Moved Temporarily";
        case HTTP_CODE_NOT_MODIFIED:
            return "Not Modified";
        case HTTP_CODE_BAD_REQUEST:
            return "Bad Request";
        case HTTP_CODE_UNAUTHORIZED:
            return "Authorization Required";
        case HTTP_CODE_FORBIDDEN:
            return "Forbidden";
        case HTTP_CODE_NOT_FOUND:
            return "Not Found";
        case HTTP_CODE_METHOD_NOT_ALLOWED:
            return "Method Not Allowed";
        case HTTP_CODE_REQUEST_TIME_OUT:
            return "Request Time Out";
        case HTTP_CODE_GONE:
            return "Gone";
        case HTTP_CODE_REQUEST_URI_TOO_LONG:
            return "Request URI Too Long";
        case HTTP_CODE_LOCKED:
            return "Locked";
        case HTTP_CODE_INTERNAL_SERVER_ERROR:
            return "Internal Server Error";
        case HTTP_CODE_NOT_IMPLEMENTED:
            return "Not Implemented";
        case HTTP_CODE_SERVICE_UNAVAILABLE:
            return "Service Unavailable";
    }

    WARN("Undefined code found. %d", code);
    return "-";
}

/******************************************************************************
 * Private internal functions.
 *****************************************************************************/
#ifndef _DOXYGEN_SKIP

static ad_http_t *http_new(struct evbuffer *out) {
    // Create a new connection container.
    ad_http_t *http = NEW_OBJECT(ad_http_t);
    if (http == NULL)
        return NULL;

    // Allocate additional resources.
    http->request.inbuf = evbuffer_new();
    http->request.headers = qlisttbl(
            QLISTTBL_UNIQUE | QLISTTBL_CASEINSENSITIVE);
    http->response.headers = qlisttbl(
            QLISTTBL_UNIQUE | QLISTTBL_CASEINSENSITIVE);
    if (http->request.inbuf == NULL || http->request.headers == NULL
            || http->response.headers == NULL) {
        http_free(http);
        return NULL;
    }

    // Initialize structure.
    http->request.status = AD_HTTP_REQ_INIT;
    http->request.contentlength = -1;
    http->response.contentlength = -1;
    http->response.outbuf = out;

    return http;
}

static void http_free(ad_http_t *http) {
    if (http) {
        if (http->request.inbuf)
            evbuffer_free(http->request.inbuf);
        if (http->request.method)
            free(http->request.method);
        if (http->request.uri)
            free(http->request.uri);
        if (http->request.httpver)
            free(http->request.httpver);
        if (http->request.path)
            free(http->request.path);
        if (http->request.query)
            free(http->request.query);

        if (http->request.headers)
            http->request.headers->free(http->request.headers);
        if (http->request.host)
            free(http->request.host);
        if (http->request.domain)
            free(http->request.domain);

        if (http->response.headers)
            http->response.headers->free(http->response.headers);
        if (http->response.reason)
            free(http->response.reason);

        free(http);
    }
}

static void http_free_cb(ad_conn_t *conn, void *userdata) {
    http_free((ad_http_t *) userdata);
}

static size_t http_add_inbuf(struct evbuffer *buffer, ad_http_t *http,
                             size_t maxsize) {
    if (maxsize == 0 || evbuffer_get_length(buffer) == 0) {
        return 0;
    }

    return evbuffer_remove_buffer(buffer, http->request.inbuf, maxsize);
}

static int http_parser(ad_http_t *http, struct evbuffer *in) {
    ASSERT(http != NULL && in != NULL);

    if (http->request.status == AD_HTTP_REQ_INIT) {
        char *line = evbuffer_readln(in, NULL, EVBUFFER_EOL_CRLF);
        if (line == NULL)
            return http->request.status;
        http->request.status = parse_requestline(http, line);
        free(line);
        // Do not call user callbacks until I reach the next state.
        if (http->request.status == AD_HTTP_REQ_INIT) {
            return AD_TAKEOVER;
        }
    }

    if (http->request.status == AD_HTTP_REQ_REQUESTLINE_DONE) {
        http->request.status = parse_headers(http, in);
        // Do not call user callbacks until I reach the next state.
        if (http->request.status == AD_HTTP_REQ_REQUESTLINE_DONE) {
            return AD_TAKEOVER;
        }
    }

    if (http->request.status == AD_HTTP_REQ_HEADER_DONE) {
        http->request.status = parse_body(http, in);
        // Do not call user callbacks until I reach the next state.
        if (http->request.status == AD_HTTP_REQ_HEADER_DONE) {
            return AD_TAKEOVER;
        }
    }

    if (http->request.status == AD_HTTP_REQ_DONE) {
        return AD_OK;
    }

    if (http->request.status == AD_HTTP_ERROR) {
        return AD_CLOSE;
    }

    BUG_EXIT();
    return AD_CLOSE;
}

static int parse_requestline(ad_http_t *http, char *line) {
    // Parse request line.
    char *saveptr;
    char *method = strtok_r(line, " ", &saveptr);
    char *uri = strtok_r(NULL, " ", &saveptr);
    char *httpver = strtok_r(NULL, " ", &saveptr);
    char *tmp = strtok_r(NULL, " ", &saveptr);

    if (method == NULL || uri == NULL || httpver == NULL || tmp != NULL) {
        DEBUG("Invalid request line. %s", line);
        return AD_HTTP_ERROR;
    }

    // Set request method
    http->request.method = qstrupper(strdup(method));

    // Set HTTP version
    http->request.httpver = qstrupper(strdup(httpver));
    if (strcmp(http->request.httpver, HTTP_PROTOCOL_09)
            && strcmp(http->request.httpver, HTTP_PROTOCOL_10)
            && strcmp(http->request.httpver, HTTP_PROTOCOL_11)) {
        DEBUG("Unknown protocol: %s", http->request.httpver);
        return AD_HTTP_ERROR;
    }

    // Set URI
    if (uri[0] == '/') {
        http->request.uri = strdup(uri);
    } else if ((tmp = strstr(uri, "://"))) {
        // divide URI into host and path
        char *path = strstr(tmp + CONST_STRLEN("://"), "/");
        if (path == NULL) {  // URI has no path ex) http://domain.com:80
            http->request.headers->putstr(http->request.headers, "Host",
                                          tmp + CONST_STRLEN("://"));
            http->request.uri = strdup("/");
        } else {  // URI has path, ex) http://domain.com:80/path
            *path = '\0';
            http->request.headers->putstr(http->request.headers, "Host",
                                          tmp + CONST_STRLEN("://"));
            *path = '/';
            http->request.uri = strdup(path);
        }
    } else {
        DEBUG("Invalid URI format. %s", uri);
        return AD_HTTP_ERROR;
    }

    // Set request path. Only path part from URI.
    http->request.path = strdup(http->request.uri);
    tmp = strstr(http->request.path, "?");
    if (tmp) {
        *tmp = '\0';
        http->request.query = strdup(tmp + 1);
    } else {
        http->request.query = strdup("");
    }
    qurl_decode(http->request.path);

    // check path
    if (isValidPathname(http->request.path) == false) {
        DEBUG("Invalid URI format : %s", http->request.uri);
        return AD_HTTP_ERROR;
    }
    correctPathname(http->request.path);

    DEBUG("Method=%s, URI=%s, VER=%s", http->request.method, http->request.uri, http->request.httpver);

    return AD_HTTP_REQ_REQUESTLINE_DONE;
}

static int parse_headers(ad_http_t *http, struct evbuffer *in) {
    char *line;
    while ((line = evbuffer_readln(in, NULL, EVBUFFER_EOL_CRLF))) {
        if (IS_EMPTY_STR(line)) {
            const char *clen = http->request.headers->getstr(
                    http->request.headers, "Content-Length", false);
            http->request.contentlength = (clen) ? atol(clen) : -1;
            free(line);
            return AD_HTTP_REQ_HEADER_DONE;
        }
        // Parse
        char *name, *value;
        char *tmp = strstr(line, ":");
        if (tmp) {
            *tmp = '\0';
            name = qstrtrim(line);
            value = qstrtrim(tmp + 1);
        } else {
            name = qstrtrim(line);
            value = "";
        }
        // Add
        http->request.headers->putstr(http->request.headers, name, value);

        free(line);
    }

    return http->request.status;
}

static int parse_body(ad_http_t *http, struct evbuffer *in) {
    // Handle static data case.
    if (http->request.contentlength == 0) {
        return AD_HTTP_REQ_DONE;
    } else if (http->request.contentlength > 0) {
        if (http->request.contentlength > http->request.bodyin) {
            size_t maxread = http->request.contentlength - http->request.bodyin;
            if (maxread > 0 && evbuffer_get_length(in) > 0) {
                http->request.bodyin += http_add_inbuf(in, http, maxread);
            }
        }
        if (http->request.contentlength == http->request.bodyin) {
            return AD_HTTP_REQ_DONE;
        }
    } else {
        // Check if Transfer-Encoding is chunked.
        const char *tranenc = http->request.headers->getstr(
                http->request.headers, "Transfer-Encoding", false);
        if (tranenc != NULL && !strcmp(tranenc, "chunked")) {
            // TODO: handle chunked encoding
            for (;;) {
                ssize_t chunksize = parse_chunked_body(http, in);
                if (chunksize > 0) {
                    continue;
                } else if (chunksize == 0) {
                    return AD_HTTP_REQ_DONE;
                } else if (chunksize == -1) {
                    return http->request.status;
                } else {
                    return AD_HTTP_ERROR;
                }
            }
        } else {
            return AD_HTTP_REQ_DONE;
        }
    }

    return http->request.status;
}

/**
 * Parse chunked body and append it to inbuf.
 *
 * @return number of bytes in a chunk. so 0 for the ending chunk. -1 for not enough data, -2 format error.
 */
static ssize_t parse_chunked_body(ad_http_t *http, struct evbuffer *in) {
    // Peek chunk size.
    size_t crlf_len = 0;
    char *line = evbuffer_peekln(in, &crlf_len, EVBUFFER_EOL_CRLF);
    if (line == NULL)
        return -1;  // not enough data.
    size_t linelen = strlen(line);

    // Parse chunk size
    int chunksize = -1;
    sscanf(line, "%x", &chunksize);
    free(line);
    if (chunksize < 0)
        return -2;  // format error

    // Check if we've received whole data of this chunk.
    size_t datalen = linelen + crlf_len + chunksize + crlf_len;
    size_t inbuflen = evbuffer_get_length(in);
    if (inbuflen < datalen) {
        return -1;  // not enough data.
    }

    // Copy chunk body
    evbuffer_drainln(in, NULL, EVBUFFER_EOL_CRLF);
    http_add_inbuf(in, http, chunksize);
    evbuffer_drainln(in, NULL, EVBUFFER_EOL_CRLF);

    return chunksize;
}

/**
 * validate file path
 */
static bool isValidPathname(const char *path) {
    if (path == NULL)
        return false;

    int len = strlen(path);
    if (len == 0 || len >= PATH_MAX)
        return false;
    else if (path[0] != '/')
        return false;
    else if (strpbrk(path, "\\:*?\"<>|") != NULL)
        return false;

    // check folder name length
    int n;
    char *t;
    for (n = 0, t = (char *) path; *t != '\0'; t++) {
        if (*t == '/') {
            n = 0;
            continue;
        }
        if (n >= FILENAME_MAX) {
            DEBUG("Filename too long.");
            return false;
        }
        n++;
    }

    return true;
}

/**
 * Correct pathname.
 *
 * @note
 *    remove :  heading & tailing white spaces, double slashes, tailing slash
 */
static void correctPathname(char *path) {
    // Take care of head & tail white spaces.
    qstrtrim(path);

    // Take care of double slashes.
    while (strstr(path, "//") != NULL)
        qstrreplace("sr", path, "//", "/");

    // Take care of tailing slash.
    int len = strlen(path);
    if (len <= 1)
        return;
    if (path[len - 1] == '/')
        path[len - 1] = '\0';
}

static char *evbuffer_peekln(struct evbuffer *buffer, size_t *n_read_out,
                             enum evbuffer_eol_style eol_style) {
    // Check if first line has arrived.
    struct evbuffer_ptr ptr = evbuffer_search_eol(buffer, NULL, n_read_out,
                                                  eol_style);
    if (ptr.pos == -1)
        return NULL;

    char *line = (char *) malloc(ptr.pos + 1);
    if (line == NULL)
        return NULL;

    // Linearizes buffer
    if (ptr.pos > 0) {
        char *bufferptr = (char *) evbuffer_pullup(buffer, ptr.pos);
        ASSERT(bufferptr != NULL);
        strncpy(line, bufferptr, ptr.pos);
    }
    line[ptr.pos] = '\0';

    return line;
}

static ssize_t evbuffer_drainln(struct evbuffer *buffer, size_t *n_read_out,
                                enum evbuffer_eol_style eol_style) {
    char *line = evbuffer_readln(buffer, n_read_out, eol_style);
    if (line == NULL)
        return -1;

    size_t linelen = strlen(line);
    free(line);
    return linelen;
}

#endif // _DOXYGEN_SKIP
