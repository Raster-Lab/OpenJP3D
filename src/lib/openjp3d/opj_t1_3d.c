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
 * @file opj_t1_3d.c
 * @brief Tier-1 raw bit-plane encoder/decoder for JP3D code-blocks.
 */

#include "opj_t1_3d.h"
#include "opj_mem.h"

#include <string.h>
#include <stdlib.h>

/* =========================================================================
 * Bit-buffer helpers
 * ========================================================================= */

typedef struct {
    uint8_t *buf;
    size_t   cap;      /* total capacity */
    size_t   byte_pos;
    int      bit_pos;  /* 7..0, next bit to write within current byte */
} opj_wbitbuf_t;

typedef struct {
    const uint8_t *buf;
    size_t         size;
    size_t         byte_pos;
    int            bit_pos; /* 7..0, next bit to read */
} opj_rbitbuf_t;

static void wbit_init(opj_wbitbuf_t *b, uint8_t *buf, size_t cap)
{
    b->buf      = buf;
    b->cap      = cap;
    b->byte_pos = 0;
    b->bit_pos  = 7;
    if (cap > 0)
        buf[0] = 0;
}

/** Write 1 bit.  Returns 0 on overflow. */
static int wbit_put(opj_wbitbuf_t *b, int bit)
{
    if (b->byte_pos >= b->cap)
        return 0;
    if (bit)
        b->buf[b->byte_pos] |= (uint8_t)(1u << b->bit_pos);
    if (b->bit_pos == 0) {
        b->bit_pos = 7;
        b->byte_pos++;
        if (b->byte_pos < b->cap)
            b->buf[b->byte_pos] = 0;
    } else {
        b->bit_pos--;
    }
    return 1;
}

/** Flush to byte boundary; returns total bytes used. */
static size_t wbit_flush(opj_wbitbuf_t *b)
{
    if (b->bit_pos != 7)
        b->byte_pos++;
    return b->byte_pos;
}

static void rbit_init(opj_rbitbuf_t *b, const uint8_t *buf, size_t size)
{
    b->buf      = buf;
    b->size     = size;
    b->byte_pos = 0;
    b->bit_pos  = 7;
}

/** Read 1 bit. Returns -1 on underflow. */
static int rbit_get(opj_rbitbuf_t *b)
{
    if (b->byte_pos >= b->size)
        return -1;
    int bit = (b->buf[b->byte_pos] >> b->bit_pos) & 1;
    if (b->bit_pos == 0) {
        b->bit_pos = 7;
        b->byte_pos++;
    } else {
        b->bit_pos--;
    }
    return bit;
}

/** Align read pointer to next byte boundary. */
static void rbit_align(opj_rbitbuf_t *b)
{
    if (b->bit_pos != 7) {
        b->bit_pos = 7;
        b->byte_pos++;
    }
}

/* =========================================================================
 * Encode
 * ========================================================================= */

