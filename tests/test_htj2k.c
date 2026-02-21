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
 * @file test_htj2k.c
 * @brief Tests for the HTJ2K high-throughput block coder and transcoding API.
 *
 * Covers:
 *  - HT encode→decode round-trips at various block sizes and bit-depths.
 *  - API flag: OPJ_JP3D_USE_HTJ2K propagated through encode/decode.
 *  - opj_jp3d_transcode_to_ht() lossless transcoding from EBCOT to HT.
 *  - Edge cases: all-zero blocks, single-sample blocks, maximum values.
 */

#include "openjp3d.h"
/* Access HT block-level functions directly for unit tests */
#include "opj_ht3d.h"

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

/** Fill a volume with deterministic non-zero values in [1, max_val]. */
static void fill_volume(opj_volume_t *vol, uint32_t max_val)
{
    for (uint32_t c = 0; c < vol->numcomps; c++) {
        opj_volume_comp_t *comp = &vol->comps[c];
        size_t n = (size_t)comp->w * comp->h * comp->d;
        for (size_t i = 0; i < n; i++)
            comp->data[i] = (int32_t)((i * 6271 + c * 137) % (max_val + 1));
    }
}

/** Compare two volumes sample-by-sample. */
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

/* =========================================================================
 * HT block-level unit tests (opj_ht3d_encode_cblk / opj_ht3d_decode_cblk)
 * ========================================================================= */

/** Test HT round-trip for a given block of samples. */
static int ht_cblk_roundtrip(const int32_t *orig, uint32_t w, uint32_t h,
                               uint32_t d)
{
    size_t   nsamp  = (size_t)w * h * d;
    /* Use a generous bound to avoid under-allocation for tiny blocks */
    size_t   cap    = 128 + nsamp * 12;
    uint8_t *enc    = (uint8_t *)malloc(cap);
    int32_t *dec    = (int32_t *)malloc(nsamp * sizeof(int32_t));
    if (!enc || !dec) { free(enc); free(dec); return 0; }

    size_t enc_len = opj_ht3d_encode_cblk(orig, w, h, d, enc, cap);
    if (enc_len == 0) { free(enc); free(dec); return 0; }

    int ok = opj_ht3d_decode_cblk(enc, enc_len, dec, w, h, d);
    if (!ok) { free(enc); free(dec); return 0; }

    ok = (memcmp(orig, dec, nsamp * sizeof(int32_t)) == 0);
    free(enc);
    free(dec);
    return ok;
}

static void test_ht_cblk_all_zeros(void)
{
    int32_t data[8] = {0};
    ASSERT(ht_cblk_roundtrip(data, 2, 2, 2));
}

static void test_ht_cblk_single_one(void)
{
    int32_t data[8] = {0};
    data[3] = 1;
    ASSERT(ht_cblk_roundtrip(data, 2, 2, 2));
}

static void test_ht_cblk_single_neg(void)
{
    int32_t data[8] = {0};
    data[5] = -7;
    ASSERT(ht_cblk_roundtrip(data, 2, 2, 2));
}

static void test_ht_cblk_4x4x4_random(void)
{
    int32_t data[64];
    for (int i = 0; i < 64; i++)
        data[i] = (int32_t)((i * 31337 + 17) % 256);
    ASSERT(ht_cblk_roundtrip(data, 4, 4, 4));
}

static void test_ht_cblk_4x4x4_signed(void)
{
    int32_t data[64];
    for (int i = 0; i < 64; i++)
        data[i] = (int32_t)(((i * 31337 + 17) % 511) - 255);
    ASSERT(ht_cblk_roundtrip(data, 4, 4, 4));
}

static void test_ht_cblk_8x8x8(void)
{
    int32_t data[512];
    for (int i = 0; i < 512; i++)
        data[i] = (int32_t)((i * 6271 + 137) % 256);
    ASSERT(ht_cblk_roundtrip(data, 8, 8, 8));
}

