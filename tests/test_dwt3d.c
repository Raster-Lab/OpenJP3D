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
 * @file test_dwt3d.c
 * @brief Unit tests for the 3-D DWT (forward + inverse round-trip).
 */

#include "opj_dwt3d.h"
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
            fprintf(stderr, "FAIL: %s  (line %d)\n", #cond, __LINE__); \
        } \
    } while (0)

static int arrays_equal(const int32_t *a, const int32_t *b, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (a[i] != b[i]) return 0;
    return 1;
}

static int arrays_close(const int32_t *a, const int32_t *b, size_t n, int32_t tol)
{
    for (size_t i = 0; i < n; i++) {
        int32_t diff = a[i] - b[i];
        if (diff < 0) diff = -diff;
        if (diff > tol) return 0;
    }
    return 1;
}

/** Round-trip test for filter 53 (lossless): must be bit-exact. */
static int rt53(uint32_t w, uint32_t h, uint32_t d,
                uint32_t nx, uint32_t ny, uint32_t nz)
{
    size_t n = (size_t)w * h * d;
    int32_t *orig = (int32_t *)opj_jp3d_malloc(n * sizeof(int32_t));
    int32_t *data = (int32_t *)opj_jp3d_malloc(n * sizeof(int32_t));
    if (!orig || !data) { opj_jp3d_free(orig); opj_jp3d_free(data); return 0; }

    /* Fill with deterministic values */
    for (size_t i = 0; i < n; i++)
        orig[i] = (int32_t)(((i * 6271) ^ (i << 3)) & 0xFF);

    memcpy(data, orig, n * sizeof(int32_t));
    opj_dwt3d_fwd(data, w, h, d, nx, ny, nz, OPJ_JP3D_FILTER_53);
    opj_dwt3d_inv(data, w, h, d, nx, ny, nz, OPJ_JP3D_FILTER_53);

    int ok = arrays_equal(orig, data, n);
    opj_jp3d_free(orig);
    opj_jp3d_free(data);
    return ok;
}

/** Round-trip test for filter 97 (lossy): allow tolerance. */
static int rt97(uint32_t w, uint32_t h, uint32_t d,
                uint32_t nx, uint32_t ny, uint32_t nz,
                int32_t tol)
{
    size_t n = (size_t)w * h * d;
    int32_t *orig = (int32_t *)opj_jp3d_malloc(n * sizeof(int32_t));
    int32_t *data = (int32_t *)opj_jp3d_malloc(n * sizeof(int32_t));
    if (!orig || !data) { opj_jp3d_free(orig); opj_jp3d_free(data); return 0; }

    for (size_t i = 0; i < n; i++)
        orig[i] = (int32_t)(((i * 6271) ^ (i << 3)) & 0x7F);

    memcpy(data, orig, n * sizeof(int32_t));
    opj_dwt3d_fwd(data, w, h, d, nx, ny, nz, OPJ_JP3D_FILTER_97);
    opj_dwt3d_inv(data, w, h, d, nx, ny, nz, OPJ_JP3D_FILTER_97);

    int ok = arrays_close(orig, data, n, tol);
    opj_jp3d_free(orig);
    opj_jp3d_free(data);
    return ok;
}

int main(void)
{
    /* 1. 53 1D-like: 2x1x1, 1 level */
    ASSERT(rt53(2, 1, 1, 1, 0, 0));

    /* 2. 53 1D-like: 4x1x1 */
    ASSERT(rt53(4, 1, 1, 1, 0, 0));

    /* 3. 53 1D-like: 5x1x1 (odd) */
    ASSERT(rt53(5, 1, 1, 1, 0, 0));

    /* 4. 53 3D: 4x4x4, 1 level */
    ASSERT(rt53(4, 4, 4, 1, 1, 1));

    /* 5. 53 3D: 8x8x8, 1 level */
    ASSERT(rt53(8, 8, 8, 1, 1, 1));

    /* 6. 53 3D: 1x1x4 (Z-only) */
    ASSERT(rt53(1, 1, 4, 0, 0, 1));

    /* 7. 53 3D: 8x8x8, 2 levels */
    ASSERT(rt53(8, 8, 8, 2, 2, 2));

    /* 8. 53 3D: all-zero array (trivial) */
    {
        uint32_t w = 4, h = 4, d = 4;
        size_t n = (size_t)w * h * d;
        int32_t *data = (int32_t *)opj_jp3d_calloc(n, sizeof(int32_t));
        int32_t *orig = (int32_t *)opj_jp3d_calloc(n, sizeof(int32_t));
        opj_dwt3d_fwd(data, w, h, d, 1, 1, 1, OPJ_JP3D_FILTER_53);
        opj_dwt3d_inv(data, w, h, d, 1, 1, 1, OPJ_JP3D_FILTER_53);
        ASSERT(arrays_equal(orig, data, n));
        opj_jp3d_free(data); opj_jp3d_free(orig);
    }

    /* 9. 53 3D: constant value array */
    {
        uint32_t w = 4, h = 4, d = 4;
        size_t n = (size_t)w * h * d;
        int32_t *data = (int32_t *)opj_jp3d_malloc(n * sizeof(int32_t));
        int32_t *orig = (int32_t *)opj_jp3d_malloc(n * sizeof(int32_t));
        for (size_t i = 0; i < n; i++) data[i] = orig[i] = 100;
        opj_dwt3d_fwd(data, w, h, d, 1, 1, 1, OPJ_JP3D_FILTER_53);
        opj_dwt3d_inv(data, w, h, d, 1, 1, 1, OPJ_JP3D_FILTER_53);
        ASSERT(arrays_equal(orig, data, n));
        opj_jp3d_free(data); opj_jp3d_free(orig);
    }

    /* 10. 53 3D: 16x16x16, 3 levels */
    ASSERT(rt53(16, 16, 16, 3, 3, 3));

    /* 11. 53 3D: non-power-of-2: 5x7x3 */
    ASSERT(rt53(5, 7, 3, 1, 1, 1));

    /* 12. 53 3D: 2x2x2, 1 level */
    ASSERT(rt53(2, 2, 2, 1, 1, 1));

    /* 13. 53 0-level (identity) */
    ASSERT(rt53(8, 8, 8, 0, 0, 0));

    /* 14. 97 3D: 8x8x8, 1 level (3D cascading allows tolerance of 3) */
    ASSERT(rt97(8, 8, 8, 1, 1, 1, 3));

    /* 15. 97 3D: 4x4x4, 1 level */
    ASSERT(rt97(4, 4, 4, 1, 1, 1, 3));

    printf("Passed %d/%d tests\n", pass_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}
