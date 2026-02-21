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
 * @file opj_jpip3d_server.c
 * @brief JPIP 3-D server implementation.
 */

#include "opj_jpip3d_server.h"
#include "opj_jpip3d_stream.h"
#include "opj_jpip3d_meta.h"

#include "openjp3d.h"

#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define SERVER_INITIAL_CAP 8

/* -------------------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------------------- */

opj_jpip3d_server_t *opj_jpip3d_server_create(void)
{
    opj_jpip3d_server_t *srv =
        (opj_jpip3d_server_t *)opj_jp3d_malloc(sizeof(opj_jpip3d_server_t));
    if (!srv) return NULL;

    srv->datasets     = NULL;
    srv->num_datasets = 0;
    srv->cap_datasets = 0;
    srv->cache        = opj_jpip3d_cache_create();
    if (!srv->cache) {
        opj_jp3d_free(srv);
        return NULL;
    }
    return srv;
}

void opj_jpip3d_server_destroy(opj_jpip3d_server_t *server)
{
    if (!server) return;

    for (size_t i = 0; i < server->num_datasets; i++) {
        opj_jp3d_free(server->datasets[i].codestream);
    }
    opj_jp3d_free(server->datasets);
    opj_jpip3d_cache_destroy(server->cache);
    opj_jp3d_free(server);
}

/* -------------------------------------------------------------------------
 * Dataset management
 * ------------------------------------------------------------------------- */

/** Find the dataset index with the given name, or (size_t)-1 if not found. */
static size_t find_dataset(const opj_jpip3d_server_t *srv, const char *name)
{
    for (size_t i = 0; i < srv->num_datasets; i++) {
        if (strncmp(srv->datasets[i].name, name, 255) == 0) return i;
    }
    return (size_t)-1;
}

/** Get or create a dataset slot for @p name. Returns pointer or NULL. */
static opj_jpip3d_dataset_t *get_or_create_dataset(opj_jpip3d_server_t *srv,
                                                     const char *name)
{
    size_t idx = find_dataset(srv, name);
    if (idx != (size_t)-1) return &srv->datasets[idx];

    /* Grow array if necessary. */
    if (srv->num_datasets >= srv->cap_datasets) {
        size_t new_cap = srv->cap_datasets ? srv->cap_datasets * 2
                                           : SERVER_INITIAL_CAP;
        opj_jpip3d_dataset_t *new_arr =
            (opj_jpip3d_dataset_t *)opj_jp3d_realloc(
                srv->datasets,
                new_cap * sizeof(opj_jpip3d_dataset_t));
        if (!new_arr) return NULL;
        srv->datasets     = new_arr;
        srv->cap_datasets = new_cap;
    }

    opj_jpip3d_dataset_t *ds = &srv->datasets[srv->num_datasets++];
    memset(ds, 0, sizeof(*ds));
    strncpy(ds->name, name, sizeof(ds->name) - 1);
    ds->name[sizeof(ds->name) - 1] = '\0';
    return ds;
}

int opj_jpip3d_server_load_dataset_mem(opj_jpip3d_server_t *server,
                                        const char *name,
                                        const uint8_t *data, size_t size)
{
    if (!server || !name || !data || size == 0) return 0;

    opj_jpip3d_dataset_t *ds = get_or_create_dataset(server, name);
    if (!ds) return 0;

    /* Copy codestream. */
    uint8_t *copy = (uint8_t *)opj_jp3d_malloc(size);
    if (!copy) return 0;
    memcpy(copy, data, size);

    /* Decode briefly to get volume dimensions. */
    opj_volume_t *vol = opj_jp3d_decode(copy, size, NULL, NULL, NULL);
    if (!vol) {
        opj_jp3d_free(copy);
        return 0;
    }

    ds->width          = vol->comps[0].w;
    ds->height         = vol->comps[0].h;
    ds->depth          = vol->comps[0].d;
    ds->numcomps       = vol->numcomps;
    opj_jp3d_destroy_volume(vol);

    /* Replace any existing codestream. */
    opj_jp3d_free(ds->codestream);
    ds->codestream      = copy;
    ds->codestream_size = size;
    return 1;
}

int opj_jpip3d_server_load_dataset(opj_jpip3d_server_t *server,
                                    const char *name, const char *filepath)
{
    if (!server || !name || !filepath) return 0;

    FILE *fp = fopen(filepath, "rb");
    if (!fp) return 0;

    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return 0; }
    long flen = ftell(fp);
    if (flen <= 0) { fclose(fp); return 0; }
    rewind(fp);

    uint8_t *buf = (uint8_t *)opj_jp3d_malloc((size_t)flen);
    if (!buf) { fclose(fp); return 0; }

    if (fread(buf, 1, (size_t)flen, fp) != (size_t)flen) {
        opj_jp3d_free(buf);
        fclose(fp);
        return 0;
    }
    fclose(fp);

    int ok = opj_jpip3d_server_load_dataset_mem(server, name, buf, (size_t)flen);
    opj_jp3d_free(buf);
    return ok;
}

