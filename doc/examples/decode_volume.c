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
 * @file decode_volume.c
 * @brief Example: Decode a JP3D codestream back to sample data.
 *
 * This program reads a JP3D codestream from a file, decodes it, and prints
 * a summary of the decoded volume (dimensions, bit depth, first few samples).
 *
 * Build (assuming OpenJP3D is installed):
 * @code
 *   cc -o decode_volume decode_volume.c -lopenjp3d
 * @endcode
 */

#include "openjp3d.h"

#include <stdio.h>
#include <stdlib.h>

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
    if (argc < 2) {
        fprintf(stderr, "Usage: decode_volume <input.jp3d>\n");
        return EXIT_FAILURE;
    }

    const char *inpath = argv[1];
    printf("OpenJP3D decode_volume example (version %s)\n",
           opj_jp3d_get_version());

    /* ---- Read codestream from file ------------------------------------- */

    FILE *fp = fopen(inpath, "rb");
    if (!fp) {
        perror(inpath);
        return EXIT_FAILURE;
    }
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fsize <= 0) {
        fprintf(stderr, "Empty or unreadable file\n");
        fclose(fp);
        return EXIT_FAILURE;
    }

    uint8_t *codestream = (uint8_t *)opj_jp3d_malloc((size_t)fsize);
    if (!codestream) {
        fprintf(stderr, "Allocation failed\n");
        fclose(fp);
        return EXIT_FAILURE;
    }
    fread(codestream, 1, (size_t)fsize, fp);
    fclose(fp);

    /* ---- Decode -------------------------------------------------------- */

    opj_jp3d_dec_params_t params;
    opj_jp3d_set_default_decoder_parameters(&params);
    params.verbose = 1;

    opj_volume_t *vol = opj_jp3d_decode(
        codestream, (size_t)fsize,
        &params,
        msg_callback, NULL
    );
    opj_jp3d_free(codestream);

    if (!vol) {
        fprintf(stderr, "Decoding failed\n");
        return EXIT_FAILURE;
    }

    /* ---- Print volume info --------------------------------------------- */

    printf("Decoded volume:\n");
    printf("  Components: %u\n", vol->numcomps);
    for (uint32_t c = 0; c < vol->numcomps; c++) {
        opj_volume_comp_t *comp = &vol->comps[c];
        printf("  Component %u: %u x %u x %u, %u-bit %s\n",
               c, comp->w, comp->h, comp->d, comp->prec,
               comp->sgnd ? "signed" : "unsigned");

        /* Print first 8 samples of the first slice. */
        printf("    First samples: ");
        uint32_t n = comp->w < 8 ? comp->w : 8;
        for (uint32_t i = 0; i < n; i++)
            printf("%d ", comp->data[i]);
        printf("...\n");
    }

    opj_jp3d_destroy_volume(vol);
    printf("Done.\n");
    return EXIT_SUCCESS;
}
