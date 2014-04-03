#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <limits.h>
#include <assert.h>
#include <errno.h>
#include <event2/buffer.h>
#include "macro.h"
#include "qlibc.h"
#include "ad_server.h"
#include "ad_http_handler.h"

#ifndef _DOXYGEN_SKIP
static ad_http_t *http_new(void);
static void http_free(ad_http_t *http);
static void http_free_cb(ad_conn_t *conn, void *userdata);

static int http_parser(ad_http_t *http, struct evbuffer *in);
static int parse_requestline(ad_http_t *http, char *line);
static int parse_headers(ad_http_t *http, struct evbuffer *in);
static bool isValidPathname(const char *path);
static void correctPathname(char *path);
#endif

int ad_http_handler(short event, ad_conn_t *conn, void *userdata) {
    if (event & AD_EVENT_INIT) {
        DEBUG("==> HTTP INIT");
        ad_http_t *http = http_new();
        if (http == NULL) return AD_CLOSE;
        ad_conn_set_extra(conn, http, http_free_cb);
        return AD_OK;
    } else if (event & AD_EVENT_READ) {
        DEBUG("==> HTTP READ");
        ad_http_t *http = (ad_http_t *)ad_conn_get_extra(conn);
        return http_parser(http, conn->in);
    } else if (event & AD_EVENT_WRITE) {
        DEBUG("==> HTTP WRITE");
        return AD_OK;
    } else if (event & AD_EVENT_CLOSE) {
        DEBUG("==> HTTP CLOSE=%x (TIMEOUT=%d, SHUTDOWN=%d)",
              event, event & AD_EVENT_TIMEOUT, event & AD_EVENT_SHUTDOWN);
        return AD_OK;
    }

    BUG_EXIT();
}

static ad_http_t *http_new(void) {
    // Create a new connection container.
    ad_http_t *http = NEW_STRUCT(ad_http_t);
    if (http == NULL) return NULL;

    // Allocate additional resources.
    http->request.headers = qlisttbl(QLISTTBL_UNIQUE | QLISTTBL_CASEINSENSITIVE);
    http->response.headers = qlisttbl(QLISTTBL_UNIQUE | QLISTTBL_CASEINSENSITIVE);
    if(http->request.headers == NULL || http->response.headers == NULL) {
        http_free(http);
        return NULL;
    }

    // Initialize structure.
    http->request.status = AD_HTTP_REQ_INIT;
    http->request.contentlength = -1;
    http->response.status = AD_HTTP_RES_INIT;

    return http;
}

static void http_free(ad_http_t *http) {
    if (http) {
        if (http->request.headers) http->request.headers->free(http->request.headers);
        if (http->response.headers) http->response.headers->free(http->response.headers);
        free(http);
    }
}

static void http_free_cb(ad_conn_t *conn, void *userdata) {
    http_free((ad_http_t *)userdata);
}

static int http_parser(ad_http_t *http, struct evbuffer *in) {
    ASSERT(http != NULL);

    if (http->request.status == AD_HTTP_REQ_INIT) {
        char *line = evbuffer_readln(in, NULL, EVBUFFER_EOL_CRLF);
        if (line == NULL) return http->request.status;
        http->request.status = parse_requestline(http, line);
        free(line);
        if (http->request.status == AD_HTTP_REQ_INIT) {
            return AD_TAKEOVER;
        }
    }

    if (http->request.status == AD_HTTP_REQ_REQUESTLINE_DONE) {
        http->request.status = parse_headers(http, in);
        if (http->request.status == AD_HTTP_REQ_REQUESTLINE_DONE) {
            return AD_TAKEOVER;
        }
    }

    if (http->request.status == AD_HTTP_REQ_HEADER_DONE) {
        return AD_OK;
    }

    if (http->request.status == AD_HTTP_REQ_ERROR) {
        return AD_CLOSE;
    }

    return AD_OK;
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
        return AD_HTTP_REQ_ERROR;
    }

    // Set request method
    http->request.method = qstrupper(strdup(method));

    // Set HTTP version
    http->request.httpver = qstrupper(strdup(httpver));
    if (strcmp(http->request.httpver, HTTP_PROTOCOL_09)
        && strcmp(http->request.httpver, HTTP_PROTOCOL_10)
        && strcmp(http->request.httpver, HTTP_PROTOCOL_11)
       ) {
        DEBUG("Unknown protocol: %s", http->request.httpver);
        return AD_HTTP_REQ_ERROR;
    }

    // Set URI
    if (uri[0] == '/') {
        http->request.uri = strdup(uri);
    } else if ((tmp = strstr(uri, "://"))) {
        // divide URI into host and path
        char *path = strstr(tmp + CONST_STRLEN("://"), "/");
        if (path == NULL) {  // URI has no path ex) http://domain.com:80
            http->request.headers->putstr(http->request.headers, "Host", tmp  + CONST_STRLEN("://"));
            http->request.uri = strdup("/");
        } else {  // URI has path, ex) http://domain.com:80/path
            *path = '\0';
            http->request.headers->putstr(http->request.headers, "Host", tmp  + CONST_STRLEN("://"));
            *path = '/';
            http->request.uri = strdup(path);
        }
    } else {
        DEBUG("Invalid URI format. %s", uri);
        return AD_HTTP_REQ_ERROR;
    }

    // Set request path. Only path part from URI.
    http->request.path = strdup(http->request.uri);
    tmp = strstr(http->request.path, "?");
    if (tmp) {
        *tmp ='\0';
        http->request.query = strdup(tmp + 1);
    } else {
        http->request.query = strdup("");
    }
    qurl_decode(http->request.path);

    // check path
    if (isValidPathname(http->request.path) == false) {
        DEBUG("Invalid URI format : %s", http->request.uri);
        return AD_HTTP_REQ_ERROR;
    }
    correctPathname(http->request.path);

    DEBUG("Method=%s, URI=%s, VER=%s", http->request.method, http->request.uri, http->request.httpver);

    return AD_HTTP_REQ_REQUESTLINE_DONE;
}

static int parse_headers(ad_http_t *http, struct evbuffer *in) {
    char *line;
    while ((line = evbuffer_readln(in, NULL, EVBUFFER_EOL_CRLF))) {
        if (IS_EMPTY_STR(line)) {
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


/******************************************************************************
 * Private internal functions.
 *****************************************************************************/

/**
 * validate file path
 */
static bool isValidPathname(const char *path) {
    if (path == NULL) return false;

    int len = strlen(path);
    if (len == 0 || len >= PATH_MAX) return false;
    else if (path[0] != '/') return false;
    else if (strpbrk(path, "\\:*?\"<>|") != NULL) return false;

    // check folder name length
    int n;
    char *t;
    for (n = 0, t = (char *)path; *t != '\0'; t++) {
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
    while (strstr(path, "//") != NULL) qstrreplace("sr", path, "//", "/");

    // Take care of tailing slash.
    int len = strlen(path);
    if (len <= 1) return;
    if (path[len - 1] == '/') path[len - 1] = '\0';
}

