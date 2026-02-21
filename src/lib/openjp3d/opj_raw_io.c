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
 * @file opj_raw_io.c
 * @brief Raw binary volume I/O.
 */

#include "opj_raw_io.h"
#include <stdio.h>
#include <stdint.h>

int opj_raw_io_write(const opj_volume_t *vol, const char *path)
{
    if (!vol || !path)
        return 0;

    FILE *fp = fopen(path, "wb");
    if (!fp)
        return 0;

    for (uint32_t c = 0; c < vol->numcomps; c++) {
        const opj_volume_comp_t *comp = &vol->comps[c];
        size_t nsamp = (size_t)comp->w * comp->h * comp->d;

        for (size_t i = 0; i < nsamp; i++) {
            int32_t val = comp->data[i];
            if (comp->prec <= 8) {
                uint8_t b = comp->sgnd ? (int8_t)val : (uint8_t)val;
                if (fwrite(&b, 1, 1, fp) != 1) { fclose(fp); return 0; }
            } else if (comp->prec <= 16) {
                uint16_t v = comp->sgnd ? (int16_t)val : (uint16_t)val;
                uint8_t le[2] = { (uint8_t)(v), (uint8_t)(v >> 8) };
                if (fwrite(le, 1, 2, fp) != 2) { fclose(fp); return 0; }
            } else {
                uint32_t v = (uint32_t)val;
                uint8_t le[4] = {
                    (uint8_t)(v      ), (uint8_t)(v >>  8),
                    (uint8_t)(v >> 16), (uint8_t)(v >> 24)
                };
                if (fwrite(le, 1, 4, fp) != 4) { fclose(fp); return 0; }
            }
        }
    }

    fclose(fp);
    return 1;
}

int opj_raw_io_read(opj_volume_t *vol, const char *path)
{
    if (!vol || !path)
        return 0;

    FILE *fp = fopen(path, "rb");
    if (!fp)
        return 0;

    for (uint32_t c = 0; c < vol->numcomps; c++) {
        opj_volume_comp_t *comp = &vol->comps[c];
        size_t nsamp = (size_t)comp->w * comp->h * comp->d;

        for (size_t i = 0; i < nsamp; i++) {
            int32_t val = 0;
            if (comp->prec <= 8) {
                uint8_t b;
                if (fread(&b, 1, 1, fp) != 1) { fclose(fp); return 0; }
                val = comp->sgnd ? (int8_t)b : (int32_t)b;
            } else if (comp->prec <= 16) {
                uint8_t le[2];
                if (fread(le, 1, 2, fp) != 2) { fclose(fp); return 0; }
                uint16_t v = (uint16_t)le[0] | ((uint16_t)le[1] << 8);
                val = comp->sgnd ? (int16_t)v : (int32_t)v;
            } else {
                uint8_t le[4];
                if (fread(le, 1, 4, fp) != 4) { fclose(fp); return 0; }
                uint32_t v = (uint32_t)le[0]        | ((uint32_t)le[1] << 8) |
                             ((uint32_t)le[2] << 16) | ((uint32_t)le[3] << 24);
                val = (int32_t)v;
            }
            comp->data[i] = val;
        }
    }

    fclose(fp);
    return 1;
}