static void test_ht_cblk_16bit(void)
{
    int32_t data[64];
    for (int i = 0; i < 64; i++)
        data[i] = (int32_t)((i * 1031 + 3) % 65536);
    ASSERT(ht_cblk_roundtrip(data, 4, 4, 4));
}

static void test_ht_cblk_max_value(void)
{
    int32_t data[8];
    for (int i = 0; i < 8; i++) data[i] = 65535;
    ASSERT(ht_cblk_roundtrip(data, 2, 2, 2));
}

static void test_ht_cblk_alternating(void)
{
    int32_t data[64];
    for (int i = 0; i < 64; i++)
        data[i] = (i & 1) ? 255 : 0;
    ASSERT(ht_cblk_roundtrip(data, 4, 4, 4));
}

static void test_ht_cblk_1x1x1(void)
{
    int32_t data = 42;
    ASSERT(ht_cblk_roundtrip(&data, 1, 1, 1));
}

static void test_ht_cblk_1x1x4(void)
{
    int32_t data[4] = {10, 0, -5, 200};
    ASSERT(ht_cblk_roundtrip(data, 1, 1, 4));
}

static void test_ht_cblk_all_same(void)
{
    int32_t data[27];
    for (int i = 0; i < 27; i++) data[i] = 128;
    ASSERT(ht_cblk_roundtrip(data, 3, 3, 3));
}

static void test_ht_cblk_large_magnitude(void)
{
    int32_t data[8] = {1 << 20, -(1 << 18), 0, 1 << 15, 0, -(1 << 10), 7, -1};
    ASSERT(ht_cblk_roundtrip(data, 2, 2, 2));
}

static void test_ht_cblk_sparse(void)
{
    int32_t data[64];
    memset(data, 0, sizeof(data));
    data[0]  = 1;
    data[63] = -1;
    ASSERT(ht_cblk_roundtrip(data, 4, 4, 4));
}

/* =========================================================================
 * Full codec round-trip with HT mode (OPJ_JP3D_USE_HTJ2K)
 * ========================================================================= */

