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
 * @file openjpip3d.c
 * @brief OpenJPIP3D JPIP 3-D streaming — public API implementation.
 *
 * This file provides the public entry points declared in openjpip3d.h.
 * Most work is delegated to the internal modules; the only non-trivial
 * logic here is the adaptation between the public individual-parameter
 * cache API and the internal key-struct API.
 */

#include "openjpip3d.h"

/* Internal module headers — order matters for dependency satisfaction. */
#include "opj_jpip3d_cache.h"
#include "opj_jpip3d_request.h"
#include "opj_jpip3d_stream.h"
#include "opj_jpip3d_meta.h"
#include "opj_jpip3d_server.h"
#include "opj_jpip3d_client.h"

/** @brief JPIP3D version string. */
#define OPJ_JPIP3D_VERSION "0.1.0"

/* =========================================================================
 * Version
 * ========================================================================= */

const char *opj_jpip3d_get_version(void)
{
    return OPJ_JPIP3D_VERSION;
}

/* =========================================================================
 * Cache — public individual-parameter wrappers
 *
 * The internal functions use opj_jpip3d_precinct_key_t; the public API
 * avoids exposing that struct to external consumers.
 * ========================================================================= */

static void fill_key(opj_jpip3d_precinct_key_t *k,
                     uint32_t session_id,
                     uint32_t tile_x,  uint32_t tile_y,  uint32_t tile_z,
                     uint32_t layer,   uint32_t res,      uint32_t comp,
                     uint32_t prec_x,  uint32_t prec_y,  uint32_t prec_z)
{
    k->session_id = session_id;
    k->tile_x     = tile_x;  k->tile_y    = tile_y;  k->tile_z    = tile_z;
    k->layer      = layer;   k->res       = res;      k->comp      = comp;
    k->precinct_x = prec_x;  k->precinct_y = prec_y; k->precinct_z = prec_z;
}

int opj_jpip3d_cache_is_precinct_delivered(
    const opj_jpip3d_cache_t *cache,
    uint32_t session_id,
    uint32_t tile_x,  uint32_t tile_y,  uint32_t tile_z,
    uint32_t layer,   uint32_t res,     uint32_t comp,
    uint32_t prec_x,  uint32_t prec_y,  uint32_t prec_z)
{
    opj_jpip3d_precinct_key_t k;
    fill_key(&k, session_id, tile_x, tile_y, tile_z,
             layer, res, comp, prec_x, prec_y, prec_z);
    return opj_jpip3d_cache_is_precinct_delivered_by_key(cache, &k);
}

int opj_jpip3d_cache_mark_precinct_delivered(
    opj_jpip3d_cache_t *cache,
    uint32_t session_id,
    uint32_t tile_x,  uint32_t tile_y,  uint32_t tile_z,
    uint32_t layer,   uint32_t res,     uint32_t comp,
    uint32_t prec_x,  uint32_t prec_y,  uint32_t prec_z)
{
    opj_jpip3d_precinct_key_t k;
    fill_key(&k, session_id, tile_x, tile_y, tile_z,
             layer, res, comp, prec_x, prec_y, prec_z);
    return opj_jpip3d_cache_mark_precinct_delivered_by_key(cache, &k);
}
