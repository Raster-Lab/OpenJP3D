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
 * @file opj_jpip3d_cache.h
 * @brief Internal header: precinct/tile delivery cache model.
 *
 * This header exposes an internal API using the @c opj_jpip3d_precinct_key_t
 * composite key.  The public API (openjpip3d.h) exposes individual-parameter
 * wrappers to avoid leaking this internal type to consumers.
 */

#ifndef OPJ_JPIP3D_CACHE_H
#define OPJ_JPIP3D_CACHE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Precinct key type
 * ------------------------------------------------------------------------- */

/**
 * @brief Composite key identifying a single 3-D precinct within a session.
 */
typedef struct opj_jpip3d_precinct_key {
    uint32_t session_id;
    uint32_t tile_x,    tile_y,    tile_z;
    uint32_t layer,     res,       comp;
    uint32_t precinct_x, precinct_y, precinct_z;
} opj_jpip3d_precinct_key_t;

/* -------------------------------------------------------------------------
 * Opaque cache handle
 * ------------------------------------------------------------------------- */

/** @brief Opaque handle to the precinct-delivery cache. */
typedef struct opj_jpip3d_cache opj_jpip3d_cache_t;

/* -------------------------------------------------------------------------
 * Cache lifecycle
 * ------------------------------------------------------------------------- */

/** @brief Allocate a new empty cache. Returns NULL on failure. */
opj_jpip3d_cache_t *opj_jpip3d_cache_create(void);
/** @brief Destroy @p cache and free all memory. */
void                opj_jpip3d_cache_destroy(opj_jpip3d_cache_t *cache);

/* -------------------------------------------------------------------------
 * Internal (by-key) query / update functions
 * These names carry the _by_key suffix to avoid clashing with the public
 * individual-parameter wrappers declared in openjpip3d.h.
 * ------------------------------------------------------------------------- */

/**
 * @brief Return 1 if @p key has already been delivered, 0 otherwise.
 */
int opj_jpip3d_cache_is_precinct_delivered_by_key(
    const opj_jpip3d_cache_t        *cache,
    const opj_jpip3d_precinct_key_t *key);

/**
 * @brief Record that @p key has been delivered.
 * @return 1 on success, 0 on allocation failure.
 */
int opj_jpip3d_cache_mark_precinct_delivered_by_key(
    opj_jpip3d_cache_t              *cache,
    const opj_jpip3d_precinct_key_t *key);

/**
 * @brief Remove all entries belonging to @p session_id.
 */
void opj_jpip3d_cache_reset_session(opj_jpip3d_cache_t *cache,
                                    uint32_t session_id);

#ifdef __cplusplus
}
#endif

#endif /* OPJ_JPIP3D_CACHE_H */
