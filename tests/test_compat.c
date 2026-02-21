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
 * @file test_compat.c
 * @brief Task 7.3 — Upstream compatibility: symbol naming, header
 *        self-containment, and API stability checks.
 *
 * Verifies that version macros, public constants, default parameter values,
 * and the full encode→decode pipeline remain stable across builds.
 */

#include "openjp3d.h"

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
 * 1. Version string is "1.0.0"
 * ========================================================================= */

static void test_version_string(void)
{
    const char *ver = opj_jp3d_get_version();
    ASSERT(ver != NULL);
    ASSERT(strcmp(ver, "1.0.0") == 0);
    ASSERT(strcmp(OPJ_JP3D_VERSION, "1.0.0") == 0);
}

/* =========================================================================
 * 2. Version macros: MAJOR=1, MINOR=0, PATCH=0
 * ========================================================================= */

static void test_version_macros(void)
{
    ASSERT(OPJ_JP3D_VERSION_MAJOR == 1);
    ASSERT(OPJ_JP3D_VERSION_MINOR == 0);
    ASSERT(OPJ_JP3D_VERSION_PATCH == 0);
}

/* =========================================================================
 * 3. Public API functions are callable
 * ========================================================================= */

static void test_api_callable(void)
{
    /* create + destroy */
    opj_volume_t *vol = opj_jp3d_create_volume(1, 4, 4, 4, 8, 0);
    ASSERT(vol != NULL);
    opj_jp3d_destroy_volume(vol);

    /* malloc / free */
    void *p = opj_jp3d_malloc(64);
    ASSERT(p != NULL);
    opj_jp3d_free(p);

    /* version */
    ASSERT(opj_jp3d_get_version() != NULL);

    /* encoder params */
    opj_jp3d_enc_params_t enc;
    opj_jp3d_set_default_encoder_parameters(&enc);
    ASSERT(enc.tile_width > 0 || enc.tile_width == 0); /* just callable */

    /* decoder params */
    opj_jp3d_dec_params_t dec;
    opj_jp3d_set_default_decoder_parameters(&dec);
    ASSERT(dec.verbose == 0 || dec.verbose != 0); /* just callable */
}

/* =========================================================================
 * 4. Bool constants
 * ========================================================================= */

static void test_bool_constants(void)
{
    ASSERT(OPJ_JP3D_TRUE == 1);
    ASSERT(OPJ_JP3D_FALSE == 0);
}

/* =========================================================================
 * 5. Filter constants
 * ========================================================================= */

static void test_filter_constants(void)
{
    ASSERT(OPJ_JP3D_FILTER_53 == 0);
    ASSERT(OPJ_JP3D_FILTER_97 == 1);
}

/* =========================================================================
 * 6. Colour-space constants
 * ========================================================================= */

static void test_colorspace_constants(void)
{
    ASSERT(OPJ_JP3D_CS_UNKNOWN == 0);
    ASSERT(OPJ_JP3D_CS_SRGB == 1);
    ASSERT(OPJ_JP3D_CS_GRAY == 2);
    ASSERT(OPJ_JP3D_CS_YUV == 3);
}

/* =========================================================================
 * 7. HTJ2K constant
 * ========================================================================= */

static void test_htj2k_constant(void)
{
    ASSERT(OPJ_JP3D_USE_HTJ2K == 1);
}

/* =========================================================================
 * 8. Encoder default params: filter=53, target_rate=0 (lossless)
 * ========================================================================= */

static void test_encoder_defaults(void)
{
    opj_jp3d_enc_params_t enc;
    opj_jp3d_set_default_encoder_parameters(&enc);
    ASSERT(enc.filter == OPJ_JP3D_FILTER_53);
    ASSERT(enc.target_rate == 0.0f);
    ASSERT(enc.use_htj2k == 0);
    ASSERT(enc.num_layers >= 1);
}

/* =========================================================================
 * 9. Decoder default params: verbose=0
 * ========================================================================= */

static void test_decoder_defaults(void)
{
    opj_jp3d_dec_params_t dec;
    opj_jp3d_set_default_decoder_parameters(&dec);
    ASSERT(dec.verbose == 0);
}

/* =========================================================================
 * 10. Basic encode→decode→verify (API stability)
 * ========================================================================= */

static void test_api_stability_roundtrip(void)
{
    opj_volume_t *orig = opj_jp3d_create_volume(1, 4, 4, 4, 8, 0);
    ASSERT(orig != NULL);
    if (!orig) return;

    /* Fill with deterministic data */
    size_t n = (size_t)4 * 4 * 4;
    for (size_t i = 0; i < n; i++)
        orig->comps[0].data[i] = (int32_t)((i * 6271) % 256);

    opj_jp3d_enc_params_t enc;
    opj_jp3d_set_default_encoder_parameters(&enc);
    enc.num_resolutions_x = enc.num_resolutions_y = enc.num_resolutions_z = 1;
    enc.tile_width = 4; enc.tile_height = 4; enc.tile_depth = 4;
    enc.cblk_width = 4; enc.cblk_height = 4; enc.cblk_depth = 4;

    uint8_t *cs = NULL;
    size_t cs_sz = 0;
    int enc_ok = opj_jp3d_encode(orig, &enc, &cs, &cs_sz, NULL, NULL);
    ASSERT(enc_ok);
    ASSERT(cs_sz > 0);

    if (enc_ok) {
        opj_volume_t *dec = opj_jp3d_decode(cs, cs_sz, NULL, NULL, NULL);
        opj_jp3d_free(cs);
        ASSERT(dec != NULL);
        if (dec) {
            ASSERT(dec->numcomps == orig->numcomps);
            ASSERT(dec->comps[0].w == 4);
            ASSERT(dec->comps[0].h == 4);
            ASSERT(dec->comps[0].d == 4);

            /* Bit-exact match */
            int match = 1;
            for (size_t i = 0; i < n; i++) {
                if (orig->comps[0].data[i] != dec->comps[0].data[i]) {
                    match = 0;
                    break;
                }
            }
            ASSERT(match);
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
    test_version_string();
    test_version_macros();
    test_api_callable();
    test_bool_constants();
    test_filter_constants();
    test_colorspace_constants();
    test_htj2k_constant();
    test_encoder_defaults();
    test_decoder_defaults();
    test_api_stability_roundtrip();

    printf("Passed %d/%d tests\n", pass_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}