static int ht_full_roundtrip(uint32_t numcomps, uint32_t w, uint32_t h,
                               uint32_t d, uint32_t prec, uint32_t max_val)
{
    opj_volume_t *orig = opj_jp3d_create_volume(numcomps, w, h, d, prec, 0);
    if (!orig) return 0;
    fill_volume(orig, max_val);

    opj_jp3d_enc_params_t enc;
    opj_jp3d_set_default_encoder_parameters(&enc);
    enc.use_htj2k         = OPJ_JP3D_USE_HTJ2K;
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

static void test_ht_full_4x4x4_8bit(void)
{
    ASSERT(ht_full_roundtrip(1, 4, 4, 4, 8, 255));
}

static void test_ht_full_8x8x8_8bit(void)
{
    ASSERT(ht_full_roundtrip(1, 8, 8, 8, 8, 255));
}

static void test_ht_full_16x16x16_8bit(void)
{
    ASSERT(ht_full_roundtrip(1, 16, 16, 16, 8, 255));
}

static void test_ht_full_4x4x4_16bit(void)
{
    ASSERT(ht_full_roundtrip(1, 4, 4, 4, 16, 65535));
}

static void test_ht_full_multicomp(void)
{
    ASSERT(ht_full_roundtrip(3, 8, 8, 8, 8, 255));
}

static void test_ht_full_allzero(void)
{
    opj_volume_t *orig = opj_jp3d_create_volume(1, 4, 4, 4, 8, 0);
    ASSERT(orig != NULL);
    if (!orig) return;
    /* data already zero from calloc */

    opj_jp3d_enc_params_t enc;
    opj_jp3d_set_default_encoder_parameters(&enc);
    enc.use_htj2k         = OPJ_JP3D_USE_HTJ2K;
    enc.tile_width = enc.tile_height = enc.tile_depth = 4;
    enc.cblk_width = enc.cblk_height = enc.cblk_depth = 4;
    enc.num_resolutions_x = enc.num_resolutions_y = enc.num_resolutions_z = 1;

    uint8_t *cs = NULL; size_t cs_sz = 0;
    int enc_ok = opj_jp3d_encode(orig, &enc, &cs, &cs_sz, NULL, NULL);
    ASSERT(enc_ok);
    if (enc_ok) {
        opj_volume_t *dec = opj_jp3d_decode(cs, cs_sz, NULL, NULL, NULL);
        opj_jp3d_free(cs);
        ASSERT(dec != NULL && volumes_equal(orig, dec));
        opj_jp3d_destroy_volume(dec);
    }
    opj_jp3d_destroy_volume(orig);
}

static void test_ht_full_1x1x4(void)
{
    ASSERT(ht_full_roundtrip(1, 1, 1, 4, 8, 255));
}

static void test_ht_full_2x2x2(void)
{
    ASSERT(ht_full_roundtrip(1, 2, 2, 2, 8, 255));
}

/* =========================================================================
 * Transcode: EBCOT → HT round-trip
 * ========================================================================= */

static void test_transcode_to_ht_basic(void)
{
    /* 1. Encode with EBCOT */
    opj_volume_t *orig = opj_jp3d_create_volume(1, 8, 8, 8, 8, 0);
    ASSERT(orig != NULL);
    if (!orig) return;
    fill_volume(orig, 200);

    opj_jp3d_enc_params_t enc;
    opj_jp3d_set_default_encoder_parameters(&enc);
    enc.use_htj2k         = 0; /* EBCOT */
    enc.tile_width = enc.tile_height = enc.tile_depth = 8;
    enc.cblk_width = enc.cblk_height = enc.cblk_depth = 8;
    enc.num_resolutions_x = enc.num_resolutions_y = enc.num_resolutions_z = 1;

    uint8_t *ebcot_cs = NULL; size_t ebcot_sz = 0;
    int enc_ok = opj_jp3d_encode(orig, &enc, &ebcot_cs, &ebcot_sz, NULL, NULL);
    ASSERT(enc_ok);
    if (!enc_ok) { opj_jp3d_destroy_volume(orig); return; }

    /* 2. Transcode to HT */
    uint8_t *ht_cs = NULL; size_t ht_sz = 0;
    int tc_ok = opj_jp3d_transcode_to_ht(ebcot_cs, ebcot_sz, NULL,
                                          &ht_cs, &ht_sz, NULL, NULL);
    opj_jp3d_free(ebcot_cs);
    ASSERT(tc_ok);
    if (!tc_ok) { opj_jp3d_destroy_volume(orig); return; }

    /* 3. Decode HT codestream — must equal original */
    opj_volume_t *dec = opj_jp3d_decode(ht_cs, ht_sz, NULL, NULL, NULL);
    opj_jp3d_free(ht_cs);
    ASSERT(dec != NULL);
    if (dec) {
        ASSERT(volumes_equal(orig, dec));
        opj_jp3d_destroy_volume(dec);
    }
    opj_jp3d_destroy_volume(orig);
}

static void test_transcode_ht_to_ht(void)
{
    /* Transcoding an already-HT stream should still produce correct output. */
    opj_volume_t *orig = opj_jp3d_create_volume(1, 4, 4, 4, 8, 0);
    ASSERT(orig != NULL);
    if (!orig) return;
    fill_volume(orig, 100);

    opj_jp3d_enc_params_t enc;
    opj_jp3d_set_default_encoder_parameters(&enc);
    enc.use_htj2k         = OPJ_JP3D_USE_HTJ2K;
    enc.tile_width = enc.tile_height = enc.tile_depth = 4;
    enc.cblk_width = enc.cblk_height = enc.cblk_depth = 4;
    enc.num_resolutions_x = enc.num_resolutions_y = enc.num_resolutions_z = 1;

    uint8_t *ht1 = NULL; size_t ht1_sz = 0;
    ASSERT(opj_jp3d_encode(orig, &enc, &ht1, &ht1_sz, NULL, NULL));

    uint8_t *ht2 = NULL; size_t ht2_sz = 0;
    int tc_ok = opj_jp3d_transcode_to_ht(ht1, ht1_sz, NULL, &ht2, &ht2_sz,
                                          NULL, NULL);
    opj_jp3d_free(ht1);
    ASSERT(tc_ok);
    if (tc_ok) {
        opj_volume_t *dec = opj_jp3d_decode(ht2, ht2_sz, NULL, NULL, NULL);
        opj_jp3d_free(ht2);
        ASSERT(dec != NULL && volumes_equal(orig, dec));
        opj_jp3d_destroy_volume(dec);
    }
    opj_jp3d_destroy_volume(orig);
}

static void test_transcode_multicomp(void)
{
    opj_volume_t *orig = opj_jp3d_create_volume(2, 4, 4, 4, 8, 0);
    ASSERT(orig != NULL);
    if (!orig) return;
    fill_volume(orig, 255);

    opj_jp3d_enc_params_t enc;
    opj_jp3d_set_default_encoder_parameters(&enc);
    enc.use_htj2k         = 0;
    enc.tile_width = enc.tile_height = enc.tile_depth = 4;
    enc.cblk_width = enc.cblk_height = enc.cblk_depth = 4;
    enc.num_resolutions_x = enc.num_resolutions_y = enc.num_resolutions_z = 1;

    uint8_t *ebcot = NULL; size_t ebcot_sz = 0;
    ASSERT(opj_jp3d_encode(orig, &enc, &ebcot, &ebcot_sz, NULL, NULL));

    uint8_t *ht = NULL; size_t ht_sz = 0;
    ASSERT(opj_jp3d_transcode_to_ht(ebcot, ebcot_sz, NULL, &ht, &ht_sz,
                                     NULL, NULL));
    opj_jp3d_free(ebcot);

    opj_volume_t *dec = opj_jp3d_decode(ht, ht_sz, NULL, NULL, NULL);
    opj_jp3d_free(ht);
    ASSERT(dec != NULL && volumes_equal(orig, dec));
    opj_jp3d_destroy_volume(dec);
    opj_jp3d_destroy_volume(orig);
}

/* =========================================================================
 * API: default params should have use_htj2k == 0
 * ========================================================================= */

static void test_default_params_no_htj2k(void)
{
    opj_jp3d_enc_params_t p;
    opj_jp3d_set_default_encoder_parameters(&p);
    ASSERT(p.use_htj2k == 0);
}

static void test_htj2k_flag_value(void)
{
    ASSERT(OPJ_JP3D_USE_HTJ2K == 1);
}

/* =========================================================================
 * main
 * ========================================================================= */

int main(void)
{
    /* HT block-level unit tests */
    test_ht_cblk_all_zeros();
    test_ht_cblk_single_one();
    test_ht_cblk_single_neg();
    test_ht_cblk_4x4x4_random();
    test_ht_cblk_4x4x4_signed();
    test_ht_cblk_8x8x8();
    test_ht_cblk_16bit();
    test_ht_cblk_max_value();
    test_ht_cblk_alternating();
    test_ht_cblk_1x1x1();
    test_ht_cblk_1x1x4();
    test_ht_cblk_all_same();
    test_ht_cblk_large_magnitude();
    test_ht_cblk_sparse();

    /* Full-codec round-trips */
    test_ht_full_4x4x4_8bit();
    test_ht_full_8x8x8_8bit();
    test_ht_full_16x16x16_8bit();
    test_ht_full_4x4x4_16bit();
    test_ht_full_multicomp();
    test_ht_full_allzero();
    test_ht_full_1x1x4();
    test_ht_full_2x2x2();

    /* Transcoding */
    test_transcode_to_ht_basic();
    test_transcode_ht_to_ht();
    test_transcode_multicomp();

    /* API checks */
    test_default_params_no_htj2k();
    test_htj2k_flag_value();

    printf("Passed %d/%d tests\n", pass_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}
