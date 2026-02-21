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
 * @file bench_dwt3d.c
 * @brief Reproducible benchmark for the 3-D DWT.
 *
 * Measures the throughput (MVoxels/sec) of the forward and inverse 3-D 5/3
 * (lossless) and 9/7 (lossy) DWT transforms at several volume sizes.  The
 * benchmark reports CPU feature availability so results can be compared
 * across runs with different SIMD capabilities.
 *
 * Usage:
 *   bench_dwt3d [<width> <height> <depth> <levels> <repeats>]
 *
 * Defaults: 256 × 256 × 64, 3 decomposition levels, 10 repetitions.
 */

#include "openjp3d.h"
#include "opj_cpu.h"
#include "opj_dwt3d.h"
#include "opj_mem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* =========================================================================
 * Timing helper
 * ========================================================================= */

static double now_sec(void)
{
#if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 199309L
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
#else
    return (double)clock() / (double)CLOCKS_PER_SEC;
#endif
}

/* =========================================================================
 * Benchmark one configuration
 * ========================================================================= */

static void bench_config(uint32_t w, uint32_t h, uint32_t d,
                          uint32_t levels, int repeats, int filter,
                          const char *filter_name)
{
    size_t  nvox  = (size_t)w * h * d;
    int32_t *data = (int32_t *)opj_jp3d_malloc(nvox * sizeof(int32_t));
    int32_t *orig = (int32_t *)opj_jp3d_malloc(nvox * sizeof(int32_t));
    if (!data || !orig) {
        fprintf(stderr, "bench_dwt3d: out of memory\n");
        opj_jp3d_free(data);
        opj_jp3d_free(orig);
        return;
    }

    /* Fill with deterministic pseudo-random data */
    for (size_t i = 0; i < nvox; i++)
        orig[i] = (int32_t)((i * 6271u ^ (i << 3)) & 0xFFFFu);

    /* Warm-up pass */
    memcpy(data, orig, nvox * sizeof(int32_t));
    opj_dwt3d_fwd(data, w, h, d, levels, levels, levels, filter);
    opj_dwt3d_inv(data, w, h, d, levels, levels, levels, filter);

    /* Forward DWT benchmark */
    double fwd_elapsed = 0.0;
    for (int r = 0; r < repeats; r++) {
        memcpy(data, orig, nvox * sizeof(int32_t));
        double t0 = now_sec();
        opj_dwt3d_fwd(data, w, h, d, levels, levels, levels, filter);
        fwd_elapsed += now_sec() - t0;
    }
    double fwd_mvps = ((double)nvox * repeats) / (fwd_elapsed * 1e6);

    /* Inverse DWT benchmark */
    opj_dwt3d_fwd(data, w, h, d, levels, levels, levels, filter);
    double inv_elapsed = 0.0;
    int32_t *work = (int32_t *)opj_jp3d_malloc(nvox * sizeof(int32_t));
    if (work) {
        memcpy(work, data, nvox * sizeof(int32_t));
        for (int r = 0; r < repeats; r++) {
            memcpy(data, work, nvox * sizeof(int32_t));
            double t0 = now_sec();
            opj_dwt3d_inv(data, w, h, d, levels, levels, levels, filter);
            inv_elapsed += now_sec() - t0;
        }
    }
    double inv_mvps = (work && inv_elapsed > 0.0)
                    ? ((double)nvox * repeats) / (inv_elapsed * 1e6)
                    : 0.0;

    printf("  DWT-%s %4u×%4u×%4u L=%u  fwd %7.2f MVox/s  inv %7.2f MVox/s\n",
           filter_name, w, h, d, levels, fwd_mvps, inv_mvps);

    opj_jp3d_free(data);
    opj_jp3d_free(orig);
    opj_jp3d_free(work);
}

/* =========================================================================
 * main
 * ========================================================================= */

int main(int argc, char *argv[])
{
    uint32_t bw      = 256;
    uint32_t bh      = 256;
    uint32_t bd      = 64;
    uint32_t blevels = 3;
    int      repeats = 10;

    if (argc >= 6) {
        bw      = (uint32_t)atoi(argv[1]);
        bh      = (uint32_t)atoi(argv[2]);
        bd      = (uint32_t)atoi(argv[3]);
        blevels = (uint32_t)atoi(argv[4]);
        repeats = atoi(argv[5]);
    }

    printf("OpenJP3D DWT benchmark  (version %s)\n", opj_jp3d_get_version());
    printf("CPU features: %s\n\n", opj_cpu_features_str());

    printf("Volume: %u × %u × %u,  levels=%u,  repeats=%d\n\n",
           bw, bh, bd, blevels, repeats);

    /* Run benchmarks for both filters */
    bench_config(bw, bh, bd, blevels, repeats, OPJ_JP3D_FILTER_53, "53 (lossless)");
    bench_config(bw, bh, bd, blevels, repeats, OPJ_JP3D_FILTER_97, "97 (lossy)   ");

    /* Additional smaller / larger sizes */
    printf("\nSweep across volume depths (fixed %ux%u XY):\n", bw, bh);
    static const uint32_t depths[] = {8, 16, 32, 64, 128};
    for (size_t i = 0; i < sizeof(depths) / sizeof(depths[0]); i++) {
        bench_config(bw, bh, depths[i], blevels, repeats,
                     OPJ_JP3D_FILTER_53, "53");
    }

    printf("\nDone.\n");
    return 0;
}
