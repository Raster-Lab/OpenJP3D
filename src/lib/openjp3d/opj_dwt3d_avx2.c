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
 * @file opj_dwt3d_avx2.c
 * @brief AVX2-optimised 5/3 integer lifting DWT for OpenJP3D.
 *
 * Provides opj_dwt53_fwd_1d_avx2() and opj_dwt53_inv_1d_avx2(), compiled
 * with -mavx2.  Processes 8 int32 samples per SIMD word (256-bit), falling
 * back to the SSE4.1 path for the tail or to scalar for very short rows.
 * Output is bit-identical to the scalar reference.
 *
 * De-interleaving strategy (8 pairs = 16 elements):
 *   Load lo128 = [e0,o0,e1,o1,e2,o2,e3,o3]  (256-bit view: two 128-bit halves)
 *   The 256-bit AVX2 shuffle operates within 128-bit lanes, so we use:
 *     permutevar8x32 with index [0,2,4,6, 1,3,5,7] to de-interleave.
 */

#ifdef OPJ_JP3D_HAVE_AVX2

#include "opj_dwt3d_simd.h"
#include <immintrin.h>
#include <string.h>

/* =========================================================================
 * De-interleave / re-interleave helpers (AVX2, 8 pairs)
 * =========================================================================
 *
 * AVX2 approach: use _mm256_permutevar8x32_epi32 with a fixed index vector
 * that separates even and odd lanes across both 128-bit halves.
 *
 *   Input  x[0..15]: e0 o0 e1 o1 e2 o2 e3 o3 | e4 o4 e5 o5 e6 o6 e7 o7
 *   idx_even = [0,2,4,6, 8,10,12,14] → evens = e0 e1 e2 e3 e4 e5 e6 e7
 *   idx_odd  = [1,3,5,7, 9,11,13,15] → odds  = o0 o1 o2 o3 o4 o5 o6 o7
 *
 * Re-interleave: use two 256-bit unpack + permute steps.
 *   unpacklo_epi32(lo, hi) = e0 o0 e1 o1 | e4 o4 e5 o5  (within each lane)
 *   unpackhi_epi32(lo, hi) = e2 o2 e3 o3 | e6 o6 e7 o7
 *   permute4x64_epi64 to reorder 64-bit chunks to sequential order.
 * ========================================================================= */

/* Index vector for de-interleave: select even/odd lanes from 16-element input.
 * We load two 256-bit registers (16 elements) and process separately.        */

