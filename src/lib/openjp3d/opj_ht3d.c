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
 * @file opj_ht3d.c
 * @brief High-Throughput (HTJ2K-style) block coder for JP3D 3-D code-blocks.
 *
 * Implements MEL (Minimum Entropy Length) entropy coding for the significance
 * stream and Exp-Golomb / MagSgn variable-length coding for coefficient
 * magnitudes and signs.  Both the encoder and decoder operate on a single
 * forward pass, which is the key source of speedup over multi-pass EBCOT.
 */

#include "opj_ht3d.h"
#include "opj_mem.h"

#include <string.h>
#include <stdlib.h>

/* =========================================================================
 * MEL state table
 *
 * K[state] determines the run-length threshold 2^K[state].  The state
 * machine adapts: long runs of zeros push the state up (larger threshold,
 * fewer MEL bits emitted); frequent 1s push the state down.
 * ========================================================================= */

static const int mel_k_table[9] = { 0, 0, 0, 1, 1, 2, 2, 3, 3 };

/* =========================================================================
 * Bit-buffer helpers (shared by MEL and MagSgn streams)
 * ========================================================================= */

typedef struct {
    uint8_t *buf;
    size_t   cap;
    size_t   pos;      /* next byte index to write */
    uint8_t  cur;      /* partial byte accumulator (MSB first) */
    int      bits;     /* bits already accumulated in cur (0..7) */
} wbb_t;   /* write bit-buffer */

typedef struct {
    const uint8_t *buf;
    size_t         size;
    size_t         pos;
    uint8_t        cur;
    int            bits; /* bits remaining in cur (0..7) */
} rbb_t;   /* read bit-buffer */

static void wbb_init(wbb_t *b, uint8_t *buf, size_t cap)
{
    b->buf  = buf;
    b->cap  = cap;
    b->pos  = 0;
    b->cur  = 0;
    b->bits = 0;
}

/** Write one bit MSB-first.  Returns 0 on buffer overflow. */
static int wbb_put(wbb_t *b, int bit)
{
    b->cur = (uint8_t)((b->cur << 1) | (bit & 1));
    b->bits++;
    if (b->bits == 8) {
        if (b->pos >= b->cap)
            return 0;
        b->buf[b->pos++] = b->cur;
        b->cur  = 0;
        b->bits = 0;
    }
    return 1;
}

/** Flush partial byte (zero-pad LSBs); returns total bytes written. */
static size_t wbb_flush(wbb_t *b)
{
    if (b->bits > 0) {
        if (b->pos < b->cap)
            b->buf[b->pos++] = (uint8_t)(b->cur << (8 - b->bits));
        b->bits = 0;
        b->cur  = 0;
    }
    return b->pos;
}

static void rbb_init(rbb_t *b, const uint8_t *buf, size_t size)
{
    b->buf  = buf;
    b->size = size;
    b->pos  = 0;
    b->cur  = 0;
    b->bits = 0;
}

/**
 * Read one bit MSB-first.  Returns 0 (not -1) on underflow so that
 * decoding short-circuits gracefully to all-zero output.
 */
static int rbb_get(rbb_t *b)
{
    if (b->bits == 0) {
        if (b->pos >= b->size)
            return 0;
        b->cur  = b->buf[b->pos++];
        b->bits = 8;
    }
    int bit = (b->cur >> 7) & 1;
    b->cur  = (uint8_t)(b->cur << 1);
    b->bits--;
    return bit;
}

/* =========================================================================
 * MEL encoder
 * ========================================================================= */

typedef struct {
    wbb_t wb;    /* underlying bit buffer */
    int   state; /* current MEL state (0..8) */
    int   run;   /* accumulated zero-run length */
} mel_enc_t;

static void mel_enc_init(mel_enc_t *e, uint8_t *buf, size_t cap)
{
    wbb_init(&e->wb, buf, cap);
    e->state = 0;
    e->run   = 0;
}

/**
 * Encode one significance symbol (0 = not significant, 1 = significant).
 * Returns 0 on buffer overflow.
 */
