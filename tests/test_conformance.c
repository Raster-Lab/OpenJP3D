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
 * @file test_conformance.c
 * @brief Task 7.1 — Conformance testing: edge-case volumes and codestreams.
 *
 * Validates round-trip correctness for degenerate dimensions, high bit-depths,
 * signed samples, multi-component volumes, tiling, HTJ2K mode, and lossy
 * encoding with PSNR verification.
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

/** Fill a volume component with deterministic values in [0, max_val]. */
static void fill_volume(opj_volume_t *vol, uint32_t max_val)
{
    for (uint32_t c = 0; c < vol->numcomps; c++) {
        opj_volume_comp_t *comp = &vol->comps[c];
        size_t n = (size_t)comp->w * comp->h * comp->d;
        for (size_t i = 0; i < n; i++)
            comp->data[i] = (int32_t)((i * 6271 + c * 137) % (max_val + 1));
    }
}

/** Fill a volume component with signed values in [-half, half-1]. */
static void fill_volume_signed(opj_volume_t *vol, int32_t half)
{
    for (uint32_t c = 0; c < vol->numcomps; c++) {
        opj_volume_comp_t *comp = &vol->comps[c];
        size_t n = (size_t)comp->w * comp->h * comp->d;
        for (size_t i = 0; i < n; i++)
            comp->data[i] = (int32_t)(((int64_t)(i * 6271 + c * 137) %
                             (2 * half)) - half);
    }
}

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

/** Compute PSNR between two single-component volumes. */
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

/** Lossless round-trip with default (single-tile, 1-level DWT) params. */
static int do_roundtrip(uint32_t numcomps, uint32_t w, uint32_t h, uint32_t d,
                        uint32_t prec, uint32_t max_val, int32_t sgnd)
{
    opj_volume_t *orig = opj_jp3d_create_volume(numcomps, w, h, d, prec, sgnd);
    if (!orig) return 0;
    if (sgnd)
        fill_volume_signed(orig, (int32_t)(max_val / 2 + 1));
    else
        fill_volume(orig, max_val);

    opj_jp3d_enc_params_t enc;
    opj_jp3d_set_default_encoder_parameters(&enc);
    enc.num_resolutions_x = 1;
    enc.num_resolutions_y = 1;
    enc.num_resolutions_z = 1;
    enc.tile_width  = w;
    enc.tile_height = h;
    enc.tile_depth  = d;
    enc.cblk_width  = w;
    enc.cblk_height = h;
    enc.cblk_depth  = d;

    uint8_t *cs    = NULL;
    size_t   cs_sz = 0;
    if (!opj_jp3d_encode(orig, &enc, &cs, &cs_sz, NULL, NULL)) {
        opj_jp3d_destroy_volume(orig);
        return 0;
    }

    opj_volume_t *dec = opj_jp3d_decode(cs, cs_sz, NULL, NULL, NULL);
    opj_jp3d_free(cs);
    if (!dec) {
        opj_jp3d_destroy_volume(orig);
        return 0;
    }

    int ok = volumes_equal(orig, dec);
    opj_jp3d_destroy_volume(orig);
    opj_jp3d_destroy_volume(dec);
    return ok;
}

/* =========================================================================
 * 1. Single-slice volume (d=1)
 * ========================================================================= */

static void test_single_slice(void)
{
    ASSERT(do_roundtrip(1, 8, 8, 1, 8, 255, 0));
}

/* =========================================================================
 * 2. Single-row volume (h=1)
 * ========================================================================= */

static void test_single_row(void)
{
    ASSERT(do_roundtrip(1, 8, 1, 4, 8, 255, 0));
}

/* =========================================================================
 * 3. Single-column volume (w=1)
 * ========================================================================= */

static void test_single_column(void)
{
    ASSERT(do_roundtrip(1, 1, 8, 4, 8, 255, 0));
}

/* =========================================================================
 * 4. 1x1x1 minimal volume
 * ========================================================================= */

static void test_1x1x1(void)
{
    ASSERT(do_roundtrip(1, 1, 1, 1, 8, 255, 0));
}

/* =========================================================================
 * 5. Maximum 8-bit value coverage (all 0-255 in 16x16x1)
 * ========================================================================= */

