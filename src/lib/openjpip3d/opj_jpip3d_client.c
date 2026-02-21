/*
 * Copyright (c) 2024-2026, OpenJP3D Contributors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file opj_jpip3d_client.c
 * @brief JPIP 3-D client session implementation.
 */

#include "opj_jpip3d_client.h"

#include "openjp3d.h"

#include <stddef.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Session-ID generator (monotonically incrementing, no thread safety)
 * ------------------------------------------------------------------------- */
static uint32_t s_next_session_id = 1;

/* -------------------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------------------- */

opj_jpip3d_session_t *opj_jpip3d_session_open(opj_jpip3d_server_t *server)
{
    if (!server) return NULL;

    opj_jpip3d_session_t *sess =
        (opj_jpip3d_session_t *)opj_jp3d_malloc(sizeof(opj_jpip3d_session_t));
    if (!sess) return NULL;

    sess->server      = server;
    sess->session_id  = s_next_session_id++;
    sess->local_cache = opj_jpip3d_cache_create();
    if (!sess->local_cache) {
        opj_jp3d_free(sess);
        return NULL;
    }
    return sess;
}

void opj_jpip3d_session_close(opj_jpip3d_session_t *session)
{
    if (!session) return;
    opj_jpip3d_cache_destroy(session->local_cache);
    opj_jp3d_free(session);
}

/* -------------------------------------------------------------------------
 * Request / receive
 * ------------------------------------------------------------------------- */

int opj_jpip3d_session_request_region(opj_jpip3d_session_t *session,
                                      opj_jpip3d_request_t *req,
                                      opj_jpip3d_response_t *resp)
{
    if (!session || !req || !resp) return 0;

    /* Stamp the session-ID into the request so the server can cache it. */
    req->session_id = session->session_id;

    return opj_jpip3d_server_handle_request(session->server, req, resp);
}

opj_volume_t *opj_jpip3d_session_receive_volume(opj_jpip3d_session_t *session,
                                                opj_jpip3d_request_t *req)
{
    if (!session || !req) return NULL;

    opj_jpip3d_response_t resp;
    memset(&resp, 0, sizeof(resp));

    if (!opj_jpip3d_session_request_region(session, req, &resp)) return NULL;

    /* Empty response means the region was already cached. */
    if (resp.size == 0 || resp.data == NULL) {
        opj_jp3d_free(resp.data);
        return NULL;
    }

    /* Decode the codestream payload into a volume. */
    opj_volume_t *vol = opj_jp3d_decode(resp.data, resp.size, NULL, NULL, NULL);
    opj_jp3d_free(resp.data);
    return vol;
}