void opj_dwt53_fwd_1d_avx2(int32_t *x, uint32_t n, int32_t *scratch)
{
    if (n <= 1)
        return;

    const uint32_t sn = (n + 1) / 2;
    const uint32_t dn = n / 2;
    int32_t * const low  = scratch;
    int32_t * const high = scratch + sn;
    uint32_t i;

    /* Index vectors for de-interleave: even positions 0,2,4,6 and odd 1,3,5,7
     * within a single 256-bit register holding 8 int32 = 4 pairs.            */
    const __m256i idx_even = _mm256_set_epi32(6, 4, 2, 0, 6, 4, 2, 0);
    const __m256i idx_odd  = _mm256_set_epi32(7, 5, 3, 1, 7, 5, 3, 1);
    /* For pairs4: process 8 pairs (16 elements) per iteration — two AVX2 loads.
     * Each AVX2 register holds 8 int32 = 4 pairs; process two registers.      */

    /* ------------------------------------------------------------------
     * Step 1: de-interleave using AVX2 (8 pairs per iteration).
     * ------------------------------------------------------------------ */
    {
        const uint32_t pairs8 = (n / 16) * 8; /* # of complete 8-pair blocks */
        for (i = 0; i < pairs8; i += 8) {
            /* Load 16 elements (8 pairs) as two 256-bit registers */
            __m256i v0 = _mm256_loadu_si256((const __m256i *)(x + 2 * i));
            __m256i v1 = _mm256_loadu_si256((const __m256i *)(x + 2 * i + 8));
            /* De-interleave v0 → e0..e3 (low 128) and o0..o3 (high 128) */
            __m256i e0 = _mm256_permutevar8x32_epi32(v0, idx_even); /* e0-e3 in both halves */
            __m256i o0 = _mm256_permutevar8x32_epi32(v0, idx_odd);  /* o0-o3 in both halves */
            __m256i e1 = _mm256_permutevar8x32_epi32(v1, idx_even);
            __m256i o1 = _mm256_permutevar8x32_epi32(v1, idx_odd);
            /* Pack: take low 128 bits from e0 and low 128 bits from e1 → e0..e7 */
            __m256i evens = _mm256_permute2x128_si256(e0, e1, 0x20); /* low of e0, low of e1 */
            __m256i odds  = _mm256_permute2x128_si256(o0, o1, 0x20);
            _mm256_storeu_si256((__m256i *)(low  + i), evens);
            _mm256_storeu_si256((__m256i *)(high + i), odds);
        }
        /* tail: scalar */
        for (i = (n / 16) * 8; i < sn; i++) low[i]  = x[2 * i];
        for (i = (n / 16) * 8; i < dn; i++) high[i] = x[2 * i + 1];
    }

    /* ------------------------------------------------------------------
     * Step 2: Predict  high[i] -= (low[i] + low[i+1]) >> 1
     * ------------------------------------------------------------------ */
    if (dn > 0) {
        const uint32_t vend = (dn >= 2) ? ((dn - 1) / 8) * 8 : 0;
        for (i = 0; i < vend; i += 8) {
            __m256i ei   = _mm256_loadu_si256((const __m256i *)(low + i));
            __m256i eip1 = _mm256_loadu_si256((const __m256i *)(low + i + 1));
            __m256i oi   = _mm256_loadu_si256((const __m256i *)(high + i));
            __m256i pred = _mm256_srai_epi32(_mm256_add_epi32(ei, eip1), 1);
            _mm256_storeu_si256((__m256i *)(high + i),
                                _mm256_sub_epi32(oi, pred));
        }
        for (i = vend; i < dn; i++) {
            const int32_t right = (2 * (i + 1) < n) ? low[i + 1] : low[sn - 1];
            high[i] = high[i] - ((low[i] + right) >> 1);
        }
    }

    /* ------------------------------------------------------------------
     * Step 3: Update  low[i] += (high[i-1] + high[i] + 2) >> 2
     * ------------------------------------------------------------------ */
    /* i = 0 */
    {
        const int32_t right = (dn > 0) ? high[0] : 0;
        low[0] += (high[0] + right + 2) >> 2;
    }
    if (dn >= 2) {
        for (i = 1; i + 7 < dn; i += 8) {
            __m256i hm1 = _mm256_loadu_si256((const __m256i *)(high + i - 1));
            __m256i hi  = _mm256_loadu_si256((const __m256i *)(high + i));
            __m256i li  = _mm256_loadu_si256((const __m256i *)(low  + i));
            __m256i sum = _mm256_add_epi32(_mm256_add_epi32(hm1, hi),
                                           _mm256_set1_epi32(2));
            _mm256_storeu_si256((__m256i *)(low + i),
                                _mm256_add_epi32(li, _mm256_srai_epi32(sum, 2)));
        }
        for (i = 1 + ((dn - 1) / 8) * 8; i < sn; i++) {
            const int32_t left  = high[i - 1];
            const int32_t right = (i < dn) ? high[i] : high[dn - 1];
            low[i] += (left + right + 2) >> 2;
        }
    } else if (sn > 1) {
        low[1] += (high[0] + high[0] + 2) >> 2;
    }

    memcpy(x,       low,  sn * sizeof(int32_t));
    memcpy(x + sn,  high, dn * sizeof(int32_t));

    _mm256_zeroupper();
}

/* =========================================================================
 * Inverse 5/3 1-D (AVX2)
 * ========================================================================= */

