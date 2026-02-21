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
 * @file htj2k_encode.c
 * @brief Example: Encode a volume using the HTJ2K high-throughput block coder.
 *
 * This program creates a test volume, encodes it with the HTJ2K block coder
 * (JPEG 2000 Part 15 adapted for 3-D), decodes it, and verifies lossless
 * round-trip.
 *
 * Build (assuming OpenJP3D is installed):
 * @code
 *   cc -o htj2k_encode htj2k_encode.c -lopenjp3d
 * @endcode
 */

#include "openjp3d.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** @brief Volume dimensions. */
#define VOL_W 16
#define VOL_H 16
#define VOL_D 8

int main(void)
{
    int ret = EXIT_SUCCESS;
    printf("OpenJP3D htj2k_encode example (version %s)\n",
           opj_jp3d_get_version());

    /* ---- Create a test volume ------------------------------------------ */

    opj_volume_t *vol = opj_jp3d_create_volume(1, VOL_W, VOL_H, VOL_D, 8, 0);
    if (!vol) {
        fprintf(stderr, "Failed to create volume\n");
        return EXIT_FAILURE;
    }

    const uint32_t total = VOL_W * VOL_H * VOL_D;
    for (uint32_t i = 0; i < total; i++)
        vol->comps[0].data[i] = (int32_t)(i % 251);  /* Arbitrary pattern */

    /* ---- Encode with HTJ2K --------------------------------------------- */

    opj_jp3d_enc_params_t params;
    opj_jp3d_set_default_encoder_parameters(&params);
    params.filter    = OPJ_JP3D_FILTER_53;     /* Lossless */
    params.use_htj2k = OPJ_JP3D_USE_HTJ2K;    /* Enable HT block coder */

    uint8_t *cs = NULL;
    size_t   cs_size = 0;
    if (!opj_jp3d_encode(vol, &params, &cs, &cs_size, NULL, NULL)) {
        fprintf(stderr, "HTJ2K encoding failed\n");
        opj_jp3d_destroy_volume(vol);
        return EXIT_FAILURE;
    }
    printf("HTJ2K encoded: %zu bytes\n", cs_size);

    /* ---- Decode -------------------------------------------------------- */

    opj_volume_t *dec = opj_jp3d_decode(cs, cs_size, NULL, NULL, NULL);
    if (!dec) {
        fprintf(stderr, "Decoding failed\n");
        opj_jp3d_free(cs);
        opj_jp3d_destroy_volume(vol);
        return EXIT_FAILURE;
    }

    /* ---- Verify lossless round-trip ------------------------------------ */

    printf("Verifying lossless round-trip... ");
    int match = 1;
    for (uint32_t i = 0; i < total; i++) {
        if (vol->comps[0].data[i] != dec->comps[0].data[i]) {
            fprintf(stderr, "\nMismatch at index %u: %d != %d\n",
                    i, vol->comps[0].data[i], dec->comps[0].data[i]);
            match = 0;
            ret = EXIT_FAILURE;
            break;
        }
    }
    if (match)
        printf("OK — all %u samples match.\n", total);

    /* ---- Transcode from EBCOT to HTJ2K -------------------------------- */

    /* First encode with EBCOT. */
    opj_jp3d_enc_params_t ebcot_params;
    opj_jp3d_set_default_encoder_parameters(&ebcot_params);

    uint8_t *ebcot_cs = NULL;
    size_t   ebcot_size = 0;
    if (opj_jp3d_encode(vol, &ebcot_params, &ebcot_cs, &ebcot_size,
                         NULL, NULL)) {
        printf("EBCOT encoded: %zu bytes\n", ebcot_size);

        /* Transcode EBCOT → HTJ2K. */
        uint8_t *ht_cs = NULL;
        size_t   ht_size = 0;
        if (opj_jp3d_transcode_to_ht(ebcot_cs, ebcot_size, NULL,
                                      &ht_cs, &ht_size, NULL, NULL)) {
            printf("Transcoded to HTJ2K: %zu bytes\n", ht_size);
            opj_jp3d_free(ht_cs);
        }
        opj_jp3d_free(ebcot_cs);
    }

    /* ---- Cleanup ------------------------------------------------------- */

    opj_jp3d_free(cs);
    opj_jp3d_destroy_volume(vol);
    opj_jp3d_destroy_volume(dec);

    printf("Done.\n");
    return ret;
}