static int mel_enc_sym(mel_enc_t *e, int sig)
{
    if (!sig) {
        e->run++;
        if (e->run == (1 << mel_k_table[e->state])) {
            /* Run reached threshold: emit '1', advance state. */
            if (!wbb_put(&e->wb, 1))
                return 0;
            e->run = 0;
            if (e->state < 8)
                e->state++;
        }
    } else {
        /* Significance event: emit '0' + run in K[state] bits. */
        if (!wbb_put(&e->wb, 0))
            return 0;
        for (int b = mel_k_table[e->state] - 1; b >= 0; b--) {
            if (!wbb_put(&e->wb, (e->run >> b) & 1))
                return 0;
        }
        e->run = 0;
        if (e->state > 0)
            e->state--;
    }
    return 1;
}

/**
 * Flush trailing partial run and byte-align.
 * Returns total MEL bytes written.
 */
static size_t mel_enc_flush(mel_enc_t *e)
{
    /* Emit the remaining partial run (if any) as a short-run event
     * without a trailing '1'; the decoder will stop after N symbols
     * so the phantom significance event is never consumed. */
    if (e->run > 0) {
        (void)wbb_put(&e->wb, 0);
        for (int b = mel_k_table[e->state] - 1; b >= 0; b--)
            (void)wbb_put(&e->wb, (e->run >> b) & 1);
    }
    return wbb_flush(&e->wb);
}

/* =========================================================================
 * MEL decoder
 * ========================================================================= */

typedef struct {
    rbb_t rb;         /* underlying bit buffer */
    int   state;      /* current MEL state */
    int   zeros_left; /* buffered zero symbols to emit */
    int   pending1;   /* whether a '1' follows zeros_left */
} mel_dec_t;

static void mel_dec_init(mel_dec_t *d, const uint8_t *buf, size_t size)
{
    rbb_init(&d->rb, buf, size);
    d->state     = 0;
    d->zeros_left = 0;
    d->pending1  = 0;
}

/** Decode one significance symbol. */
static int mel_dec_sym(mel_dec_t *d)
{
    /* Return buffered zero first. */
    if (d->zeros_left > 0) {
        d->zeros_left--;
        return 0;
    }
    /* Return buffered significance event. */
    if (d->pending1) {
        d->pending1 = 0;
        return 1;
    }

    /* Read next MEL token. */
    int bit = rbb_get(&d->rb);

    if (bit == 1) {
        /* Full run: 2^K[state] zeros, no trailing significance event. */
        int run = 1 << mel_k_table[d->state];
        if (d->state < 8)
            d->state++;
        /* Emit first zero now; buffer the rest. */
        d->zeros_left = run - 1;
        return 0;
    } else {
        /* Short run: read K[state] bits, then a significance event follows. */
        int k   = mel_k_table[d->state];
        int run = 0;
        for (int b = k - 1; b >= 0; b--)
            run = (run << 1) | rbb_get(&d->rb);
        if (d->state > 0)
            d->state--;
        if (run > 0) {
            d->zeros_left = run - 1;
            d->pending1   = 1;
            return 0;
        } else {
            return 1; /* immediate significance event */
        }
    }
}

/* =========================================================================
 * MagSgn (magnitude + sign) stream — Exp-Golomb order-0 coding
 *
 * Encoding of a significant coefficient coeff (coeff != 0):
 *   m    = |coeff| - 1        (>= 0)
 *   n    = floor(log2(m+1))   (0 for m==0)
 *   prefix: n zero bits followed by one '1' bit
 *   data:   low n bits of m
 *   sign:   (coeff < 0) ? 1 : 0
 *
 * Total bits per coefficient: 2n + 2  (for m >= 1); 2 bits for m==0 (|c|==1)
 * ========================================================================= */

/** Compute floor(log2(v)) for v >= 1. */
static int ilog2(uint32_t v)
{
    int n = 0;
    while (v > 1) { v >>= 1; n++; }
    return n;
}