void opj_dwt53_inv_1d_avx2(int32_t *x, uint32_t n, int32_t *scratch)
{
    if (n <= 1)
        return;

    const uint32_t sn = (n + 1) / 2;
    const uint32_t dn = n / 2;
    int32_t * const low  = scratch;
    int32_t * const high = scratch + sn;
    uint32_t i;

    memcpy(low,  x,      sn * sizeof(int32_t));
    memcpy(high, x + sn, dn * sizeof(int32_t));

    /* ------------------------------------------------------------------
     * Inverse update  low[i] -= (high[i-1] + high[i] + 2) >> 2
     * ------------------------------------------------------------------ */
    {
        const int32_t right = (dn > 0) ? high[0] : 0;
        low[0] -= (high[0] + right + 2) >> 2;
    }
    if (dn >= 2) {
        for (i = 1; i + 7 < dn; i += 8) {
            __m256i hm1 = _mm256_loadu_si256((const __m256i *)(high + i - 1));
            __m256i hi  = _mm256_loadu_si256((const __m256i *)(high + i));
            __m256i li  = _mm256_loadu_si256((const __m256i *)(low  + i));
            __m256i sum = _mm256_add_epi32(_mm256_add_epi32(hm1, hi),
                                           _mm256_set1_epi32(2));
            _mm256_storeu_si256((__m256i *)(low + i),
                                _mm256_sub_epi32(li, _mm256_srai_epi32(sum, 2)));
        }
        for (i = 1 + ((dn - 1) / 8) * 8; i < sn; i++) {
            const int32_t left  = high[i - 1];
            const int32_t right = (i < dn) ? high[i] : high[dn - 1];
            low[i] -= (left + right + 2) >> 2;
        }
    } else if (sn > 1) {
        low[1] -= (high[0] + high[0] + 2) >> 2;
    }

    /* ------------------------------------------------------------------
     * Inverse predict: scatter low[i] and high_out[i] back to x.
     *   x[2i]   = low[i]
     *   x[2i+1] = high[i] + (low[i] + low[i+1]) >> 1
     *
     * Re-interleave 8 pairs using AVX2:
     *   unpacklo/hi_epi32 within 128-bit halves, then permute.
     * ------------------------------------------------------------------ */
    if (dn > 0) {
        const uint32_t vend = (dn >= 2) ? ((dn - 1) / 8) * 8 : 0;
        for (i = 0; i < vend; i += 8) {
            __m256i li   = _mm256_loadu_si256((const __m256i *)(low  + i));
            __m256i lip1 = _mm256_loadu_si256((const __m256i *)(low  + i + 1));
            __m256i hi   = _mm256_loadu_si256((const __m256i *)(high + i));
            __m256i hout = _mm256_add_epi32(
                               hi,
                               _mm256_srai_epi32(_mm256_add_epi32(li, lip1), 1));
            /* Re-interleave: pairs 0-3 */
            __m256i lo128 = _mm256_permute2x128_si256(li,   hout, 0x20); /* low(li)|low(hout) */
            __m256i hi128 = _mm256_permute2x128_si256(li,   hout, 0x31); /* hi(li) |hi(hout)  */
            __m128i li_lo = _mm256_castsi256_si128(lo128);
            __m128i ho_lo = _mm256_extracti128_si256(lo128, 1);
            __m128i li_hi = _mm256_castsi256_si128(hi128);
            __m128i ho_hi = _mm256_extracti128_si256(hi128, 1);
            /* pairs 0-3: unpack lo-half li with lo-half hout */
            _mm_storeu_si128((__m128i *)(x + 2 * i),
                             _mm_unpacklo_epi32(li_lo, ho_lo));
            _mm_storeu_si128((__m128i *)(x + 2 * i + 4),
                             _mm_unpackhi_epi32(li_lo, ho_lo));
            /* pairs 4-7 */
            _mm_storeu_si128((__m128i *)(x + 2 * i + 8),
                             _mm_unpacklo_epi32(li_hi, ho_hi));
            _mm_storeu_si128((__m128i *)(x + 2 * i + 12),
                             _mm_unpackhi_epi32(li_hi, ho_hi));
        }
        for (i = vend; i < dn; i++) {
            const int32_t right = (i + 1 < sn) ? low[i + 1] : low[sn - 1];
            x[2 * i]     = low[i];
            x[2 * i + 1] = high[i] + ((low[i] + right) >> 1);
        }
    }

    if (n & 1)
        x[n - 1] = low[sn - 1];

    _mm256_zeroupper();
}

#endif /* OPJ_JP3D_HAVE_AVX2 */
