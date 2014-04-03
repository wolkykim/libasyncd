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
 * ad_bypass_handler header file.
 *
 * @file ad_bypass_handler.h
 */

#ifndef _AD_BYPASS_HANDLER_H
#define _AD_BYPASS_HANDLER_H

#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include "ad_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*\
|                                 TYPEDEFS                                    |
\*---------------------------------------------------------------------------*/
typedef struct ad_bypass_s ad_bypass_t;

/*---------------------------------------------------------------------------*\
|                                STRUCTURES                                   |
\*---------------------------------------------------------------------------*/
/**
 * Bypass-handler structure.
 */
struct ad_bypass_s {
    ad_server_t *server;        /*!< reference pointer to server */
    struct bufferevent *buffer; /*!< reference pointer to buffer */
    struct evbuffer *in;        /*!< in buffer */
    struct evbuffer *out;       /*!< out buffer */
    int status;                 /*!< hook status such as AD_OK */
    void *userdata;         /*!< user data */
};

/*----------------------------------------------------------------------------*\
|                             PUBLIC FUNCTIONS                                 |
\*----------------------------------------------------------------------------*/
extern void *ad_bypass_set_userdata(ad_bypass_t *conn, const void *userdata);
extern void *ad_bypass_get_userdata(ad_bypass_t *conn);

/*---------------------------------------------------------------------------*\
|                             INTERNAL USE ONLY                               |
\*---------------------------------------------------------------------------*/
#ifndef _DOXYGEN_SKIP
extern void *bypass_new(ad_server_t *server, struct bufferevent *buffer);
#endif /* _DOXYGEN_SKIP */

#ifdef __cplusplus
}
#endif

#endif /*_AD_BYPASS_HANDLER_H */