static int magsign_encode(wbb_t *wb, int32_t coeff)
{
    uint32_t mag  = (uint32_t)(coeff < 0 ? -coeff : coeff); /* >= 1 */
    int      sign = (coeff < 0) ? 1 : 0;
    /* Exp-Golomb order-0 of (mag-1): encode mag as k zeros + mag in (k+1) bits,
     * where k = floor(log2(mag)). */
    int      k    = ilog2(mag); /* 0 for mag==1, 1 for mag==2..3, etc. */

    /* Write k leading zeros */
    for (int i = 0; i < k; i++)
        if (!wbb_put(wb, 0)) return 0;

    /* Write mag in (k+1) bits, MSB first (the leading '1' is explicit here) */
    for (int b = k; b >= 0; b--)
        if (!wbb_put(wb, (mag >> b) & 1)) return 0;

    /* Sign */
    if (!wbb_put(wb, sign)) return 0;
    return 1;
}

static int32_t magsign_decode(rbb_t *rb)
{
    /* Count leading zeros (k) — the while loop consumes the trailing '1'. */
    int k = 0;
    while (rbb_get(rb) == 0)
        k++;

    /* Reconstruct mag: the leading '1' (consumed above) is at bit k. */
    uint32_t mag = 1u << k;
    for (int b = k - 1; b >= 0; b--)
        mag |= (uint32_t)((unsigned)rbb_get(rb) << b);

    int sign = rbb_get(rb);
    return sign ? -(int32_t)mag : (int32_t)mag;
}

/* =========================================================================
 * Public encode function
 * ========================================================================= */

/* Conservative upper bound on encoded block size.
 * Header(18) + MEL worst-case + MagSgn worst-case. */
static size_t ht_encode_bound(uint32_t w, uint32_t h, uint32_t d)
{
    size_t nsamp = (size_t)w * h * d;
    /* MEL: at most 2 bits per symbol + alignment */
    size_t mel_max = (nsamp * 2 + 7) / 8 + 2;
    /* MagSgn: Exp-Golomb for 31-bit magnitude = up to 2*31+2 = 64 bits per
     * sample = 8 bytes per sample worst case */
    size_t ms_max  = nsamp * 8;
    return 18 + mel_max + ms_max;
}

size_t opj_ht3d_encode_cblk(const int32_t *data,
                              uint32_t w, uint32_t h, uint32_t d,
                              uint8_t *out_buf, size_t out_cap)
{
    size_t nsamp = (size_t)w * h * d;

    /* Sanity check — must have enough room for the full output */
    if (out_cap < ht_encode_bound(w, h, d))
        return 0;

    /* --- Find number of active bit planes (mbps) --- */
    int32_t max_abs = 0;
    for (size_t i = 0; i < nsamp; i++) {
        int32_t av = data[i] < 0 ? -data[i] : data[i];
        if (av > max_abs)
            max_abs = av;
    }
    int mbps = 0;
    if (max_abs > 0) {
        int32_t tmp = max_abs;
        while (tmp > 0) { mbps++; tmp >>= 1; }
    }

    /* --- Write header --- */
    size_t pos = 0;
    out_buf[pos++] = 0x48u; /* magic 'H' — identifies HT block */
    out_buf[pos++] = (uint8_t)mbps;
    /* w big-endian */
    out_buf[pos++] = (uint8_t)(w >> 24);
    out_buf[pos++] = (uint8_t)(w >> 16);
    out_buf[pos++] = (uint8_t)(w >>  8);
    out_buf[pos++] = (uint8_t) w;
    /* h big-endian */
    out_buf[pos++] = (uint8_t)(h >> 24);
    out_buf[pos++] = (uint8_t)(h >> 16);
    out_buf[pos++] = (uint8_t)(h >>  8);
    out_buf[pos++] = (uint8_t) h;
    /* d big-endian */
    out_buf[pos++] = (uint8_t)(d >> 24);
    out_buf[pos++] = (uint8_t)(d >> 16);
    out_buf[pos++] = (uint8_t)(d >>  8);
    out_buf[pos++] = (uint8_t) d;

    /* Reserve 4 bytes for mel_len (written after MEL encoding). */
    size_t mel_len_pos = pos;
    pos += 4;

    /* --- All-zero block fast path --- */
    if (mbps == 0) {
        /* mel_len = 0 */
        out_buf[mel_len_pos    ] = 0;
        out_buf[mel_len_pos + 1] = 0;
        out_buf[mel_len_pos + 2] = 0;
        out_buf[mel_len_pos + 3] = 0;
        return pos;
    }

    /* --- MEL pass: encode significance stream --- */
    mel_enc_t mel;
    mel_enc_init(&mel, out_buf + pos, out_cap - pos);

    for (size_t i = 0; i < nsamp; i++) {
        int sig = (data[i] != 0) ? 1 : 0;
        if (!mel_enc_sym(&mel, sig))
            return 0;
    }
    size_t mel_bytes = mel_enc_flush(&mel);
    pos += mel_bytes;

    /* Write mel_len field */
    out_buf[mel_len_pos    ] = (uint8_t)(mel_bytes >> 24);
    out_buf[mel_len_pos + 1] = (uint8_t)(mel_bytes >> 16);
    out_buf[mel_len_pos + 2] = (uint8_t)(mel_bytes >>  8);
    out_buf[mel_len_pos + 3] = (uint8_t) mel_bytes;

    /* --- MagSgn pass: encode non-zero coefficients --- */
    if (out_cap < pos)
        return 0;
    wbb_t ms;
    wbb_init(&ms, out_buf + pos, out_cap - pos);

    for (size_t i = 0; i < nsamp; i++) {
        if (data[i] != 0) {
            if (!magsign_encode(&ms, data[i]))
                return 0;
        }
    }
    pos += wbb_flush(&ms);

    return pos;
}

