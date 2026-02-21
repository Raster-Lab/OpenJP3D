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
 * @file test_raw_io.c
 * @brief Unit tests for opj_raw_io_write / opj_raw_io_read.
 */

#include "openjp3d.h"
#include "opj_raw_io.h"

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

static const char *TMP1 = "/tmp/opj_raw_io_test1.raw";
static const char *TMP2 = "/tmp/opj_raw_io_test2.raw";
static const char *TMP3 = "/tmp/opj_raw_io_test3.raw";
static const char *TMP4 = "/tmp/opj_raw_io_test4.raw";

static int volumes_equal(const opj_volume_t *a, const opj_volume_t *b)
{
    if (a->numcomps != b->numcomps) return 0;
    for (uint32_t c = 0; c < a->numcomps; c++) {
        const opj_volume_comp_t *ca = &a->comps[c];
        const opj_volume_comp_t *cb = &b->comps[c];
        size_t n = (size_t)ca->w * ca->h * ca->d;
        for (size_t i = 0; i < n; i++)
            if (ca->data[i] != cb->data[i]) return 0;
    }
    return 1;
}

int main(void)
{
    /* 1. Write then read 8-bit */
    {
        opj_volume_t *orig = opj_jp3d_create_volume(1, 4, 4, 4, 8, 0);
        opj_volume_t *back = opj_jp3d_create_volume(1, 4, 4, 4, 8, 0);
        ASSERT(orig && back);
        if (orig && back) {
            size_t n = 4u*4u*4u;
            for (size_t i = 0; i < n; i++) orig->comps[0].data[i] = (int32_t)(i & 0xFF);
            ASSERT(opj_raw_io_write(orig, TMP1) == 1);
            ASSERT(opj_raw_io_read(back, TMP1)  == 1);
            ASSERT(volumes_equal(orig, back));
        }
        opj_jp3d_destroy_volume(orig);
        opj_jp3d_destroy_volume(back);
        remove(TMP1);
    }

    /* 2. Write then read 16-bit */
    {
        opj_volume_t *orig = opj_jp3d_create_volume(1, 4, 4, 4, 16, 0);
        opj_volume_t *back = opj_jp3d_create_volume(1, 4, 4, 4, 16, 0);
        ASSERT(orig && back);
        if (orig && back) {
            size_t n = 4u*4u*4u;
            for (size_t i = 0; i < n; i++) orig->comps[0].data[i] = (int32_t)(i * 257);
            ASSERT(opj_raw_io_write(orig, TMP2) == 1);
            ASSERT(opj_raw_io_read(back, TMP2)  == 1);
            ASSERT(volumes_equal(orig, back));
        }
        opj_jp3d_destroy_volume(orig);
        opj_jp3d_destroy_volume(back);
        remove(TMP2);
    }

    /* 3. Write to non-existent directory returns error */
    {
        opj_volume_t *vol = opj_jp3d_create_volume(1, 2, 2, 2, 8, 0);
        ASSERT(vol != NULL);
        ASSERT(opj_raw_io_write(vol, "/nonexistent_dir/foo.raw") == 0);
        opj_jp3d_destroy_volume(vol);
    }

    /* 4. Read non-existent file returns error */
    {
        opj_volume_t *vol = opj_jp3d_create_volume(1, 2, 2, 2, 8, 0);
        ASSERT(vol != NULL);
        ASSERT(opj_raw_io_read(vol, "/tmp/no_such_file_opj_test.raw") == 0);
        opj_jp3d_destroy_volume(vol);
    }

    /* 5. Read back has correct dimensions */
    {
        opj_volume_t *orig = opj_jp3d_create_volume(1, 3, 5, 7, 8, 0);
        ASSERT(orig != NULL);
        if (orig) {
            opj_raw_io_write(orig, TMP3);
            opj_volume_t *back = opj_jp3d_create_volume(1, 3, 5, 7, 8, 0);
            ASSERT(back != NULL);
            if (back) {
                opj_raw_io_read(back, TMP3);
                ASSERT(back->comps[0].w == 3 &&
                       back->comps[0].h == 5 &&
                       back->comps[0].d == 7);
                opj_jp3d_destroy_volume(back);
            }
            opj_jp3d_destroy_volume(orig);
        }
        remove(TMP3);
    }

    /* 6. Signed 8-bit round-trip */
    {
        opj_volume_t *orig = opj_jp3d_create_volume(1, 4, 4, 4, 8, 1);
        opj_volume_t *back = opj_jp3d_create_volume(1, 4, 4, 4, 8, 1);
        ASSERT(orig && back);
        if (orig && back) {
            size_t n = 4u*4u*4u;
            for (size_t i = 0; i < n; i++)
                orig->comps[0].data[i] = (int32_t)((int8_t)(i & 0xFF));
            ASSERT(opj_raw_io_write(orig, TMP4) == 1);
            ASSERT(opj_raw_io_read(back, TMP4)  == 1);
            ASSERT(volumes_equal(orig, back));
        }
        opj_jp3d_destroy_volume(orig);
        opj_jp3d_destroy_volume(back);
        remove(TMP4);
    }

    /* 7. Multi-component round-trip */
    {
        opj_volume_t *orig = opj_jp3d_create_volume(3, 4, 4, 4, 8, 0);
        opj_volume_t *back = opj_jp3d_create_volume(3, 4, 4, 4, 8, 0);
        ASSERT(orig && back);
        if (orig && back) {
            size_t n = 4u*4u*4u;
            for (uint32_t c = 0; c < 3; c++)
                for (size_t i = 0; i < n; i++)
                    orig->comps[c].data[i] = (int32_t)((i + c * 13) & 0xFF);
            ASSERT(opj_raw_io_write(orig, TMP1) == 1);
            ASSERT(opj_raw_io_read(back, TMP1)  == 1);
            ASSERT(volumes_equal(orig, back));
        }
        opj_jp3d_destroy_volume(orig);
        opj_jp3d_destroy_volume(back);
        remove(TMP1);
    }

    printf("Passed %d/%d tests\n", pass_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}