size_t opj_t1_3d_encode_cblk(const int32_t *data,
                               uint32_t w, uint32_t h, uint32_t d,
                               uint8_t *out_buf, size_t out_cap)
{
    size_t nsamp = (size_t)w * h * d;

    /* Find max absolute value */
    int32_t max_abs = 0;
    for (size_t i = 0; i < nsamp; i++) {
        int32_t v = data[i];
        int32_t av = v < 0 ? -v : v;
        if (av > max_abs)
            max_abs = av;
    }

    /* Number of magnitude bit planes */
    int mbps = 0;
    if (max_abs > 0) {
        int32_t tmp = max_abs;
        while (tmp > 0) { mbps++; tmp >>= 1; }
    }

    /* Header: 1 + 4 + 4 + 4 = 13 bytes */
    size_t header_size = 13;
    if (out_cap < header_size)
        return 0;

    size_t pos = 0;
    out_buf[pos++] = (uint8_t)mbps;
    /* w big-endian */
    out_buf[pos++] = (uint8_t)(w >> 24);
    out_buf[pos++] = (uint8_t)(w >> 16);
    out_buf[pos++] = (uint8_t)(w >>  8);
    out_buf[pos++] = (uint8_t)(w      );
    /* h */
    out_buf[pos++] = (uint8_t)(h >> 24);
    out_buf[pos++] = (uint8_t)(h >> 16);
    out_buf[pos++] = (uint8_t)(h >>  8);
    out_buf[pos++] = (uint8_t)(h      );
    /* d */
    out_buf[pos++] = (uint8_t)(d >> 24);
    out_buf[pos++] = (uint8_t)(d >> 16);
    out_buf[pos++] = (uint8_t)(d >>  8);
    out_buf[pos++] = (uint8_t)(d      );

    if (mbps > 0) {
        opj_wbitbuf_t wb;
        wbit_init(&wb, out_buf + pos, out_cap - pos);

        /* Magnitude bit planes: MSB to LSB */
        for (int p = mbps - 1; p >= 0; p--) {
            for (size_t i = 0; i < nsamp; i++) {
                int32_t av = data[i] < 0 ? -data[i] : data[i];
                int bit = (av >> p) & 1;
                if (!wbit_put(&wb, bit))
                    return 0;
            }
            /* Pad to byte boundary after each plane */
            /* (we do a single flush at the end of all planes) */
        }
        pos += wbit_flush(&wb);

        /* Sign bits for non-zero samples */
        opj_wbitbuf_t ws;
        wbit_init(&ws, out_buf + pos, out_cap - pos);
        for (size_t i = 0; i < nsamp; i++) {
            if (data[i] != 0) {
                int sign = (data[i] < 0) ? 1 : 0;
                if (!wbit_put(&ws, sign))
                    return 0;
            }
        }
        pos += wbit_flush(&ws);
    }

    return pos;
}

/* =========================================================================
 * Decode
 * ========================================================================= */

int opj_t1_3d_decode_cblk(const uint8_t *in_buf, size_t in_size,
                            int32_t *data,
                            uint32_t w, uint32_t h, uint32_t d)
{
    if (in_size < 13)
        return 0;

    size_t pos = 0;
    int mbps = in_buf[pos++];

    uint32_t rw = ((uint32_t)in_buf[pos] << 24) | ((uint32_t)in_buf[pos+1] << 16) |
                  ((uint32_t)in_buf[pos+2] << 8) | (uint32_t)in_buf[pos+3]; pos += 4;
    uint32_t rh = ((uint32_t)in_buf[pos] << 24) | ((uint32_t)in_buf[pos+1] << 16) |
                  ((uint32_t)in_buf[pos+2] << 8) | (uint32_t)in_buf[pos+3]; pos += 4;
    uint32_t rd = ((uint32_t)in_buf[pos] << 24) | ((uint32_t)in_buf[pos+1] << 16) |
                  ((uint32_t)in_buf[pos+2] << 8) | (uint32_t)in_buf[pos+3]; pos += 4;

    if (rw != w || rh != h || rd != d)
        return 0;

    size_t nsamp = (size_t)w * h * d;
    memset(data, 0, nsamp * sizeof(int32_t));

    if (mbps > 0) {
        opj_rbitbuf_t rb;
        rbit_init(&rb, in_buf + pos, in_size - pos);

        /* Read magnitude bit planes */
        /* Allocate magnitude array */
        int32_t *mag = (int32_t *)opj_jp3d_calloc(nsamp, sizeof(int32_t));
        if (!mag)
            return 0;

        for (int p = mbps - 1; p >= 0; p--) {
            for (size_t i = 0; i < nsamp; i++) {
                int bit = rbit_get(&rb);
                if (bit < 0) { opj_jp3d_free(mag); return 0; }
                mag[i] |= (int32_t)(bit << p);
            }
        }

        /* Align to byte boundary */
        rbit_align(&rb);
        pos += rb.byte_pos;

        /* Read sign bits */
        opj_rbitbuf_t rs;
        rbit_init(&rs, in_buf + pos, in_size - pos);
        for (size_t i = 0; i < nsamp; i++) {
            if (mag[i] != 0) {
                int sign = rbit_get(&rs);
                if (sign < 0) { opj_jp3d_free(mag); return 0; }
                data[i] = sign ? -mag[i] : mag[i];
            } else {
                data[i] = 0;
            }
        }

        opj_jp3d_free(mag);
    }

    return 1;
}
