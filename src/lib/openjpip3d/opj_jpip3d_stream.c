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
 * @file opj_jpip3d_stream.c
 * @brief JPT/JPP growing-buffer stream implementation.
 */

#include "opj_jpip3d_stream.h"

#include "openjp3d.h"   /* opj_jp3d_malloc / realloc / free */

#include <string.h>
#include <stddef.h>

/* -------------------------------------------------------------------------
 * Internal struct definition
 * ------------------------------------------------------------------------- */

struct opj_jpip3d_stream {
    uint8_t *data;
    size_t   size;
    size_t   capacity;
};

#define STREAM_INITIAL_CAP 256

/* -------------------------------------------------------------------------
 * Private helpers
 * ------------------------------------------------------------------------- */

/** Ensure the stream has room for @p extra more bytes. */
static int stream_reserve(opj_jpip3d_stream_t *s, size_t extra)
{
    if (s->size + extra <= s->capacity) return 1;

    size_t new_cap = s->capacity ? s->capacity : STREAM_INITIAL_CAP;
    while (new_cap < s->size + extra) new_cap *= 2;

    uint8_t *new_data = (uint8_t *)opj_jp3d_realloc(s->data, new_cap);
    if (!new_data) return 0;
    s->data     = new_data;
    s->capacity = new_cap;
    return 1;
}

/** Append @p len raw bytes to the stream. */
static int stream_write_bytes(opj_jpip3d_stream_t *s,
                               const uint8_t *bytes, size_t len)
{
    if (!stream_reserve(s, len)) return 0;
    memcpy(s->data + s->size, bytes, len);
    s->size += len;
    return 1;
}

/** Write a big-endian uint32_t to the stream. */
static int stream_write_u32_be(opj_jpip3d_stream_t *s, uint32_t v)
{
    uint8_t b[4];
    b[0] = (uint8_t)((v >> 24) & 0xFF);
    b[1] = (uint8_t)((v >> 16) & 0xFF);
    b[2] = (uint8_t)((v >>  8) & 0xFF);
    b[3] = (uint8_t)( v        & 0xFF);
    return stream_write_bytes(s, b, 4);
}

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

opj_jpip3d_stream_t *opj_jpip3d_stream_create(void)
{
    opj_jpip3d_stream_t *s =
        (opj_jpip3d_stream_t *)opj_jp3d_malloc(sizeof(opj_jpip3d_stream_t));
    if (!s) return NULL;
    s->data     = NULL;
    s->size     = 0;
    s->capacity = 0;
    return s;
}

void opj_jpip3d_stream_destroy(opj_jpip3d_stream_t *s)
{
    if (!s) return;
    opj_jp3d_free(s->data);
    opj_jp3d_free(s);
}

const uint8_t *opj_jpip3d_stream_get_data(const opj_jpip3d_stream_t *s)
{
    return s ? s->data : NULL;
}

size_t opj_jpip3d_stream_get_size(const opj_jpip3d_stream_t *s)
{
    return s ? s->size : 0;
}

int opj_jpip3d_jpt_write_tile_part(opj_jpip3d_stream_t *s,
    uint32_t tile_x,  uint32_t tile_y,  uint32_t tile_z,
    uint32_t layer,   uint32_t res_level, uint32_t comp,
    const uint8_t *data, uint32_t data_len)
{
    if (!s) return 0;

    /* Magic "JPT3" */
    if (!stream_write_bytes(s, (const uint8_t *)OPJ_JPIP3D_JPT_MAGIC, 4))
        return 0;

    /* Header fields */
    if (!stream_write_u32_be(s, tile_x))     return 0;
    if (!stream_write_u32_be(s, tile_y))     return 0;
    if (!stream_write_u32_be(s, tile_z))     return 0;
    if (!stream_write_u32_be(s, layer))      return 0;
    if (!stream_write_u32_be(s, res_level))  return 0;
    if (!stream_write_u32_be(s, comp))       return 0;
    if (!stream_write_u32_be(s, data_len))   return 0;

    /* Payload */
    if (data_len > 0 && data) {
        if (!stream_write_bytes(s, data, data_len)) return 0;
    }
    return 1;
}

int opj_jpip3d_jpp_write_precinct(opj_jpip3d_stream_t *s,
    uint32_t tile_x,  uint32_t tile_y,  uint32_t tile_z,
    uint32_t prec_x,  uint32_t prec_y,  uint32_t prec_z,
    uint32_t layer,   uint32_t res_level, uint32_t comp,
    const uint8_t *data, uint32_t data_len)
{
    if (!s) return 0;

    /* Magic "JPP3" */
    if (!stream_write_bytes(s, (const uint8_t *)OPJ_JPIP3D_JPP_MAGIC, 4))
        return 0;

    /* Header fields */
    if (!stream_write_u32_be(s, tile_x))     return 0;
    if (!stream_write_u32_be(s, tile_y))     return 0;
    if (!stream_write_u32_be(s, tile_z))     return 0;
    if (!stream_write_u32_be(s, prec_x))     return 0;
    if (!stream_write_u32_be(s, prec_y))     return 0;
    if (!stream_write_u32_be(s, prec_z))     return 0;
    if (!stream_write_u32_be(s, layer))      return 0;
    if (!stream_write_u32_be(s, res_level))  return 0;
    if (!stream_write_u32_be(s, comp))       return 0;
    if (!stream_write_u32_be(s, data_len))   return 0;

    /* Payload */
    if (data_len > 0 && data) {
        if (!stream_write_bytes(s, data, data_len)) return 0;
    }
    return 1;
}
