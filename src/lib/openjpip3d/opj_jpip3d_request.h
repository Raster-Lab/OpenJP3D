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
 * @file opj_jpip3d_request.h
 * @brief Internal header: JPIP 3-D request parsing and serialisation.
 */

#ifndef OPJ_JPIP3D_REQUEST_H
#define OPJ_JPIP3D_REQUEST_H

#include "openjpip3d.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Parse a JPIP 3-D URL query string into an opj_jpip3d_request_t.
 *
 * Supported parameters: fsiz3d=w,h,d  roff3d=x,y,z  rsiz3d=w,h,d
 * comp=N  layers=N  level=N  sid=N  type=N  dataset=name
 *
 * @param query  Null-terminated URL query string (e.g. "fsiz3d=4,4,4&comp=0").
 * @param req    Output request structure, zeroed before parsing begins.
 * @return 1 on success, 0 on failure.
 */
int opj_jpip3d_parse_request(const char *query, opj_jpip3d_request_t *req);

/**
 * @brief Serialise an opj_jpip3d_request_t to a URL query string.
 *
 * @param req    Request to serialise.
 * @param buf    Output buffer.
 * @param buflen Capacity of @p buf including space for the NUL terminator.
 * @return Number of bytes written (not counting NUL), or -1 on error.
 */
int opj_jpip3d_request_to_url_params(const opj_jpip3d_request_t *req,
                                     char *buf, size_t buflen);

#ifdef __cplusplus
}
#endif

#endif /* OPJ_JPIP3D_REQUEST_H */
