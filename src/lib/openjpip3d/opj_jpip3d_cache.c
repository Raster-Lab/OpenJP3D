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
 * @file opj_jpip3d_cache.c
 * @brief Precinct/tile delivery cache implementation (growing array of keys).
 */

#include "opj_jpip3d_cache.h"

#include "openjp3d.h"   /* opj_jp3d_malloc / realloc / free */

#include <string.h>
#include <stddef.h>

/* -------------------------------------------------------------------------
 * Internal struct definition
 * ------------------------------------------------------------------------- */

struct opj_jpip3d_cache {
    opj_jpip3d_precinct_key_t *keys;
    size_t                     count;
    size_t                     capacity;
};

#define CACHE_INITIAL_CAP 64

/* -------------------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------------------- */

opj_jpip3d_cache_t *opj_jpip3d_cache_create(void)
{
    opj_jpip3d_cache_t *c =
        (opj_jpip3d_cache_t *)opj_jp3d_malloc(sizeof(opj_jpip3d_cache_t));
    if (!c) return NULL;
    c->keys     = NULL;
    c->count    = 0;
    c->capacity = 0;
    return c;
}

void opj_jpip3d_cache_destroy(opj_jpip3d_cache_t *cache)
{
    if (!cache) return;
    opj_jp3d_free(cache->keys);
    opj_jp3d_free(cache);
}

/* -------------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------------- */

static int keys_equal(const opj_jpip3d_precinct_key_t *a,
                      const opj_jpip3d_precinct_key_t *b)
{
    return (a->session_id  == b->session_id  &&
            a->tile_x      == b->tile_x      &&
            a->tile_y      == b->tile_y      &&
            a->tile_z      == b->tile_z      &&
            a->layer       == b->layer       &&
            a->res         == b->res         &&
            a->comp        == b->comp        &&
            a->precinct_x  == b->precinct_x  &&
            a->precinct_y  == b->precinct_y  &&
            a->precinct_z  == b->precinct_z);
}

/* -------------------------------------------------------------------------
 * Public (by-key) API
 * ------------------------------------------------------------------------- */

int opj_jpip3d_cache_is_precinct_delivered_by_key(
    const opj_jpip3d_cache_t        *cache,
    const opj_jpip3d_precinct_key_t *key)
{
    if (!cache || !key) return 0;
    for (size_t i = 0; i < cache->count; i++) {
        if (keys_equal(&cache->keys[i], key)) return 1;
    }
    return 0;
}

int opj_jpip3d_cache_mark_precinct_delivered_by_key(
    opj_jpip3d_cache_t              *cache,
    const opj_jpip3d_precinct_key_t *key)
{
    if (!cache || !key) return 0;

    /* Already present? */
    if (opj_jpip3d_cache_is_precinct_delivered_by_key(cache, key)) return 1;

    /* Grow array if needed. */
    if (cache->count >= cache->capacity) {
        size_t new_cap = cache->capacity ? cache->capacity * 2 : CACHE_INITIAL_CAP;
        opj_jpip3d_precinct_key_t *new_keys =
            (opj_jpip3d_precinct_key_t *)opj_jp3d_realloc(
                cache->keys,
                new_cap * sizeof(opj_jpip3d_precinct_key_t));
        if (!new_keys) return 0;
        cache->keys     = new_keys;
        cache->capacity = new_cap;
    }

    cache->keys[cache->count++] = *key;
    return 1;
}

void opj_jpip3d_cache_reset_session(opj_jpip3d_cache_t *cache,
                                    uint32_t session_id)
{
    if (!cache) return;

    /* Compact in-place: keep entries whose session_id differs. */
    size_t write = 0;
    for (size_t read = 0; read < cache->count; read++) {
        if (cache->keys[read].session_id != session_id) {
            cache->keys[write++] = cache->keys[read];
        }
    }
    cache->count = write;
}
