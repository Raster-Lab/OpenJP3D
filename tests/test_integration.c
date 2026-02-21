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
 * @file test_integration.c
 * @brief Task 7.2 — Real-world dataset simulation integration tests.
 *
 * Exercises the full codec pipeline with synthetic volumes that mimic
 * characteristics of CT, MRI, satellite, and microscopy data.
 */

#include "openjp3d.h"

#include <math.h>
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

/* =========================================================================
 * Helpers
 * ========================================================================= */

/** Compare two volumes sample by sample; return 1 if identical. */
static int volumes_equal(const opj_volume_t *a, const opj_volume_t *b)
{
    if (a->numcomps != b->numcomps) return 0;
    for (uint32_t c = 0; c < a->numcomps; c++) {
        const opj_volume_comp_t *ca = &a->comps[c];
        const opj_volume_comp_t *cb = &b->comps[c];
        if (ca->w != cb->w || ca->h != cb->h || ca->d != cb->d) return 0;
        size_t n = (size_t)ca->w * ca->h * ca->d;
        for (size_t i = 0; i < n; i++)
            if (ca->data[i] != cb->data[i]) return 0;
    }
    return 1;
}

/** Compute PSNR between two volumes given a peak value. */
static double compute_psnr(const opj_volume_t *orig, const opj_volume_t *dec,
                           double max_val)
{
    double mse = 0.0;
    size_t total = 0;
    for (uint32_t c = 0; c < orig->numcomps; c++) {
        const opj_volume_comp_t *ca = &orig->comps[c];
        const opj_volume_comp_t *cb = &dec->comps[c];
        size_t n = (size_t)ca->w * ca->h * ca->d;
        for (size_t i = 0; i < n; i++) {
            double diff = (double)ca->data[i] - (double)cb->data[i];
            mse += diff * diff;
        }
        total += n;
    }
    if (total == 0) return 0.0;
    mse /= (double)total;
    if (mse == 0.0) return 999.0;
    return 10.0 * log10((max_val * max_val) / mse);
}

