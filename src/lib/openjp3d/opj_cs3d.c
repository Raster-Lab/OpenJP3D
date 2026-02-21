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
 * @file opj_cs3d.c
 * @brief Codestream I/O helpers and growable buffer for OpenJP3D.
 */

#include "opj_cs3d.h"
#include "opj_mem.h"

#include <string.h>

/* =========================================================================
 * opj_buf_t
 * ========================================================================= */

int opj_buf_write(opj_buf_t *b, const uint8_t *src, size_t n)
{
    if (n == 0)
        return 1;
    if (b->len + n > b->cap) {
        size_t new_cap = b->cap ? b->cap * 2 : 4096;
        while (new_cap < b->len + n)
            new_cap *= 2;
        uint8_t *p = (uint8_t *)opj_jp3d_realloc(b->data, new_cap);
        if (!p)
            return 0;
        b->data = p;
        b->cap  = new_cap;
    }
    memcpy(b->data + b->len, src, n);
    b->len += n;
    return 1;
}

void opj_buf_free(opj_buf_t *b)
{
    opj_jp3d_free(b->data);
    b->data = NULL;
    b->len  = 0;
    b->cap  = 0;
}

/* =========================================================================
 * Write helpers
 * ========================================================================= */

void opj_cs3d_write_u8(opj_buf_t *b, uint8_t v)
{
    opj_buf_write(b, &v, 1);
}

void opj_cs3d_write_u16(opj_buf_t *b, uint16_t v)
{
    uint8_t tmp[2] = { (uint8_t)(v >> 8), (uint8_t)(v) };
    opj_buf_write(b, tmp, 2);
}

void opj_cs3d_write_u32(opj_buf_t *b, uint32_t v)
{
    uint8_t tmp[4] = {
        (uint8_t)(v >> 24), (uint8_t)(v >> 16),
        (uint8_t)(v >>  8), (uint8_t)(v)
    };
    opj_buf_write(b, tmp, 4);
}

void opj_cs3d_write_f32(opj_buf_t *b, float v)
{
    uint32_t bits;
    memcpy(&bits, &v, sizeof(bits));
    opj_cs3d_write_u32(b, bits);
}

/* =========================================================================
 * Read helpers
 * ========================================================================= */

uint8_t opj_cs3d_read_u8(const uint8_t *d, size_t *pos, size_t maxlen, int *err)
{
    if (*err || *pos + 1 > maxlen) { *err = 1; return 0; }
    return d[(*pos)++];
}

uint16_t opj_cs3d_read_u16(const uint8_t *d, size_t *pos, size_t maxlen, int *err)
{
    if (*err || *pos + 2 > maxlen) { *err = 1; return 0; }
    uint16_t v = ((uint16_t)d[*pos] << 8) | d[*pos + 1];
    *pos += 2;
    return v;
}

uint32_t opj_cs3d_read_u32(const uint8_t *d, size_t *pos, size_t maxlen, int *err)
{
    if (*err || *pos + 4 > maxlen) { *err = 1; return 0; }
    uint32_t v = ((uint32_t)d[*pos]     << 24) |
                 ((uint32_t)d[*pos + 1] << 16) |
                 ((uint32_t)d[*pos + 2] <<  8) |
                  (uint32_t)d[*pos + 3];
    *pos += 4;
    return v;
}

float opj_cs3d_read_f32(const uint8_t *d, size_t *pos, size_t maxlen, int *err)
{
    uint32_t bits = opj_cs3d_read_u32(d, pos, maxlen, err);
    float v;
    memcpy(&v, &bits, sizeof(v));
    return v;
}
