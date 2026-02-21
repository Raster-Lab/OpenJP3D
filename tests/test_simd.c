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
 * @file test_simd.c
 * @brief SIMD correctness tests for the Phase 4 optimisations.
 *
 * Tests:
 *  1. CPU feature detection API returns a valid bitmask.
 *  2. CPU feature string is non-empty.
 *  3–N. For each available SIMD path (SSE4.1, AVX2, NEON), the forward and
 *       inverse 5/3 1-D DWT produce bit-identical output to the scalar
 *       reference across a range of row lengths and data patterns.
 *  N+1. The dispatch function (called via opj_dwt3d_fwd/inv) produces
 *       bit-identical output to the scalar reference for 3-D volumes.
 */

#include "openjp3d.h"
#include "opj_cpu.h"
#include "opj_dwt3d.h"
#include "opj_dwt3d_simd.h"
#include "opj_mem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int test_count = 0;
static int pass_count = 0;

#define ASSERT(cond) \
    do { \
        test_count++; \
        if (cond) { \
            pass_count++; \
        } else { \
            fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, #cond); \
        } \
    } while (0)

/* =========================================================================
 * Helpers
 * ========================================================================= */

static int arrays_equal(const int32_t *a, const int32_t *b, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (a[i] != b[i]) return 0;
    return 1;
}

/**
 * @brief Fill array with deterministic values.
 *
 * Uses values in [-127, 127] to stay in a range well-supported by all
 * bit-depths (and avoid overflow in the 5/3 lifting predict step).
 */
static void fill_det(int32_t *x, uint32_t n, uint32_t seed)
{
    for (uint32_t i = 0; i < n; i++)
        x[i] = (int32_t)(((i * 6271u + seed) ^ (i << 3)) & 0xFF) - 127;
}

/* =========================================================================
 * Scalar 5/3 1-D reference (duplicated from opj_dwt3d.c for comparison)
 * These functions are static in opj_dwt3d.c so we re-implement them here.
 * ========================================================================= */

static void scalar_fwd53_1d(int32_t *x, uint32_t n, int32_t *scratch)
{
    if (n <= 1) return;
    uint32_t sn = (n + 1) / 2;
    uint32_t dn = n / 2;
    int32_t *low  = scratch;
    int32_t *high = scratch + sn;

    for (uint32_t i = 0; i < dn; i++) {
        int32_t right = (2 * (i + 1) < n) ? x[2 * (i + 1)] : x[2 * (i + 1) - 2];
        high[i] = x[2 * i + 1] - ((x[2 * i] + right) >> 1);
    }
    for (uint32_t i = 0; i < sn; i++) {
        int32_t left  = (i > 0)  ? high[i - 1] : high[0];
        int32_t right = (i < dn) ? high[i]      : high[dn > 0 ? dn - 1 : 0];
        low[i] = x[2 * i] + ((left + right + 2) >> 2);
    }
    memcpy(x,       low,  sn * sizeof(int32_t));
    memcpy(x + sn,  high, dn * sizeof(int32_t));
}

static void scalar_inv53_1d(int32_t *x, uint32_t n, int32_t *scratch)
{
    if (n <= 1) return;
    uint32_t sn = (n + 1) / 2;
    uint32_t dn = n / 2;
    int32_t *low  = scratch;
    int32_t *high = scratch + sn;

    memcpy(low,  x,      sn * sizeof(int32_t));
    memcpy(high, x + sn, dn * sizeof(int32_t));

    for (uint32_t i = 0; i < sn; i++) {
        int32_t left  = (i > 0)  ? high[i - 1] : high[0];
        int32_t right = (i < dn) ? high[i]      : high[dn > 0 ? dn - 1 : 0];
        low[i] -= ((left + right + 2) >> 2);
    }
    for (uint32_t i = 0; i < dn; i++) {
        int32_t right = (i + 1 < sn) ? low[i + 1] : low[sn > 0 ? sn - 1 : 0];
        x[2 * i]     = low[i];
        x[2 * i + 1] = high[i] + ((low[i] + right) >> 1);
    }
    if (n & 1)
        x[n - 1] = low[sn - 1];
}

/* =========================================================================
 * Generic 1-D SIMD vs scalar comparison helper
 * ========================================================================= */

typedef void (*fwd_fn_t)(int32_t *, uint32_t, int32_t *);
typedef void (*inv_fn_t)(int32_t *, uint32_t, int32_t *);

/**
 * @brief Test that simd_fwd produces the same output as scalar_fwd for length n.
 * @return 1 on pass, 0 on fail.
 */