static void test_all_8bit_values(void)
{
    opj_volume_t *orig = opj_jp3d_create_volume(1, 16, 16, 1, 8, 0);
    ASSERT(orig != NULL);
    if (!orig) return;

    /* Fill with all 256 values */
    for (int i = 0; i < 256; i++)
        orig->comps[0].data[i] = (int32_t)i;

    opj_jp3d_enc_params_t enc;
    opj_jp3d_set_default_encoder_parameters(&enc);
    enc.num_resolutions_x = enc.num_resolutions_y = enc.num_resolutions_z = 1;
    enc.tile_width = 16; enc.tile_height = 16; enc.tile_depth = 1;
    enc.cblk_width = 16; enc.cblk_height = 16; enc.cblk_depth = 1;

    uint8_t *cs = NULL; size_t cs_sz = 0;
    int enc_ok = opj_jp3d_encode(orig, &enc, &cs, &cs_sz, NULL, NULL);
    ASSERT(enc_ok);
    if (enc_ok) {
        opj_volume_t *dec = opj_jp3d_decode(cs, cs_sz, NULL, NULL, NULL);
        opj_jp3d_free(cs);
        ASSERT(dec != NULL && volumes_equal(orig, dec));
        if (dec) opj_jp3d_destroy_volume(dec);
    }
    opj_jp3d_destroy_volume(orig);
}

/* =========================================================================
 * 6. 16-bit precision round-trip
 * ========================================================================= */

static void test_16bit_precision(void)
{
    ASSERT(do_roundtrip(1, 8, 8, 4, 16, 65535, 0));
}

/* =========================================================================
 * 7. 32-bit precision round-trip (prec=32, max_val up to 2^30)
 * ========================================================================= */

static void test_32bit_precision(void)
{
    ASSERT(do_roundtrip(1, 4, 4, 4, 32, (uint32_t)(1u << 30), 0));
}

/* =========================================================================
 * 8. Signed 8-bit samples (-128..127)
 * ========================================================================= */

static void test_signed_8bit(void)
{
    ASSERT(do_roundtrip(1, 8, 8, 4, 8, 255, 1));
}

/* =========================================================================
 * 9. Signed 16-bit samples
 * ========================================================================= */

static void test_signed_16bit(void)
{
    ASSERT(do_roundtrip(1, 8, 8, 4, 16, 65535, 1));
}

/* =========================================================================
 * 10. Multi-component (3 components)
 * ========================================================================= */

static void test_3_components(void)
{
    ASSERT(do_roundtrip(3, 8, 8, 4, 8, 255, 0));
}

/* =========================================================================
 * 11. Multi-component (4 components, RGBA-like)
 * ========================================================================= */

static void test_4_components(void)
{
    ASSERT(do_roundtrip(4, 8, 8, 4, 8, 255, 0));
}

/* =========================================================================
 * 12. Tiled encoding (tiles smaller than volume)
 * ========================================================================= */

static void test_tiled_encoding(void)
{
    opj_volume_t *orig = opj_jp3d_create_volume(1, 16, 16, 8, 8, 0);
    ASSERT(orig != NULL);
    if (!orig) return;
    fill_volume(orig, 255);

    opj_jp3d_enc_params_t enc;
    opj_jp3d_set_default_encoder_parameters(&enc);
    enc.num_resolutions_x = enc.num_resolutions_y = enc.num_resolutions_z = 1;
    enc.tile_width = 8; enc.tile_height = 8; enc.tile_depth = 4;
    enc.cblk_width = 8; enc.cblk_height = 8; enc.cblk_depth = 4;

    uint8_t *cs = NULL; size_t cs_sz = 0;
    int enc_ok = opj_jp3d_encode(orig, &enc, &cs, &cs_sz, NULL, NULL);
    ASSERT(enc_ok);
    if (enc_ok) {
        opj_volume_t *dec = opj_jp3d_decode(cs, cs_sz, NULL, NULL, NULL);
        opj_jp3d_free(cs);
        ASSERT(dec != NULL && volumes_equal(orig, dec));
        if (dec) opj_jp3d_destroy_volume(dec);
    }
    opj_jp3d_destroy_volume(orig);
}

/* =========================================================================
 * 13. Non-power-of-2 dimensions (7x5x3)
 * ========================================================================= */

