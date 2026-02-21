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
 * @file opj_jpip3d_request.c
 * @brief JPIP 3-D request parsing and serialisation implementation.
 */

#include "opj_jpip3d_request.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* -------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------- */

/** Copy at most n-1 chars from src to dst and NUL-terminate. */
static void safe_strncpy(char *dst, const char *src, size_t n)
{
    if (n == 0) return;
    strncpy(dst, src, n - 1);
    dst[n - 1] = '\0';
}

/* -------------------------------------------------------------------------
 * opj_jpip3d_parse_request
 * Iterates over "key=value&key=value" pairs without strtok_r.
 * ------------------------------------------------------------------------- */

int opj_jpip3d_parse_request(const char *query, opj_jpip3d_request_t *req)
{
    if (!query || !req) return 0;

    memset(req, 0, sizeof(*req));

    const char *p = query;

    while (*p) {
        /* Find end of this "key=value" pair (delimited by '&' or '\0'). */
        const char *amp = p;
        while (*amp && *amp != '&') amp++;

        /* Find '=' within [p, amp). */
        const char *eq = p;
        while (eq < amp && *eq != '=') eq++;
        if (eq == amp) { p = *amp ? amp + 1 : amp; continue; } /* no '=' */

        /* Key is [p, eq), value is (eq, amp). */
        size_t key_len = (size_t)(eq - p);
        size_t val_len = (size_t)(amp - eq - 1);

        char key[64]  = {0};
        char val[512] = {0};

        if (key_len >= sizeof(key)) key_len = sizeof(key) - 1;
        if (val_len >= sizeof(val)) val_len = sizeof(val) - 1;

        memcpy(key, p, key_len);
        key[key_len] = '\0';
        memcpy(val, eq + 1, val_len);
        val[val_len] = '\0';

        if (strcmp(key, "fsiz3d") == 0) {
            sscanf(val, "%u,%u,%u",
                   &req->fsiz3d[0], &req->fsiz3d[1], &req->fsiz3d[2]);
        } else if (strcmp(key, "roff3d") == 0) {
            sscanf(val, "%u,%u,%u",
                   &req->roff3d[0], &req->roff3d[1], &req->roff3d[2]);
        } else if (strcmp(key, "rsiz3d") == 0) {
            sscanf(val, "%u,%u,%u",
                   &req->rsiz3d[0], &req->rsiz3d[1], &req->rsiz3d[2]);
        } else if (strcmp(key, "comp") == 0) {
            sscanf(val, "%u", &req->component);
        } else if (strcmp(key, "layers") == 0) {
            sscanf(val, "%u", &req->quality_layers);
        } else if (strcmp(key, "level") == 0) {
            sscanf(val, "%u", &req->resolution_level);
        } else if (strcmp(key, "sid") == 0) {
            sscanf(val, "%u", &req->session_id);
        } else if (strcmp(key, "type") == 0) {
            sscanf(val, "%d", &req->stream_type);
        } else if (strcmp(key, "dataset") == 0) {
            safe_strncpy(req->dataset, val, OPJ_JPIP3D_DATASET_NAME_MAX);
        }

        p = *amp ? amp + 1 : amp;
    }

    return 1;
}

/* -------------------------------------------------------------------------
 * opj_jpip3d_request_to_url_params
 * ------------------------------------------------------------------------- */

int opj_jpip3d_request_to_url_params(const opj_jpip3d_request_t *req,
                                     char *buf, size_t buflen)
{
    if (!req || !buf || buflen == 0) return -1;

    int written = snprintf(buf, buflen,
        "fsiz3d=%u,%u,%u"
        "&roff3d=%u,%u,%u"
        "&rsiz3d=%u,%u,%u"
        "&comp=%u"
        "&layers=%u"
        "&level=%u"
        "&sid=%u"
        "&type=%d"
        "&dataset=%s",
        req->fsiz3d[0], req->fsiz3d[1], req->fsiz3d[2],
        req->roff3d[0], req->roff3d[1], req->roff3d[2],
        req->rsiz3d[0], req->rsiz3d[1], req->rsiz3d[2],
        req->component,
        req->quality_layers,
        req->resolution_level,
        req->session_id,
        req->stream_type,
        req->dataset[0] ? req->dataset : "");

    if (written < 0 || (size_t)written >= buflen) return -1;
    return written;
}