/* =========================================================================
 * Public decode function
 * ========================================================================= */

int opj_ht3d_decode_cblk(const uint8_t *in_buf, size_t in_size,
                           int32_t *data,
                           uint32_t w, uint32_t h, uint32_t d)
{
    /* Minimum header size */
    if (in_size < 18)
        return 0;

    size_t pos = 0;

    /* Magic */
    if (in_buf[pos++] != 0x48u)
        return 0;

    /* mbps */
    int mbps = (int)in_buf[pos++];

    /* Dimensions */
    uint32_t rw = ((uint32_t)in_buf[pos  ] << 24) | ((uint32_t)in_buf[pos+1] << 16)
                | ((uint32_t)in_buf[pos+2] <<  8) |  (uint32_t)in_buf[pos+3];
    pos += 4;
    uint32_t rh = ((uint32_t)in_buf[pos  ] << 24) | ((uint32_t)in_buf[pos+1] << 16)
                | ((uint32_t)in_buf[pos+2] <<  8) |  (uint32_t)in_buf[pos+3];
    pos += 4;
    uint32_t rd = ((uint32_t)in_buf[pos  ] << 24) | ((uint32_t)in_buf[pos+1] << 16)
                | ((uint32_t)in_buf[pos+2] <<  8) |  (uint32_t)in_buf[pos+3];
    pos += 4;

    if (rw != w || rh != h || rd != d)
        return 0;

    /* mel_len */
    if (pos + 4 > in_size)
        return 0;
    uint32_t mel_len = ((uint32_t)in_buf[pos  ] << 24) | ((uint32_t)in_buf[pos+1] << 16)
                     | ((uint32_t)in_buf[pos+2] <<  8) |  (uint32_t)in_buf[pos+3];
    pos += 4;

    size_t nsamp = (size_t)w * h * d;
    memset(data, 0, nsamp * sizeof(int32_t));

    /* All-zero fast path */
    if (mbps == 0)
        return 1;

    if (pos + mel_len > in_size)
        return 0;

    /* --- MEL decode: recover significance stream --- */
    mel_dec_t mel;
    mel_dec_init(&mel, in_buf + pos, mel_len);
    pos += mel_len;

    /* Collect significance flags */
    uint8_t *sig_flags = (uint8_t *)opj_jp3d_calloc(nsamp, 1);
    if (!sig_flags)
        return 0;

    for (size_t i = 0; i < nsamp; i++)
        sig_flags[i] = (uint8_t)mel_dec_sym(&mel);

    /* --- MagSgn decode: recover coefficients --- */
    rbb_t ms;
    rbb_init(&ms, in_buf + pos, in_size - pos);

    for (size_t i = 0; i < nsamp; i++) {
        if (sig_flags[i])
            data[i] = magsign_decode(&ms);
    }

    opj_jp3d_free(sig_flags);
    return 1;
}
