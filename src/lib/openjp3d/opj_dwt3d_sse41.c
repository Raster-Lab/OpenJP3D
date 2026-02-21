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
 * @file opj_dwt3d_sse41.c
 * @brief SSE4.1-optimised 5/3 integer lifting DWT for OpenJP3D.
 *
 * Provides opj_dwt53_fwd_1d_sse41() and opj_dwt53_inv_1d_sse41().  Both
 * are compiled with -msse4.1 (set by CMake) and guarded by the
 * OPJ_JP3D_HAVE_SSE41 macro.  Output is bit-identical to the scalar
 * reference (dwt53_fwd_1d / dwt53_inv_1d in opj_dwt3d.c).
 *
 * Algorithm overview (forward transform):
 *  1. De-interleave x[] into low[] (even samples) and high[] (odd samples)
 *     using SSE4.1 shuffle + unpack — 4 pairs per SIMD word.
 *  2. Vectorised predict step: high[i] -= (low[i] + low[i+1]) >> 1
 *     (interior elements; boundary handled with scalar code).
 *  3. Vectorised update  step: low[i]  += (high[i-1] + high[i] + 2) >> 2
 *     (interior elements; boundary handled with scalar code).
 *  4. memcpy low[] then high[] back to x[].
 */

#ifdef OPJ_JP3D_HAVE_SSE41

#include "opj_dwt3d_simd.h"
#include <smmintrin.h>
#include <string.h>

/* =========================================================================
 * Forward 5/3 1-D (SSE4.1)
 * ========================================================================= */

void opj_dwt53_fwd_1d_sse41(int32_t *x, uint32_t n, int32_t *scratch)
{
    if (n <= 1)
        return;

    const uint32_t sn = (n + 1) / 2;
    const uint32_t dn = n / 2;
    int32_t * const low  = scratch;
    int32_t * const high = scratch + sn;
    uint32_t i;

    /* ------------------------------------------------------------------
     * Step 1: de-interleave x[] into low[] (even) and high[] (odd).
     * Process 8 input elements (4 pairs) per SSE4.1 iteration.
     *
     *  v0 = [e0, o0, e1, o1]
     *  v1 = [e2, o2, e3, o3]
     *  shuffle each with _MM_SHUFFLE(3,1,2,0):
     *    d0 = [e0, e1, o0, o1]
     *    d1 = [e2, e3, o2, o3]
     *  evens = unpacklo_epi64(d0,d1) = [e0,e1,e2,e3]
     *  odds  = unpackhi_epi64(d0,d1) = [o0,o1,o2,o3]
     * ------------------------------------------------------------------ */
    {
        const uint32_t pairs4 = (n / 8) * 4;
        for (i = 0; i < pairs4; i += 4) {
            __m128i v0 = _mm_loadu_si128((const __m128i *)(x + 2 * i));
            __m128i v1 = _mm_loadu_si128((const __m128i *)(x + 2 * i + 4));
            __m128i d0 = _mm_shuffle_epi32(v0, _MM_SHUFFLE(3, 1, 2, 0));
            __m128i d1 = _mm_shuffle_epi32(v1, _MM_SHUFFLE(3, 1, 2, 0));
            _mm_storeu_si128((__m128i *)(low  + i), _mm_unpacklo_epi64(d0, d1));
            _mm_storeu_si128((__m128i *)(high + i), _mm_unpackhi_epi64(d0, d1));
        }
        for (i = (n / 8) * 4; i < sn; i++) low[i]  = x[2 * i];
        for (i = (n / 8) * 4; i < dn; i++) high[i] = x[2 * i + 1];
    }

    /* ------------------------------------------------------------------
     * Step 2: Predict  high[i] -= (low[i] + low[i+1]) >> 1
     * Vectorise interior [0, dn-1) — the last element needs the
     * boundary-mirrored even sample and is handled in scalar.
     * ------------------------------------------------------------------ */
    if (dn > 0) {
        const uint32_t vend = (dn >= 2) ? ((dn - 1) / 4) * 4 : 0;
        for (i = 0; i < vend; i += 4) {
            __m128i ei   = _mm_loadu_si128((const __m128i *)(low + i));
            __m128i eip1 = _mm_loadu_si128((const __m128i *)(low + i + 1));
            __m128i oi   = _mm_loadu_si128((const __m128i *)(high + i));
            __m128i pred = _mm_srai_epi32(_mm_add_epi32(ei, eip1), 1);
            _mm_storeu_si128((__m128i *)(high + i), _mm_sub_epi32(oi, pred));
        }
        /* scalar remainder + boundary element */
        for (i = vend; i < dn; i++) {
            const int32_t right = (2 * (i + 1) < n) ? low[i + 1] : low[sn - 1];
            high[i] = high[i] - ((low[i] + right) >> 1);
        }
    }

    /* ------------------------------------------------------------------
     * Step 3: Update  low[i] += (high[i-1] + high[i] + 2) >> 2
     * Boundaries:  i = 0   → left  = high[0]  (mirror)
     *              i >= dn → right = high[dn-1] (mirror)
     * ------------------------------------------------------------------ */
    /* i = 0 (scalar boundary) */
    {
        const int32_t right = (dn > 0) ? high[0] : 0;
        low[0] += (high[0] + right + 2) >> 2;
    }
    /* interior [1, dn) vectorised */
    if (dn >= 2) {
        const uint32_t vend = 1 + ((dn - 1) / 4) * 4;
        for (i = 1; i + 3 < dn; i += 4) {
            __m128i hm1 = _mm_loadu_si128((const __m128i *)(high + i - 1));
            __m128i hi  = _mm_loadu_si128((const __m128i *)(high + i));
            __m128i li  = _mm_loadu_si128((const __m128i *)(low  + i));
            __m128i sum = _mm_add_epi32(_mm_add_epi32(hm1, hi),
                                        _mm_set1_epi32(2));
            _mm_storeu_si128((__m128i *)(low + i),
                             _mm_add_epi32(li, _mm_srai_epi32(sum, 2)));
        }
        /* scalar remainder: starts at the first i that wasn't covered above */
        for (i = (dn >= 2 ? 1 + ((dn - 1) / 4) * 4 : 1); i < sn; i++) {
            const int32_t left  = high[i - 1];
            const int32_t right = (i < dn) ? high[i] : high[dn - 1];
            low[i] += (left + right + 2) >> 2;
        }
        (void)vend;
    } else if (sn > 1) {
        /* sn=2, dn=1: update low[1] */
        low[1] += (high[0] + high[0] + 2) >> 2;
    }

    memcpy(x,       low,  sn * sizeof(int32_t));
    memcpy(x + sn,  high, dn * sizeof(int32_t));
}

