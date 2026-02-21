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
 * @file opj_dwt3d_neon.c
 * @brief NEON-optimised 5/3 integer lifting DWT for OpenJP3D (AArch64).
 *
 * Provides opj_dwt53_fwd_1d_neon() and opj_dwt53_inv_1d_neon() for
 * AArch64/NEON.  NEON is mandatory in AArch64 (ARMv8-A), so no runtime
 * guard is needed.  This file is compiled without extra flags since NEON
 * is always on for AArch64 targets.  Output is bit-identical to the
 * scalar reference.
 *
 * NEON processes 4 int32 samples per 128-bit register (int32x4_t).
 * De-interleaving uses vld2q_s32() which loads two interleaved arrays
 * of 4 int32 each in a single instruction.
 */

#ifdef OPJ_JP3D_HAVE_NEON

#include "opj_dwt3d_simd.h"
#include <arm_neon.h>
#include <string.h>

/* =========================================================================
 * Forward 5/3 1-D (NEON)
 * ========================================================================= */

void opj_dwt53_fwd_1d_neon(int32_t *x, uint32_t n, int32_t *scratch)
{
    if (n <= 1)
        return;

    const uint32_t sn = (n + 1) / 2;
    const uint32_t dn = n / 2;
    int32_t * const low  = scratch;
    int32_t * const high = scratch + sn;
    uint32_t i;

    /* ------------------------------------------------------------------
     * Step 1: de-interleave using vld2q_s32 (4 pairs per call).
     * vld2q_s32 loads two arrays of 4 int32 from interleaved memory:
     *   result.val[0] = [e0,e1,e2,e3],  result.val[1] = [o0,o1,o2,o3]
     * ------------------------------------------------------------------ */
    {
        const uint32_t pairs4 = (n / 8) * 4;
        for (i = 0; i < pairs4; i += 4) {
            int32x4x2_t v = vld2q_s32(x + 2 * i);
            vst1q_s32(low  + i, v.val[0]);
            vst1q_s32(high + i, v.val[1]);
        }
        for (i = (n / 8) * 4; i < sn; i++) low[i]  = x[2 * i];
        for (i = (n / 8) * 4; i < dn; i++) high[i] = x[2 * i + 1];
    }

    /* ------------------------------------------------------------------
     * Step 2: Predict  high[i] -= (low[i] + low[i+1]) >> 1
     * ------------------------------------------------------------------ */
    if (dn > 0) {
        const uint32_t vend = (dn >= 2) ? ((dn - 1) / 4) * 4 : 0;
        for (i = 0; i < vend; i += 4) {
            int32x4_t ei   = vld1q_s32(low  + i);
            int32x4_t eip1 = vld1q_s32(low  + i + 1);
            int32x4_t oi   = vld1q_s32(high + i);
            /* (ei + eip1) >> 1  using arithmetic shift */
            int32x4_t sum  = vaddq_s32(ei, eip1);
            int32x4_t pred = vshrq_n_s32(sum, 1);
            vst1q_s32(high + i, vsubq_s32(oi, pred));
        }
        for (i = vend; i < dn; i++) {
            const int32_t right = (2 * (i + 1) < n) ? low[i + 1] : low[sn - 1];
            high[i] = high[i] - ((low[i] + right) >> 1);
        }
    }

    /* ------------------------------------------------------------------
     * Step 3: Update  low[i] += (high[i-1] + high[i] + 2) >> 2
     * ------------------------------------------------------------------ */
    {
        const int32_t right = (dn > 0) ? high[0] : 0;
        low[0] += (high[0] + right + 2) >> 2;
    }
    if (dn >= 2) {
        const int32x4_t two = vdupq_n_s32(2);
        for (i = 1; i + 3 < dn; i += 4) {
            int32x4_t hm1 = vld1q_s32(high + i - 1);
            int32x4_t hi  = vld1q_s32(high + i);
            int32x4_t li  = vld1q_s32(low  + i);
            int32x4_t sum = vaddq_s32(vaddq_s32(hm1, hi), two);
            vst1q_s32(low + i, vaddq_s32(li, vshrq_n_s32(sum, 2)));
        }
        for (i = 1 + ((dn - 1) / 4) * 4; i < sn; i++) {
            const int32_t left  = high[i - 1];
            const int32_t right = (i < dn) ? high[i] : high[dn - 1];
            low[i] += (left + right + 2) >> 2;
        }
    } else if (sn > 1) {
        low[1] += (high[0] + high[0] + 2) >> 2;
    }

    memcpy(x,       low,  sn * sizeof(int32_t));
    memcpy(x + sn,  high, dn * sizeof(int32_t));
}

/* =========================================================================
 * Inverse 5/3 1-D (NEON)
 * ========================================================================= */

void opj_dwt53_inv_1d_neon(int32_t *x, uint32_t n, int32_t *scratch)
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
        const int32x4_t two = vdupq_n_s32(2);
        for (i = 1; i + 3 < dn; i += 4) {
            int32x4_t hm1 = vld1q_s32(high + i - 1);
            int32x4_t hi  = vld1q_s32(high + i);
            int32x4_t li  = vld1q_s32(low  + i);
            int32x4_t sum = vaddq_s32(vaddq_s32(hm1, hi), two);
            vst1q_s32(low + i, vsubq_s32(li, vshrq_n_s32(sum, 2)));
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
     * Inverse predict: scatter back to interleaved x.
     *   x[2i]   = low[i]
     *   x[2i+1] = high[i] + (low[i] + low[i+1]) >> 1
     * Use vst2q_s32 to write two arrays interleaved.
     * ------------------------------------------------------------------ */
    if (dn > 0) {
        const uint32_t vend = (dn >= 2) ? ((dn - 1) / 4) * 4 : 0;
        for (i = 0; i < vend; i += 4) {
            int32x4_t li   = vld1q_s32(low  + i);
            int32x4_t lip1 = vld1q_s32(low  + i + 1);
            int32x4_t hi   = vld1q_s32(high + i);
            int32x4_t hout = vaddq_s32(hi,
                                 vshrq_n_s32(vaddq_s32(li, lip1), 1));
            int32x4x2_t out;
            out.val[0] = li;
            out.val[1] = hout;
            vst2q_s32(x + 2 * i, out);
        }
        for (i = vend; i < dn; i++) {
            const int32_t right = (i + 1 < sn) ? low[i + 1] : low[sn - 1];
            x[2 * i]     = low[i];
            x[2 * i + 1] = high[i] + ((low[i] + right) >> 1);
        }
    }

    if (n & 1)
        x[n - 1] = low[sn - 1];
}

#endif /* OPJ_JP3D_HAVE_NEON */