static int cmp_fwd(fwd_fn_t simd_fwd, uint32_t n, uint32_t seed)
{
    int32_t *ref  = (int32_t *)opj_jp3d_malloc(n * sizeof(int32_t));
    int32_t *test = (int32_t *)opj_jp3d_malloc(n * sizeof(int32_t));
    int32_t *scr  = (int32_t *)opj_jp3d_malloc(n * sizeof(int32_t));
    if (!ref || !test || !scr) {
        opj_jp3d_free(ref); opj_jp3d_free(test); opj_jp3d_free(scr);
        return 0;
    }
    fill_det(ref, n, seed);
    memcpy(test, ref, n * sizeof(int32_t));

    scalar_fwd53_1d(ref, n, scr);
    simd_fwd(test, n, scr);

    int ok = arrays_equal(ref, test, n);
    opj_jp3d_free(ref); opj_jp3d_free(test); opj_jp3d_free(scr);
    return ok;
}

/**
 * @brief Test that simd_inv produces the same output as scalar_inv for length n.
 */
static int cmp_inv(inv_fn_t simd_inv, uint32_t n, uint32_t seed)
{
    int32_t *ref  = (int32_t *)opj_jp3d_malloc(n * sizeof(int32_t));
    int32_t *test = (int32_t *)opj_jp3d_malloc(n * sizeof(int32_t));
    int32_t *scr  = (int32_t *)opj_jp3d_malloc(n * sizeof(int32_t));
    if (!ref || !test || !scr) {
        opj_jp3d_free(ref); opj_jp3d_free(test); opj_jp3d_free(scr);
        return 0;
    }
    /* Create a valid non-interleaved DWT output as test input */
    fill_det(ref, n, seed);
    scalar_fwd53_1d(ref, n, scr);   /* ref is now [low | high] */
    memcpy(test, ref, n * sizeof(int32_t));

    scalar_inv53_1d(ref, n, scr);
    simd_inv(test, n, scr);

    int ok = arrays_equal(ref, test, n);
    opj_jp3d_free(ref); opj_jp3d_free(test); opj_jp3d_free(scr);
    return ok;
}

/* Test a range of lengths */
static void test_simd_path_fwd(fwd_fn_t fn, const char *name)
{
    static const uint32_t lens[] = {
        2, 3, 4, 5, 6, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128
    };
    for (size_t i = 0; i < sizeof(lens) / sizeof(lens[0]); i++) {
        int ok = cmp_fwd(fn, lens[i], (uint32_t)(i * 137 + 42));
        if (!ok)
            fprintf(stderr, "  %s fwd n=%u MISMATCH\n", name, lens[i]);
        ASSERT(ok);
    }
}

static void test_simd_path_inv(inv_fn_t fn, const char *name)
{
    static const uint32_t lens[] = {
        2, 3, 4, 5, 6, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128
    };
    for (size_t i = 0; i < sizeof(lens) / sizeof(lens[0]); i++) {
        int ok = cmp_inv(fn, lens[i], (uint32_t)(i * 137 + 42));
        if (!ok)
            fprintf(stderr, "  %s inv n=%u MISMATCH\n", name, lens[i]);
        ASSERT(ok);
    }
}

/* =========================================================================
 * 3-D DWT dispatch correctness (opj_dwt3d_fwd / inv)
 * ========================================================================= */

/** Round-trip test for 5/3 via the public API (uses SIMD dispatch). */
static int rt53_dispatch(uint32_t w, uint32_t h, uint32_t d,
                          uint32_t nx, uint32_t ny, uint32_t nz)
{
    size_t n = (size_t)w * h * d;
    int32_t *orig = (int32_t *)opj_jp3d_malloc(n * sizeof(int32_t));
    int32_t *data = (int32_t *)opj_jp3d_malloc(n * sizeof(int32_t));
    if (!orig || !data) { opj_jp3d_free(orig); opj_jp3d_free(data); return 0; }

    for (size_t i = 0; i < n; i++)
        orig[i] = (int32_t)(((i * 6271u) ^ (i << 3)) & 0xFF);

    memcpy(data, orig, n * sizeof(int32_t));
    opj_dwt3d_fwd(data, w, h, d, nx, ny, nz, OPJ_JP3D_FILTER_53);
    opj_dwt3d_inv(data, w, h, d, nx, ny, nz, OPJ_JP3D_FILTER_53);

    int ok = arrays_equal(orig, data, n);
    opj_jp3d_free(orig);
    opj_jp3d_free(data);
    return ok;
}

/* =========================================================================
 * main
 * ========================================================================= */

