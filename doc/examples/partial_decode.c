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
 * @file partial_decode.c
 * @brief Example: Encode a volume, then decode a sub-region via JPIP 3-D.
 *
 * This program demonstrates partial (sub-volume) decoding using the JPIP 3-D
 * client/server API. It creates a test volume, encodes it, loads the
 * codestream into a JPIP server, and requests a spatial sub-region.
 *
 * Build (assuming OpenJP3D is installed):
 * @code
 *   cc -o partial_decode partial_decode.c -lopenjpip3d -lopenjp3d
 * @endcode
 */

#include "openjp3d.h"
#include "openjpip3d.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** @brief Full volume dimensions. */
#define VOL_W 64
#define VOL_H 64
#define VOL_D 32

/** @brief Sub-region to request. */
#define SUB_W 16
#define SUB_H 16
#define SUB_D 8

int main(void)
{
    printf("OpenJP3D partial_decode example (version %s)\n",
           opj_jp3d_get_version());

    /* ---- Create and fill a test volume --------------------------------- */

    opj_volume_t *vol = opj_jp3d_create_volume(1, VOL_W, VOL_H, VOL_D, 8, 0);
    if (!vol) {
        fprintf(stderr, "Failed to create volume\n");
        return EXIT_FAILURE;
    }

    for (uint32_t z = 0; z < VOL_D; z++)
        for (uint32_t y = 0; y < VOL_H; y++)
            for (uint32_t x = 0; x < VOL_W; x++)
                vol->comps[0].data[z * VOL_W * VOL_H + y * VOL_W + x] =
                    (int32_t)((x + y + z) & 0xFF);

    /* ---- Encode -------------------------------------------------------- */

    opj_jp3d_enc_params_t enc;
    opj_jp3d_set_default_encoder_parameters(&enc);

    uint8_t *cs = NULL;
    size_t   cs_size = 0;
    if (!opj_jp3d_encode(vol, &enc, &cs, &cs_size, NULL, NULL)) {
        fprintf(stderr, "Encoding failed\n");
        opj_jp3d_destroy_volume(vol);
        return EXIT_FAILURE;
    }
    opj_jp3d_destroy_volume(vol);
    printf("Encoded %zu bytes\n", cs_size);

    /* ---- Set up JPIP server and load dataset --------------------------- */

    opj_jpip3d_server_t *srv = opj_jpip3d_server_create();
    if (!srv || !opj_jpip3d_server_load_dataset_mem(srv, "test", cs, cs_size)) {
        fprintf(stderr, "Server setup failed\n");
        opj_jp3d_free(cs);
        return EXIT_FAILURE;
    }

    /* ---- Open a client session ----------------------------------------- */

    opj_jpip3d_session_t *sess = opj_jpip3d_session_open(srv);
    if (!sess) {
        fprintf(stderr, "Session open failed\n");
        opj_jp3d_free(cs);
        opj_jpip3d_server_destroy(srv);
        return EXIT_FAILURE;
    }

    /* ---- Request a sub-volume ------------------------------------------ */

    opj_jpip3d_request_t req;
    memset(&req, 0, sizeof(req));
    snprintf(req.dataset, sizeof(req.dataset), "test");
    req.fsiz3d[0] = VOL_W;  req.fsiz3d[1] = VOL_H;  req.fsiz3d[2] = VOL_D;
    req.roff3d[0] = 0;      req.roff3d[1] = 0;       req.roff3d[2] = 0;
    req.rsiz3d[0] = SUB_W;  req.rsiz3d[1] = SUB_H;   req.rsiz3d[2] = SUB_D;

    opj_volume_t *sub = opj_jpip3d_session_receive_volume(sess, &req);
    if (!sub) {
        fprintf(stderr, "Sub-volume request failed\n");
    } else {
        printf("Received sub-volume: %u x %u x %u\n",
               sub->comps[0].w, sub->comps[0].h, sub->comps[0].d);

        /* Print a few samples from the sub-volume. */
        printf("First samples: ");
        uint32_t n = sub->comps[0].w < 8 ? sub->comps[0].w : 8;
        for (uint32_t i = 0; i < n; i++)
            printf("%d ", sub->comps[0].data[i]);
        printf("...\n");

        opj_jp3d_destroy_volume(sub);
    }

    /* ---- Cleanup ------------------------------------------------------- */

    opj_jpip3d_session_close(sess);
    opj_jp3d_free(cs);
    opj_jpip3d_server_destroy(srv);

    printf("Done.\n");
    return EXIT_SUCCESS;
}
