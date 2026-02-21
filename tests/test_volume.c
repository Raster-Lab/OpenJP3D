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
 * @file test_volume.c
 * @brief Unit tests for opj_jp3d_create_volume / opj_jp3d_destroy_volume.
 */

#include "openjp3d.h"

#include <stdio.h>
#include <stdlib.h>

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
    /* 1. create_volume returns non-NULL */
    opj_volume_t *vol = opj_jp3d_create_volume(1, 4, 4, 4, 8, 0);
    ASSERT(vol != NULL);

    /* 2. numcomps correct */
    ASSERT(vol != NULL && vol->numcomps == 1);

    /* 3. component w, h, d correct */
    ASSERT(vol != NULL && vol->comps[0].w == 4 &&
           vol->comps[0].h == 4 && vol->comps[0].d == 4);

    /* 4. data != NULL */
    ASSERT(vol != NULL && vol->comps[0].data != NULL);

    /* 5. prec correct */
    ASSERT(vol != NULL && vol->comps[0].prec == 8);

    /* 6. sgnd correct */
    ASSERT(vol != NULL && vol->comps[0].sgnd == 0);

    /* 7. destroy doesn't crash */
    opj_jp3d_destroy_volume(vol);
    vol = NULL;
    ASSERT(1); /* just reaching here means no crash */

    /* 8. create with 3 components */
    opj_volume_t *vol3 = opj_jp3d_create_volume(3, 8, 8, 8, 16, 1);
    ASSERT(vol3 != NULL && vol3->numcomps == 3);
    ASSERT(vol3 != NULL && vol3->comps[2].data != NULL);

    /* 9. data read/write */
    if (vol3) {
        vol3->comps[0].data[0] = 42;
        ASSERT(vol3->comps[0].data[0] == 42);
    } else {
        ASSERT(0);
    }
    opj_jp3d_destroy_volume(vol3);

    /* 10. create with 0 numcomps returns NULL */
    opj_volume_t *vol0 = opj_jp3d_create_volume(0, 4, 4, 4, 8, 0);
    ASSERT(vol0 == NULL);
    opj_jp3d_destroy_volume(vol0); /* should not crash */

    printf("Passed %d/%d tests\n", pass_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}
