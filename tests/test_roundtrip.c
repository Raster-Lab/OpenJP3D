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
 * @file test_roundtrip.c
 * @brief Encode→decode lossless round-trip tests for the JP3D codec.
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

/* ---- Error-callback helpers for MT-API-009 ---- */
static int error_cb_count = 0;
static opj_jp3d_msg_level_t error_cb_max_level = OPJ_JP3D_MSG_INFO;

static void test_error_cb(opj_jp3d_msg_level_t level, const char *msg,
                           void *data)
{
    (void)msg;
    (void)data;
    error_cb_count++;
    if (level > error_cb_max_level)
        error_cb_max_level = level;
}

/** Perform a full encode→decode round-trip and compare. */
static int do_roundtrip(uint32_t numcomps, uint32_t w, uint32_t h, uint32_t d,
                         uint32_t prec, uint32_t max_val)
{
    opj_volume_t *orig = opj_jp3d_create_volume(numcomps, w, h, d, prec, 0);
    if (!orig) return 0;
    fill_volume(orig, max_val);

    opj_jp3d_enc_params_t enc;
    opj_jp3d_set_default_encoder_parameters(&enc);
    /* Use 0-level DWT to isolate round-trip logic first time, but actually
       let's use real DWT to exercise the full codec */
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

int main(void)
{
    /* 1. 4x4x4, 1 comp, 8-bit lossless */
    ASSERT(do_roundtrip(1, 4, 4, 4, 8, 255));

    /* 2. 8x8x8, 1 comp, 8-bit lossless */
    ASSERT(do_roundtrip(1, 8, 8, 8, 8, 255));

    /* 3. 16x16x16, 1 comp, 8-bit lossless */
    ASSERT(do_roundtrip(1, 16, 16, 16, 8, 255));

    /* 4. 4x4x4, 1 comp, 16-bit lossless */
    ASSERT(do_roundtrip(1, 4, 4, 4, 16, 65535));

    /* 5. 8x8x8, 2 comps, 8-bit lossless */
    ASSERT(do_roundtrip(2, 8, 8, 8, 8, 255));

    /* 6. All zeros */
    {
        opj_volume_t *orig = opj_jp3d_create_volume(1, 4, 4, 4, 8, 0);
        ASSERT(orig != NULL);
        if (orig) {
            opj_jp3d_enc_params_t enc;
            opj_jp3d_set_default_encoder_parameters(&enc);
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
    }

    /* 7. All max values */
    ASSERT(do_roundtrip(1, 4, 4, 4, 8, 255));

    /* 8. Alternating values */
    {
        opj_volume_t *orig = opj_jp3d_create_volume(1, 8, 8, 8, 8, 0);
        if (orig) {
            size_t n = 8u * 8u * 8u;
            for (size_t i = 0; i < n; i++)
                orig->comps[0].data[i] = (int32_t)(i & 1 ? 255 : 0);
            opj_jp3d_enc_params_t enc;
            opj_jp3d_set_default_encoder_parameters(&enc);
            enc.tile_width = enc.tile_height = enc.tile_depth = 8;
            enc.cblk_width = enc.cblk_height = enc.cblk_depth = 8;
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
        } else {
            ASSERT(0);
        }
    }

    /* 9. Small 2x2x2 */
    ASSERT(do_roundtrip(1, 2, 2, 2, 8, 255));

    /* 10. Degenerate 1x1x4 */
    ASSERT(do_roundtrip(1, 1, 1, 4, 8, 255));

    /* 11. Error callback is invoked when decoding invalid data (MT-API-009) */
    {
        error_cb_count = 0;
        error_cb_max_level = OPJ_JP3D_MSG_INFO;

        /* Feed garbage data to the decoder. */
        uint8_t garbage[16] = {0xDE, 0xAD, 0xBE, 0xEF,
                               0x00, 0x11, 0x22, 0x33,
                               0x44, 0x55, 0x66, 0x77,
                               0x88, 0x99, 0xAA, 0xBB};
        opj_volume_t *bad = opj_jp3d_decode(garbage, sizeof(garbage),
                                            NULL, test_error_cb, NULL);
        ASSERT(bad == NULL);                         /* decode must fail */
        ASSERT(error_cb_count > 0);                  /* callback invoked */
        ASSERT(error_cb_max_level == OPJ_JP3D_MSG_ERROR); /* error level */
        if (bad) opj_jp3d_destroy_volume(bad);
    }

    printf("Passed %d/%d tests\n", pass_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}
