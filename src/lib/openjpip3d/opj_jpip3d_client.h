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
 * @file opj_jpip3d_client.h
 * @brief Internal header: JPIP 3-D client session.
 */

#ifndef OPJ_JPIP3D_CLIENT_H
#define OPJ_JPIP3D_CLIENT_H

#include "openjpip3d.h"
#include "opj_jpip3d_server.h"
#include "opj_jpip3d_cache.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief A client-side JPIP 3-D session.
 */
typedef struct opj_jpip3d_session {
    opj_jpip3d_server_t *server;       /**< Pointer to the connected server. */
    uint32_t             session_id;   /**< Unique identifier for this session. */
    opj_jpip3d_cache_t  *local_cache;  /**< Client-side delivery cache. */
} opj_jpip3d_session_t;

/** @brief Open a session against @p server. Returns NULL on failure. */
opj_jpip3d_session_t *opj_jpip3d_session_open(opj_jpip3d_server_t *server);
/** @brief Close and free a session. */
void                  opj_jpip3d_session_close(opj_jpip3d_session_t *session);
/**
 * @brief Submit a request and receive the server response.
 *
 * Sets req->session_id and delegates to opj_jpip3d_server_handle_request.
 * @return 1 on success, 0 on failure.
 */
int opj_jpip3d_session_request_region(opj_jpip3d_session_t *session,
                                      opj_jpip3d_request_t *req,
                                      opj_jpip3d_response_t *resp);
/**
 * @brief Request a region and decode the response into an opj_volume_t.
 *
 * Returns NULL if the response is empty (already cached) or on error.
 * Caller must free the returned volume with opj_jp3d_destroy_volume().
 */
opj_volume_t *opj_jpip3d_session_receive_volume(opj_jpip3d_session_t *session,
                                                opj_jpip3d_request_t *req);

#ifdef __cplusplus
}
#endif

#endif /* OPJ_JPIP3D_CLIENT_H */