/* =========================================================================
 * Inverse 5/3 1-D (SSE4.1)
 * ========================================================================= */

void opj_dwt53_inv_1d_sse41(int32_t *x, uint32_t n, int32_t *scratch)
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
    /* i = 0 */
    {
        const int32_t right = (dn > 0) ? high[0] : 0;
        low[0] -= (high[0] + right + 2) >> 2;
    }
    if (dn >= 2) {
        for (i = 1; i + 3 < dn; i += 4) {
            __m128i hm1 = _mm_loadu_si128((const __m128i *)(high + i - 1));
            __m128i hi  = _mm_loadu_si128((const __m128i *)(high + i));
            __m128i li  = _mm_loadu_si128((const __m128i *)(low  + i));
            __m128i sum = _mm_add_epi32(_mm_add_epi32(hm1, hi),
                                        _mm_set1_epi32(2));
            _mm_storeu_si128((__m128i *)(low + i),
                             _mm_sub_epi32(li, _mm_srai_epi32(sum, 2)));
        }
        for (i = 1 + ((dn - 1) / 4) * 4; i < sn; i++) {
            const int32_t left  = high[i - 1];
            const int32_t right = (i < dn) ? high[i] : high[dn - 1];
            low[i] -= (left + right + 2) >> 2;
        }
    } else if (sn > 1) {
        low[1] -= (high[0] + high[0] + 2) >> 2;
    }

    /* ------------------------------------------------------------------
     * Inverse predict  x[2i] = low[i],  x[2i+1] = high[i] + ((low[i]+low[i+1])>>1)
     * Vectorise interior [0, dn-1), scatter back to interleaved x.
     *   _mm_unpacklo_epi32(l, h) = [l0,h0,l1,h1]
     *   _mm_unpackhi_epi32(l, h) = [l2,h2,l3,h3]
     * ------------------------------------------------------------------ */
    if (dn > 0) {
        const uint32_t vend = (dn >= 2) ? ((dn - 1) / 4) * 4 : 0;
        for (i = 0; i < vend; i += 4) {
            __m128i li   = _mm_loadu_si128((const __m128i *)(low  + i));
            __m128i lip1 = _mm_loadu_si128((const __m128i *)(low  + i + 1));
            __m128i hi   = _mm_loadu_si128((const __m128i *)(high + i));
            __m128i hout = _mm_add_epi32(hi,
                               _mm_srai_epi32(_mm_add_epi32(li, lip1), 1));
            _mm_storeu_si128((__m128i *)(x + 2 * i),
                             _mm_unpacklo_epi32(li, hout));
            _mm_storeu_si128((__m128i *)(x + 2 * i + 4),
                             _mm_unpackhi_epi32(li, hout));
        }
        /* scalar remainder + boundary */
        for (i = vend; i < dn; i++) {
            const int32_t right = (i + 1 < sn) ? low[i + 1] : low[sn - 1];
            x[2 * i]     = low[i];
            x[2 * i + 1] = high[i] + ((low[i] + right) >> 1);
        }
    }

    if (n & 1)
        x[n - 1] = low[sn - 1];
}

#endif /* OPJ_JP3D_HAVE_SSE41 */