static void test_non_power_of_2(void)
{
    ASSERT(do_roundtrip(1, 7, 5, 3, 8, 255, 0));
}

/* =========================================================================
 * 14. Large volume (64x64x64)
 * ========================================================================= */

static void test_large_volume(void)
{
    ASSERT(do_roundtrip(1, 64, 64, 64, 8, 255, 0));
}

/* =========================================================================
 * 15. Asymmetric dimensions (2x32x4)
 * ========================================================================= */

static void test_asymmetric_dims(void)
{
    ASSERT(do_roundtrip(1, 2, 32, 4, 8, 255, 0));
}

/* =========================================================================
 * 16. HTJ2K mode round-trip
 * ========================================================================= */

static void test_htj2k_roundtrip(void)
{
    opj_volume_t *orig = opj_jp3d_create_volume(1, 8, 8, 8, 8, 0);
    ASSERT(orig != NULL);
    if (!orig) return;
    fill_volume(orig, 255);

    opj_jp3d_enc_params_t enc;
    opj_jp3d_set_default_encoder_parameters(&enc);
    enc.use_htj2k = OPJ_JP3D_USE_HTJ2K;
    enc.num_resolutions_x = enc.num_resolutions_y = enc.num_resolutions_z = 1;
    enc.tile_width = 8; enc.tile_height = 8; enc.tile_depth = 8;
    enc.cblk_width = 8; enc.cblk_height = 8; enc.cblk_depth = 8;

    uint8_t *cs = NULL; size_t cs_sz = 0;
    int enc_ok = opj_jp3d_encode(orig, &enc, &cs, &cs_sz, NULL, NULL);
    ASSERT(enc_ok);
    if (enc_ok) {
        opj_volume_t *dec = opj_jp3d_decode(cs, cs_sz, NULL, NULL, NULL);
        opj_jp3d_free(cs);
        ASSERT(dec != NULL && volumes_equal(orig, dec));
        if (dec) opj_jp3d_destroy_volume(dec);
    }
    opj_jp3d_destroy_volume(orig);
}

/* =========================================================================
 * 17. Lossy mode (9/7 filter) with PSNR check
 * ========================================================================= */

static void test_lossy_psnr(void)
{
    opj_volume_t *orig = opj_jp3d_create_volume(1, 16, 16, 8, 8, 0);
    ASSERT(orig != NULL);
    if (!orig) return;
    fill_volume(orig, 255);

    opj_jp3d_enc_params_t enc;
    opj_jp3d_set_default_encoder_parameters(&enc);
    enc.filter      = OPJ_JP3D_FILTER_97;
    enc.target_rate = 2.0f;
    enc.num_resolutions_x = enc.num_resolutions_y = enc.num_resolutions_z = 1;
    enc.tile_width = 16; enc.tile_height = 16; enc.tile_depth = 8;
    enc.cblk_width = 16; enc.cblk_height = 16; enc.cblk_depth = 8;

    uint8_t *cs = NULL; size_t cs_sz = 0;
    int enc_ok = opj_jp3d_encode(orig, &enc, &cs, &cs_sz, NULL, NULL);
    ASSERT(enc_ok);
    if (enc_ok) {
        opj_volume_t *dec = opj_jp3d_decode(cs, cs_sz, NULL, NULL, NULL);
        opj_jp3d_free(cs);
        ASSERT(dec != NULL);
        if (dec) {
            double psnr = compute_psnr(orig, dec, 255.0);
            ASSERT(psnr > 20.0);
            opj_jp3d_destroy_volume(dec);
        }
    }
    opj_jp3d_destroy_volume(orig);
}

/* =========================================================================
 * main
 * ========================================================================= */

int main(void)
{
    test_single_slice();
    test_single_row();
    test_single_column();
    test_1x1x1();
    test_all_8bit_values();
    test_16bit_precision();
    test_32bit_precision();
    test_signed_8bit();
    test_signed_16bit();
    test_3_components();
    test_4_components();
    test_tiled_encoding();
    test_non_power_of_2();
    test_large_volume();
    test_asymmetric_dims();
    test_htj2k_roundtrip();
    test_lossy_psnr();

    printf("Passed %d/%d tests\n", pass_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}
