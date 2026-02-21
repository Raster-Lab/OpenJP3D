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
 * @file opj_volume.c
 * @brief Volume data structure lifecycle.
 */

#include "opj_volume.h"
#include "opj_mem.h"
#include <stdlib.h>

opj_volume_t *opj_jp3d_create_volume(uint32_t numcomps,
                                      uint32_t w, uint32_t h, uint32_t d,
                                      uint32_t prec, int32_t sgnd)
{
    if (numcomps == 0)
        return NULL;

    opj_volume_t *vol = (opj_volume_t *)opj_jp3d_calloc(1, sizeof(opj_volume_t));
    if (!vol)
        return NULL;

    vol->numcomps = numcomps;
    vol->x0 = 0; vol->y0 = 0; vol->z0 = 0;
    vol->x1 = w; vol->y1 = h; vol->z1 = d;
    vol->color_space = 0;

    vol->comps = (opj_volume_comp_t *)opj_jp3d_calloc(numcomps,
                                                       sizeof(opj_volume_comp_t));
    if (!vol->comps) {
        opj_jp3d_free(vol);
        return NULL;
    }

    for (uint32_t c = 0; c < numcomps; c++) {
        opj_volume_comp_t *comp = &vol->comps[c];
        comp->w    = w;
        comp->h    = h;
        comp->d    = d;
        comp->prec = prec;
        comp->sgnd = sgnd;
        comp->dz   = 1.0f;

        size_t nsamp = (size_t)w * h * d;
        comp->data = (int32_t *)opj_jp3d_calloc(nsamp, sizeof(int32_t));
        if (!comp->data) {
            /* Free already allocated components */
            for (uint32_t j = 0; j < c; j++)
                opj_jp3d_free(vol->comps[j].data);
            opj_jp3d_free(vol->comps);
            opj_jp3d_free(vol);
            return NULL;
        }
    }

    return vol;
}

void opj_jp3d_destroy_volume(opj_volume_t *vol)
{
    if (!vol)
        return;
    if (vol->comps) {
        for (uint32_t c = 0; c < vol->numcomps; c++)
            opj_jp3d_free(vol->comps[c].data);
        opj_jp3d_free(vol->comps);
    }
    opj_jp3d_free(vol);
}
