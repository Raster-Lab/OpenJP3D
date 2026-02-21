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
 * @file test_params.c
 * @brief Unit tests for parameter initialization and version functions.
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

int main(void)
{
    opj_jp3d_enc_params_t enc;
    opj_jp3d_set_default_encoder_parameters(&enc);

    /* 1. Default tile_width = 256 */
    ASSERT(enc.tile_width == 256);

    /* 2. Default filter = OPJ_JP3D_FILTER_53 */
    ASSERT(enc.filter == OPJ_JP3D_FILTER_53);

    /* 3. Default num_resolutions_x = 3 */
    ASSERT(enc.num_resolutions_x == 3);

    /* 4. Default target_rate = 0.0 (lossless) */
    ASSERT(enc.target_rate == 0.0f);

    /* 5. Default dec params: no crash */
    opj_jp3d_dec_params_t dec;
    opj_jp3d_set_default_decoder_parameters(&dec);
    ASSERT(dec.verbose == 0);

    /* 6. get_version returns non-NULL */
    ASSERT(opj_jp3d_get_version() != NULL);

    /* 7. get_version returns "1.0.0" */
    ASSERT(strcmp(opj_jp3d_get_version(), "1.0.0") == 0);

    /* 8. Default num_layers = 1 */
    ASSERT(enc.num_layers == 1);

    printf("Passed %d/%d tests\n", pass_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}