/* -------------------------------------------------------------------------
 * Request handling
 * ------------------------------------------------------------------------- */

int opj_jpip3d_server_handle_request(opj_jpip3d_server_t *server,
                                      const opj_jpip3d_request_t *req,
                                      opj_jpip3d_response_t *resp)
{
    if (!server || !req || !resp) return 0;

    /* Initialise response to empty. */
    memset(resp, 0, sizeof(*resp));
    resp->stream_type = OPJ_JPIP3D_STREAM_JP3D;

    /* 1. Find dataset. */
    size_t idx = find_dataset(server, req->dataset);
    if (idx == (size_t)-1) return 0;
    const opj_jpip3d_dataset_t *ds = &server->datasets[idx];

    /* 2. Build cache key for this region/session. */
    opj_jpip3d_precinct_key_t key;
    key.session_id  = req->session_id;
    key.tile_x      = req->roff3d[0];
    key.tile_y      = req->roff3d[1];
    key.tile_z      = req->roff3d[2];
    key.layer       = 0;
    key.res         = 0;
    key.comp        = req->component;
    key.precinct_x  = req->rsiz3d[0];
    key.precinct_y  = req->rsiz3d[1];
    key.precinct_z  = req->rsiz3d[2];

    /* 3. Already delivered? Return empty response. */
    if (opj_jpip3d_cache_is_precinct_delivered_by_key(server->cache, &key)) {
        resp->data = NULL;
        resp->size = 0;
        return 1;
    }

    /* 4. Decode full codestream. */
    opj_volume_t *vol = opj_jp3d_decode(ds->codestream, ds->codestream_size,
                                         NULL, NULL, NULL);
    if (!vol) return 0;

    uint32_t W = vol->comps[0].w;
    uint32_t H = vol->comps[0].h;
    uint32_t D = vol->comps[0].d;
    uint32_t numcomps = vol->numcomps;

    /* 5. Compute clamped sub-volume bounds. */
    uint32_t x0 = req->roff3d[0] < W ? req->roff3d[0] : W;
    uint32_t y0 = req->roff3d[1] < H ? req->roff3d[1] : H;
    uint32_t z0 = req->roff3d[2] < D ? req->roff3d[2] : D;

    uint32_t sw = req->rsiz3d[0];
    uint32_t sh = req->rsiz3d[1];
    uint32_t sd = req->rsiz3d[2];

    if (x0 + sw > W) sw = W - x0;
    if (y0 + sh > H) sh = H - y0;
    if (z0 + sd > D) sd = D - z0;

    if (sw == 0 || sh == 0 || sd == 0) {
        opj_jp3d_destroy_volume(vol);
        return 0;
    }

    /* 6. Create and populate sub-volume. */
    uint32_t prec = vol->comps[0].prec;
    int32_t  sgnd = vol->comps[0].sgnd;
    opj_volume_t *sub = opj_jp3d_create_volume(numcomps, sw, sh, sd, prec, sgnd);
    if (!sub) {
        opj_jp3d_destroy_volume(vol);
        return 0;
    }

    for (uint32_t c = 0; c < numcomps; c++) {
        for (uint32_t z = z0; z < z0 + sd; z++) {
            for (uint32_t y = y0; y < y0 + sh; y++) {
                for (uint32_t x = x0; x < x0 + sw; x++) {
                    sub->comps[c].data[(z - z0) * sw * sh +
                                       (y - y0) * sw      +
                                       (x - x0)]
                        = vol->comps[c].data[z * W * H + y * W + x];
                }
            }
        }
    }

    opj_jp3d_destroy_volume(vol);

    /* 7. Encode sub-volume. */
    opj_jp3d_enc_params_t enc;
    opj_jp3d_set_default_encoder_parameters(&enc);
    enc.num_resolutions_x = 1;
    enc.num_resolutions_y = 1;
    enc.num_resolutions_z = 1;
    enc.tile_width  = sw;
    enc.tile_height = sh;
    enc.tile_depth  = sd;
    enc.cblk_width  = sw;
    enc.cblk_height = sh;
    enc.cblk_depth  = sd;

    uint8_t *enc_data = NULL;
    size_t   enc_size = 0;
    if (!opj_jp3d_encode(sub, &enc, &enc_data, &enc_size, NULL, NULL)) {
        opj_jp3d_destroy_volume(sub);
        return 0;
    }
    opj_jp3d_destroy_volume(sub);

    /* 8. Mark cache. */
    opj_jpip3d_cache_mark_precinct_delivered_by_key(server->cache, &key);

    /* 9. Fill response (caller frees enc_data). */
    resp->data        = enc_data;
    resp->size        = enc_size;
    resp->stream_type = OPJ_JPIP3D_STREAM_JP3D;
    resp->metadata    = NULL;
    return 1;
}