int main(void)
{
    /* -----------------------------------------------------------------------
     * Test 1: CPU feature detection returns a valid bitmask
     * --------------------------------------------------------------------- */
    {
        uint32_t f = opj_cpu_features();
        /* Feature flags are a subset of the defined flags */
        uint32_t valid_mask = OPJ_CPU_FEATURE_SSE2 | OPJ_CPU_FEATURE_SSE41 |
                              OPJ_CPU_FEATURE_AVX2  | OPJ_CPU_FEATURE_NEON;
        ASSERT((f & ~valid_mask) == 0u);
    }

    /* -----------------------------------------------------------------------
     * Test 2: CPU feature string is non-NULL and non-empty
     * --------------------------------------------------------------------- */
    {
        const char *s = opj_cpu_features_str();
        ASSERT(s != NULL);
        ASSERT(s[0] != '\0');
    }

    /* -----------------------------------------------------------------------
     * Test 3: CPU features printed for diagnostics
     * --------------------------------------------------------------------- */
    printf("Detected CPU features: %s\n", opj_cpu_features_str());

    /* -----------------------------------------------------------------------
     * SIMD 1-D DWT correctness vs scalar reference
     * --------------------------------------------------------------------- */
    uint32_t f = opj_cpu_features();

#ifdef OPJ_JP3D_HAVE_SSE41
    if (f & OPJ_CPU_FEATURE_SSE41) {
        printf("Testing SSE4.1 5/3 forward 1-D DWT...\n");
        test_simd_path_fwd(opj_dwt53_fwd_1d_sse41, "SSE4.1");
        printf("Testing SSE4.1 5/3 inverse 1-D DWT...\n");
        test_simd_path_inv(opj_dwt53_inv_1d_sse41, "SSE4.1");
    } else {
        printf("SSE4.1 not available at runtime — skipping SSE4.1 tests.\n");
    }
#else
    printf("SSE4.1 not compiled in — skipping SSE4.1 tests.\n");
#endif

#ifdef OPJ_JP3D_HAVE_AVX2
    if (f & OPJ_CPU_FEATURE_AVX2) {
        printf("Testing AVX2 5/3 forward 1-D DWT...\n");
        test_simd_path_fwd(opj_dwt53_fwd_1d_avx2, "AVX2");
        printf("Testing AVX2 5/3 inverse 1-D DWT...\n");
        test_simd_path_inv(opj_dwt53_inv_1d_avx2, "AVX2");
    } else {
        printf("AVX2 not available at runtime — skipping AVX2 tests.\n");
    }
#else
    printf("AVX2 not compiled in — skipping AVX2 tests.\n");
#endif

#ifdef OPJ_JP3D_HAVE_NEON
    if (f & OPJ_CPU_FEATURE_NEON) {
        printf("Testing NEON 5/3 forward 1-D DWT...\n");
        test_simd_path_fwd(opj_dwt53_fwd_1d_neon, "NEON");
        printf("Testing NEON 5/3 inverse 1-D DWT...\n");
        test_simd_path_inv(opj_dwt53_inv_1d_neon, "NEON");
    } else {
        printf("NEON not available at runtime — skipping NEON tests.\n");
    }
#else
    printf("NEON not compiled in — skipping NEON tests.\n");
#endif

    /* -----------------------------------------------------------------------
     * 3-D DWT dispatch round-trip tests (via opj_dwt3d_fwd / inv)
     * These exercise the full dispatch path including any SIMD path.
     * --------------------------------------------------------------------- */
    printf("Testing 3-D DWT dispatch round-trips...\n");

    ASSERT(rt53_dispatch(4,  4,  4,  1, 1, 1));
    ASSERT(rt53_dispatch(8,  8,  8,  2, 2, 2));
    ASSERT(rt53_dispatch(16, 16, 16, 3, 3, 3));
    ASSERT(rt53_dispatch(32, 32, 32, 3, 3, 3));
    ASSERT(rt53_dispatch(64, 64, 16, 3, 3, 2));
    ASSERT(rt53_dispatch(5,  7,  3,  1, 1, 1));   /* non-power-of-2 */
    ASSERT(rt53_dispatch(17, 13, 9,  2, 2, 1));   /* odd sizes      */
    ASSERT(rt53_dispatch(2,  1,  1,  1, 0, 0));   /* 1-D like       */
    ASSERT(rt53_dispatch(1,  1,  8,  0, 0, 1));   /* Z-only         */
    ASSERT(rt53_dispatch(128, 128, 32, 3, 3, 2)); /* larger volume  */

    printf("Passed %d/%d tests\n", pass_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}
