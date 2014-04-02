#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <assert.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include "macro.h"
#include "qlibc.h"
#include "ad_server.h"
#include "ad_bypass_handler.h"

#ifndef _DOXYGEN_SKIP
static void bypass_free(ad_bypass_t *conn);
static void conn_read_cb(struct bufferevent *buffer, void *userdata) ;
static void conn_write_cb(struct bufferevent *buffer, void *userdata);
static void conn_event_cb(struct bufferevent *buffer, short what, void *userdata);
static void conn_cb(ad_bypass_t *conn, int event);
#endif

/**
 * Attach userdata into this connection.
 *
 * @return previous userdata;
 */
void *ad_bypass_set_userdata(ad_bypass_t *conn, const void *userdata) {
    void *prev = conn->userdata;
    conn->userdata = (void *)userdata;
    return prev;
}

void *ad_bypass_get_userdata(ad_bypass_t *conn) {
    return conn->userdata;
}

/******************************************************************************
 * Private internal functions.
 *****************************************************************************/
ad_bypass_t *bypass_new(ad_server_t *server, struct bufferevent *buffer) {
    if (server == NULL || buffer == NULL) {
        return NULL;
    }

    // Create a new connection container.
    ad_bypass_t *conn = NEW_STRUCT(ad_bypass_t);
    if (conn == NULL) return NULL;

    // Initialize with default values.
    conn->server = server;
    conn->buffer = buffer;
    conn->in = bufferevent_get_input(buffer);
    conn->out = bufferevent_get_output(buffer);
    conn->status = AD_OK;

    // Bind callback
    bufferevent_setcb(buffer, conn_read_cb, conn_write_cb, conn_event_cb, (void *)conn);
    bufferevent_setwatermark(buffer, EV_WRITE, 0, 0);
    bufferevent_enable(buffer, EV_WRITE);
    bufferevent_enable(buffer, EV_READ);

    // Run callbacks with AD_EVENT_INIT event.
    conn->status = call_hooks(AD_EVENT_INIT, conn->server->hooks, 0, NULL, conn);

    return conn;
}

static void bypass_free(ad_bypass_t *conn) {
    if (conn) {
        if (conn->status != AD_CLOSE) {
            call_hooks(AD_EVENT_CLOSE | AD_EVENT_SHUTDOWN , conn->server->hooks, 0, NULL, conn);
        }
        if (conn->buffer) {
            bufferevent_free(conn->buffer);
        }
        if (conn->userdata) {
            WARN("Found unreleased userdata.");
        }
        free(conn);
    }
}

#define DRAIN_EVBUFFER(b) evbuffer_drain(b, evbuffer_get_length(b))
static void conn_read_cb(struct bufferevent *buffer, void *userdata) {
    DEBUG("read_cb");
    ad_bypass_t *conn = userdata;
    conn_cb(conn, AD_EVENT_READ);
}

static void conn_write_cb(struct bufferevent *buffer, void *userdata) {
    DEBUG("write_cb");
    ad_bypass_t *conn = userdata;
    conn_cb(conn, AD_EVENT_WRITE);
}

static void conn_event_cb(struct bufferevent *buffer, short what, void *userdata) {
    DEBUG("event_cb 0x%x", what);
    ad_bypass_t *conn = userdata;

    if (what & BEV_EVENT_EOF || what & BEV_EVENT_ERROR || what & BEV_EVENT_TIMEOUT) {
        conn->status = AD_CLOSE;
        conn_cb(conn, AD_EVENT_CLOSE | ((what & BEV_EVENT_TIMEOUT) ? AD_EVENT_TIMEOUT : 0));
    }
}

static void conn_cb(ad_bypass_t *conn, int event) {
DEBUG("conn: status:0x%x, event:0x%x", conn->status, event)
    if(conn->status == AD_OK || conn->status == AD_TAKEOVER) {
        conn->status = call_hooks(event, conn->server->hooks, 0, NULL, conn);
    }

    if(conn->status == AD_DONE) {
        // Do nothing but draining buffer.
        if (event == AD_EVENT_READ) {
            DEBUG("Draining buffer. %d", conn->status);
            DRAIN_EVBUFFER(conn->in);
        }
        return;
    } else if(conn->status == AD_CLOSE) {
        if (evbuffer_get_length(conn->out) <= 0) {
            int newevent = (event & AD_EVENT_CLOSE) ? event : AD_EVENT_CLOSE;
            call_hooks(newevent , conn->server->hooks, 0, NULL, conn);
            bypass_free(conn);
            DEBUG("Connection closed.");
            return;
        }
    }
}
