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
 * @file encode_volume.c
 * @brief Example: Encode a synthetic 3-D volume to a JP3D codestream.
 *
 * This self-contained program creates a small test volume, encodes it to a
 * JP3D codestream using the lossless 5/3 filter, and writes the codestream
 * to a file.
 *
 * Build (assuming OpenJP3D is installed):
 * @code
 *   cc -o encode_volume encode_volume.c -lopenjp3d
 * @endcode
 *
 * Or from the build tree:
 * @code
 *   cc -I../../src/lib/openjp3d -o encode_volume encode_volume.c \
 *      -L../../build/src/lib/openjp3d -lopenjp3d
 * @endcode
 */

#include "openjp3d.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** @brief Volume dimensions for this example. */
#define VOL_W 32
#define VOL_H 32
#define VOL_D 16

/**
 * @brief Error / warning / info callback.
 */
static void msg_callback(opj_jp3d_msg_level_t level,
                          const char *message, void *data)
{
    (void)data;
    const char *prefix = "INFO";
    if (level == OPJ_JP3D_MSG_WARNING)
        prefix = "WARN";
    if (level == OPJ_JP3D_MSG_ERROR)
        prefix = "ERROR";
    fprintf(stderr, "[%s] %s\n", prefix, message);
}

int main(int argc, char *argv[])
{
    const char *outpath = "output.jp3d";

    if (argc > 1)
        outpath = argv[1];

    printf("OpenJP3D encode_volume example (version %s)\n",
           opj_jp3d_get_version());

    /* ---- Create volume ------------------------------------------------- */

    opj_volume_t *vol = opj_jp3d_create_volume(
        1,                          /* 1 component (greyscale) */
        VOL_W, VOL_H, VOL_D,       /* dimensions */
        8,                          /* 8-bit samples */
        0                           /* unsigned */
    );
    if (!vol) {
        fprintf(stderr, "Failed to create volume\n");
        return EXIT_FAILURE;
    }

    /* Fill with a simple test pattern: XOR of coordinates. */
    for (uint32_t z = 0; z < VOL_D; z++) {
        for (uint32_t y = 0; y < VOL_H; y++) {
            for (uint32_t x = 0; x < VOL_W; x++) {
                vol->comps[0].data[z * VOL_W * VOL_H + y * VOL_W + x] =
                    (int32_t)((x ^ y ^ z) & 0xFF);
            }
        }
    }

    /* ---- Set encoder parameters ---------------------------------------- */

    opj_jp3d_enc_params_t params;
    opj_jp3d_set_default_encoder_parameters(&params);
    params.filter      = OPJ_JP3D_FILTER_53;   /* Lossless 5/3 lifting */
    params.target_rate = 0.0f;                  /* 0 = lossless */
    params.verbose     = 1;

    /* ---- Encode -------------------------------------------------------- */

    uint8_t *codestream = NULL;
    size_t   cs_size    = 0;

    opj_jp3d_bool_t ok = opj_jp3d_encode(
        vol, &params,
        &codestream, &cs_size,
        msg_callback, NULL
    );
    opj_jp3d_destroy_volume(vol);

    if (!ok) {
        fprintf(stderr, "Encoding failed\n");
        return EXIT_FAILURE;
    }

    printf("Encoded %zu bytes\n", cs_size);

    /* ---- Write to file ------------------------------------------------- */

    FILE *fp = fopen(outpath, "wb");
    if (!fp) {
        perror(outpath);
        opj_jp3d_free(codestream);
        return EXIT_FAILURE;
    }
    fwrite(codestream, 1, cs_size, fp);
    fclose(fp);
    opj_jp3d_free(codestream);

    printf("Codestream written to %s\n", outpath);
    return EXIT_SUCCESS;
}