/** Simple deterministic pseudo-random (xorshift32). */
static uint32_t xorshift32(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/** Lossless encode→decode round-trip with custom encoder params. */
static int roundtrip_with_params(opj_volume_t *orig,
                                 const opj_jp3d_enc_params_t *enc)
{
    uint8_t *cs = NULL;
    size_t cs_sz = 0;
    if (!opj_jp3d_encode(orig, enc, &cs, &cs_sz, NULL, NULL))
        return 0;

    opj_volume_t *dec = opj_jp3d_decode(cs, cs_sz, NULL, NULL, NULL);
    opj_jp3d_free(cs);
    if (!dec) return 0;

    int ok = volumes_equal(orig, dec);
    opj_jp3d_destroy_volume(dec);
    return ok;
}

/** Lossy encode→decode, return PSNR (or -1 on failure). */
static double lossy_roundtrip_psnr(opj_volume_t *orig,
                                   const opj_jp3d_enc_params_t *enc,
                                   double max_val)
{
    uint8_t *cs = NULL;
    size_t cs_sz = 0;
    if (!opj_jp3d_encode(orig, enc, &cs, &cs_sz, NULL, NULL))
        return -1.0;

    opj_volume_t *dec = opj_jp3d_decode(cs, cs_sz, NULL, NULL, NULL);
    opj_jp3d_free(cs);
    if (!dec) return -1.0;

    double psnr = compute_psnr(orig, dec, max_val);
    opj_jp3d_destroy_volume(dec);
    return psnr;
}

/* =========================================================================
 * 1. CT-like volume: 256x256x64, 16-bit unsigned, smooth gradients + noise
 * ========================================================================= */

static void test_ct_like_volume(void)
{
    const uint32_t W = 256, H = 256, D = 64;
    opj_volume_t *vol = opj_jp3d_create_volume(1, W, H, D, 16, 0);
    ASSERT(vol != NULL);
    if (!vol) return;

    uint32_t rng = 12345;
    for (uint32_t z = 0; z < D; z++)
        for (uint32_t y = 0; y < H; y++)
            for (uint32_t x = 0; x < W; x++) {
                /* Linear gradient scaled to span 16-bit range, z weighted
                   more heavily to simulate inter-slice variation in CT. */
                int32_t base = (int32_t)((x + y + z * 4) * 64);
                int32_t noise = (int32_t)(xorshift32(&rng) % 128) - 64;
                int32_t val = base + noise;
                if (val < 0) val = 0;
                if (val > 65535) val = 65535;
                vol->comps[0].data[z * W * H + y * W + x] = val;
            }

    opj_jp3d_enc_params_t enc;
    opj_jp3d_set_default_encoder_parameters(&enc);
    enc.num_resolutions_x = enc.num_resolutions_y = enc.num_resolutions_z = 1;
    enc.tile_width = W; enc.tile_height = H; enc.tile_depth = D;
    enc.cblk_width = W; enc.cblk_height = H; enc.cblk_depth = D;

    ASSERT(roundtrip_with_params(vol, &enc));
    opj_jp3d_destroy_volume(vol);
}

/* =========================================================================
 * 2. MRI-like volume: 128x128x32, 16-bit, 2 comps, smooth regions
 * ========================================================================= */

static void test_mri_like_volume(void)
{
    const uint32_t W = 128, H = 128, D = 32;
    opj_volume_t *vol = opj_jp3d_create_volume(2, W, H, D, 16, 0);
    ASSERT(vol != NULL);
    if (!vol) return;

    for (uint32_t c = 0; c < 2; c++) {
        for (uint32_t z = 0; z < D; z++)
            for (uint32_t y = 0; y < H; y++)
                for (uint32_t x = 0; x < W; x++) {
                    int32_t val = (int32_t)((x * 128 + y * 64 + z * 32 +
                                   c * 8192) % 65536);
                    vol->comps[c].data[z * W * H + y * W + x] = val;
                }
    }

    opj_jp3d_enc_params_t enc;
    opj_jp3d_set_default_encoder_parameters(&enc);
    enc.num_resolutions_x = enc.num_resolutions_y = enc.num_resolutions_z = 1;
    enc.tile_width = W; enc.tile_height = H; enc.tile_depth = D;
    enc.cblk_width = W; enc.cblk_height = H; enc.cblk_depth = D;

    ASSERT(roundtrip_with_params(vol, &enc));
    opj_jp3d_destroy_volume(vol);
}

/* =========================================================================
 * 3. Satellite/geospatial: 64x64x8, 8-bit, 3 comps (RGB bands)
 * ========================================================================= */

static void test_satellite_volume(void)
{
    const uint32_t W = 64, H = 64, D = 8;
    opj_volume_t *vol = opj_jp3d_create_volume(3, W, H, D, 8, 0);
    ASSERT(vol != NULL);
    if (!vol) return;

    uint32_t rng = 42;
    for (uint32_t c = 0; c < 3; c++) {
        for (size_t i = 0; i < (size_t)W * H * D; i++) {
            /* Mix of spatial patterns per band */
            int32_t val = (int32_t)(((i * (c + 1) * 97) + xorshift32(&rng))
                           % 256);
            vol->comps[c].data[i] = val;
        }
    }

    opj_jp3d_enc_params_t enc;
    opj_jp3d_set_default_encoder_parameters(&enc);
    enc.num_resolutions_x = enc.num_resolutions_y = enc.num_resolutions_z = 1;
    enc.tile_width = W; enc.tile_height = H; enc.tile_depth = D;
    enc.cblk_width = W; enc.cblk_height = H; enc.cblk_depth = D;

    ASSERT(roundtrip_with_params(vol, &enc));
    opj_jp3d_destroy_volume(vol);
}

/* =========================================================================
 * 4. Microscopy: 32x32x16, 8-bit, sparse (mostly zeros, bright spots)
 * ========================================================================= */

static void test_microscopy_volume(void)
{
    const uint32_t W = 32, H = 32, D = 16;
    opj_volume_t *vol = opj_jp3d_create_volume(1, W, H, D, 8, 0);
    ASSERT(vol != NULL);
    if (!vol) return;

    size_t n = (size_t)W * H * D;
    memset(vol->comps[0].data, 0, n * sizeof(int32_t));
    /* Scatter bright spots deterministically */
    for (size_t i = 0; i < n; i++) {
        if ((i * 6271) % 97 == 0)
            vol->comps[0].data[i] = (int32_t)(200 + (i % 56));
    }

    opj_jp3d_enc_params_t enc;
    opj_jp3d_set_default_encoder_parameters(&enc);
    enc.num_resolutions_x = enc.num_resolutions_y = enc.num_resolutions_z = 1;
    enc.tile_width = W; enc.tile_height = H; enc.tile_depth = D;
    enc.cblk_width = W; enc.cblk_height = H; enc.cblk_depth = D;

    ASSERT(roundtrip_with_params(vol, &enc));
    opj_jp3d_destroy_volume(vol);
}

/* =========================================================================
 * 5. CT multi-tile: 64x64x16, 16-bit, tiled (32x32x8 tiles)
 * ========================================================================= */

static void test_ct_multitile(void)
{
    const uint32_t W = 64, H = 64, D = 16;
    opj_volume_t *vol = opj_jp3d_create_volume(1, W, H, D, 16, 0);
    ASSERT(vol != NULL);
    if (!vol) return;

    for (size_t i = 0; i < (size_t)W * H * D; i++)
        vol->comps[0].data[i] = (int32_t)((i * 6271 + 137) % 65536);

    opj_jp3d_enc_params_t enc;
    opj_jp3d_set_default_encoder_parameters(&enc);
    enc.num_resolutions_x = enc.num_resolutions_y = enc.num_resolutions_z = 1;
    enc.tile_width = 32; enc.tile_height = 32; enc.tile_depth = 8;
    enc.cblk_width = 32; enc.cblk_height = 32; enc.cblk_depth = 8;

    ASSERT(roundtrip_with_params(vol, &enc));
    opj_jp3d_destroy_volume(vol);
}

/* =========================================================================
 * 6. Large HTJ2K round-trip: 32x32x32, 8-bit
 * ========================================================================= */

static void test_large_htj2k(void)
{
    const uint32_t W = 32, H = 32, D = 32;
    opj_volume_t *vol = opj_jp3d_create_volume(1, W, H, D, 8, 0);
    ASSERT(vol != NULL);
    if (!vol) return;

    for (size_t i = 0; i < (size_t)W * H * D; i++)
        vol->comps[0].data[i] = (int32_t)((i * 6271 + 137) % 256);

    opj_jp3d_enc_params_t enc;
    opj_jp3d_set_default_encoder_parameters(&enc);
    enc.use_htj2k = OPJ_JP3D_USE_HTJ2K;
    enc.num_resolutions_x = enc.num_resolutions_y = enc.num_resolutions_z = 1;
    enc.tile_width = W; enc.tile_height = H; enc.tile_depth = D;
    enc.cblk_width = W; enc.cblk_height = H; enc.cblk_depth = D;

    ASSERT(roundtrip_with_params(vol, &enc));
    opj_jp3d_destroy_volume(vol);
}

/* =========================================================================
 * 7. Lossy medical: 64x64x16, 16-bit, 9/7 filter, PSNR > 30 dB
 * ========================================================================= */

static void test_lossy_medical(void)
{
    const uint32_t W = 64, H = 64, D = 16;
    opj_volume_t *vol = opj_jp3d_create_volume(1, W, H, D, 16, 0);
    ASSERT(vol != NULL);
    if (!vol) return;

    uint32_t rng = 7777;
    for (uint32_t z = 0; z < D; z++)
        for (uint32_t y = 0; y < H; y++)
            for (uint32_t x = 0; x < W; x++) {
                int32_t base = (int32_t)((x * 256 + y * 128 + z * 64) % 65536);
                int32_t noise = (int32_t)(xorshift32(&rng) % 64) - 32;
                int32_t val = base + noise;
                if (val < 0) val = 0;
                if (val > 65535) val = 65535;
                vol->comps[0].data[z * W * H + y * W + x] = val;
            }

    opj_jp3d_enc_params_t enc;
    opj_jp3d_set_default_encoder_parameters(&enc);
    enc.filter      = OPJ_JP3D_FILTER_97;
    enc.target_rate = 4.0f;
    enc.num_resolutions_x = enc.num_resolutions_y = enc.num_resolutions_z = 1;
    enc.tile_width = W; enc.tile_height = H; enc.tile_depth = D;
    enc.cblk_width = W; enc.cblk_height = H; enc.cblk_depth = D;

    double psnr = lossy_roundtrip_psnr(vol, &enc, 65535.0);
    ASSERT(psnr > 30.0);
    opj_jp3d_destroy_volume(vol);
}

/* =========================================================================
 * 8. Multi-resolution decomposition levels (2,2,1) with 16x16x8
 * ========================================================================= */

static void test_multi_resolution(void)
{
    const uint32_t W = 16, H = 16, D = 8;
    opj_volume_t *vol = opj_jp3d_create_volume(1, W, H, D, 8, 0);
    ASSERT(vol != NULL);
    if (!vol) return;

    for (size_t i = 0; i < (size_t)W * H * D; i++)
        vol->comps[0].data[i] = (int32_t)((i * 6271 + 137) % 256);

    opj_jp3d_enc_params_t enc;
    opj_jp3d_set_default_encoder_parameters(&enc);
    enc.num_resolutions_x = 2;
    enc.num_resolutions_y = 2;
    enc.num_resolutions_z = 1;
    enc.tile_width = W; enc.tile_height = H; enc.tile_depth = D;
    enc.cblk_width = W; enc.cblk_height = H; enc.cblk_depth = D;

    ASSERT(roundtrip_with_params(vol, &enc));
    opj_jp3d_destroy_volume(vol);
}

/* =========================================================================
 * 9. Dense data: all unique values
 * ========================================================================= */

static void test_dense_unique_values(void)
{
    const uint32_t W = 8, H = 8, D = 4;
    opj_volume_t *vol = opj_jp3d_create_volume(1, W, H, D, 8, 0);
    ASSERT(vol != NULL);
    if (!vol) return;

    size_t n = (size_t)W * H * D; /* 256 — fits exactly in 8-bit range */
    for (size_t i = 0; i < n; i++)
        vol->comps[0].data[i] = (int32_t)(i % 256);

    opj_jp3d_enc_params_t enc;
    opj_jp3d_set_default_encoder_parameters(&enc);
    enc.num_resolutions_x = enc.num_resolutions_y = enc.num_resolutions_z = 1;
    enc.tile_width = W; enc.tile_height = H; enc.tile_depth = D;
    enc.cblk_width = W; enc.cblk_height = H; enc.cblk_depth = D;

    ASSERT(roundtrip_with_params(vol, &enc));
    opj_jp3d_destroy_volume(vol);
}

/* =========================================================================
 * 10. Constant data: single value everywhere
 * ========================================================================= */

static void test_constant_data(void)
{
    const uint32_t W = 16, H = 16, D = 8;
    opj_volume_t *vol = opj_jp3d_create_volume(1, W, H, D, 8, 0);
    ASSERT(vol != NULL);
    if (!vol) return;

    size_t n = (size_t)W * H * D;
    for (size_t i = 0; i < n; i++)
        vol->comps[0].data[i] = 128;

    opj_jp3d_enc_params_t enc;
    opj_jp3d_set_default_encoder_parameters(&enc);
    enc.num_resolutions_x = enc.num_resolutions_y = enc.num_resolutions_z = 1;
    enc.tile_width = W; enc.tile_height = H; enc.tile_depth = D;
    enc.cblk_width = W; enc.cblk_height = H; enc.cblk_depth = D;

    ASSERT(roundtrip_with_params(vol, &enc));
    opj_jp3d_destroy_volume(vol);
}

/* =========================================================================
 * main
 * ========================================================================= */

int main(void)
{
    test_ct_like_volume();
    test_mri_like_volume();
    test_satellite_volume();
    test_microscopy_volume();
    test_ct_multitile();
    test_large_htj2k();
    test_lossy_medical();
    test_multi_resolution();
    test_dense_unique_values();
    test_constant_data();

    printf("Passed %d/%d tests\n", pass_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}
