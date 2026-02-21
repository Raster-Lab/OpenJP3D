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
 * @file openjp3d.c
 * @brief OpenJP3D core codec implementation.
 */

#include "openjp3d.h"
#include "opj_mem.h"
#include "opj_volume.h"
#include "opj_cs3d.h"
#include "opj_tcd3d.h"

#include <string.h>
#include <stdint.h>

/* -------------------------------------------------------------------------
 * Version
 * ---------------------------------------------------------------------- */

const char *opj_jp3d_get_version(void)
{
    return OPJ_JP3D_VERSION;
}

/* -------------------------------------------------------------------------
 * Default parameters
 * ---------------------------------------------------------------------- */

void opj_jp3d_set_default_encoder_parameters(opj_jp3d_enc_params_t *params)
{
    if (!params)
        return;
    memset(params, 0, sizeof(*params));
    params->tile_width        = 256;
    params->tile_height       = 256;
    params->tile_depth        = 256;
    params->num_resolutions_x = 3;
    params->num_resolutions_y = 3;
    params->num_resolutions_z = 3;
    params->cblk_width        = 64;
    params->cblk_height       = 64;
    params->cblk_depth        = 64;
    params->filter            = OPJ_JP3D_FILTER_53;
    params->num_layers        = 1;
    params->target_rate       = 0.0f; /* lossless */
    params->verbose           = 0;
}

void opj_jp3d_set_default_decoder_parameters(opj_jp3d_dec_params_t *params)
{
    if (!params)
        return;
    memset(params, 0, sizeof(*params));
    params->verbose = 0;
}

/* -------------------------------------------------------------------------
 * Encode
 * ---------------------------------------------------------------------- */

opj_jp3d_bool_t opj_jp3d_encode(
    const opj_volume_t          *volume,
    const opj_jp3d_enc_params_t *params,
    uint8_t                    **out_data,
    size_t                      *out_size,
    opj_jp3d_msg_callback_t      callback,
    void                        *callback_data)
{
    (void)callback;
    (void)callback_data;

    if (!volume || !params || !out_data || !out_size)
        return OPJ_JP3D_FALSE;
    if (volume->numcomps == 0)
        return OPJ_JP3D_FALSE;

    opj_jp3d_enc_params_t p = *params;
    /* Clamp tile sizes to volume dimensions */
    uint32_t vol_w = volume->x1 - volume->x0;
    uint32_t vol_h = volume->y1 - volume->y0;
    uint32_t vol_d = volume->z1 - volume->z0;
    if (p.tile_width  == 0 || p.tile_width  > vol_w) p.tile_width  = vol_w;
    if (p.tile_height == 0 || p.tile_height > vol_h) p.tile_height = vol_h;
    if (p.tile_depth  == 0 || p.tile_depth  > vol_d) p.tile_depth  = vol_d;

    opj_buf_t buf = {NULL, 0, 0};

    /* SOC */
    opj_cs3d_write_u16(&buf, (uint16_t)OPJ_CS3D_SOC);

    /* SIZ3D */
    opj_cs3d_write_u16(&buf, (uint16_t)OPJ_CS3D_SIZ3D);
    opj_cs3d_write_u32(&buf, volume->x1);
    opj_cs3d_write_u32(&buf, volume->y1);
    opj_cs3d_write_u32(&buf, volume->z1);
    opj_cs3d_write_u32(&buf, volume->x0);
    opj_cs3d_write_u32(&buf, volume->y0);
    opj_cs3d_write_u32(&buf, volume->z0);
    opj_cs3d_write_u32(&buf, p.tile_width);
    opj_cs3d_write_u32(&buf, p.tile_height);
    opj_cs3d_write_u32(&buf, p.tile_depth);
    opj_cs3d_write_u32(&buf, volume->numcomps);
    opj_cs3d_write_u32(&buf, volume->color_space);
    for (uint32_t c = 0; c < volume->numcomps; c++) {
        const opj_volume_comp_t *comp = &volume->comps[c];
        opj_cs3d_write_u32(&buf, comp->w);
        opj_cs3d_write_u32(&buf, comp->h);
        opj_cs3d_write_u32(&buf, comp->d);
        opj_cs3d_write_u32(&buf, comp->prec);
        opj_cs3d_write_u32(&buf, (uint32_t)comp->sgnd);
    }

    /* COD3D */
    opj_cs3d_write_u16(&buf, (uint16_t)OPJ_CS3D_COD3D);
    opj_cs3d_write_u32(&buf, (uint32_t)p.filter);
    opj_cs3d_write_u32(&buf, p.num_resolutions_x);
    opj_cs3d_write_u32(&buf, p.num_resolutions_y);
    opj_cs3d_write_u32(&buf, p.num_resolutions_z);
    opj_cs3d_write_u32(&buf, p.cblk_width);
    opj_cs3d_write_u32(&buf, p.cblk_height);
    opj_cs3d_write_u32(&buf, p.cblk_depth);
    opj_cs3d_write_u32(&buf, p.num_layers);

    /* QCD3D */
    opj_cs3d_write_u16(&buf, (uint16_t)OPJ_CS3D_QCD3D);
    opj_cs3d_write_f32(&buf, p.target_rate);

    /* Tiles */
    uint32_t ntx = (vol_w + p.tile_width  - 1) / p.tile_width;
    uint32_t nty = (vol_h + p.tile_height - 1) / p.tile_height;
    uint32_t ntz = (vol_d + p.tile_depth  - 1) / p.tile_depth;

    uint32_t tile_idx = 0;
    for (uint32_t tz = 0; tz < ntz; tz++) {
        uint32_t tz0 = tz * p.tile_depth;
        uint32_t tz1 = tz0 + p.tile_depth; if (tz1 > vol_d) tz1 = vol_d;
        uint32_t td  = tz1 - tz0;

        for (uint32_t ty = 0; ty < nty; ty++) {
            uint32_t ty0 = ty * p.tile_height;
            uint32_t ty1 = ty0 + p.tile_height; if (ty1 > vol_h) ty1 = vol_h;
            uint32_t th  = ty1 - ty0;

            for (uint32_t tx = 0; tx < ntx; tx++) {
                uint32_t tx0 = tx * p.tile_width;
                uint32_t tx1 = tx0 + p.tile_width; if (tx1 > vol_w) tx1 = vol_w;
                uint32_t tw  = tx1 - tx0;

                /* Encode each component */
                for (uint32_t c = 0; c < volume->numcomps; c++) {
                    const opj_volume_comp_t *comp = &volume->comps[c];
                    size_t tsamp = (size_t)tw * th * td;
                    int32_t *tile_data = (int32_t *)opj_jp3d_malloc(
                        tsamp * sizeof(int32_t));
                    if (!tile_data) { opj_buf_free(&buf); return OPJ_JP3D_FALSE; }

                    /* Gather tile samples */
                    for (uint32_t z = 0; z < td; z++)
                        for (uint32_t y = 0; y < th; y++)
                            for (uint32_t x = 0; x < tw; x++)
                                tile_data[(z * th + y) * tw + x] =
                                    comp->data[((tz0 + z) * comp->h + (ty0 + y))
                                               * comp->w + (tx0 + x)];

                    opj_buf_t tile_buf = {NULL, 0, 0};
                    int ok = opj_tcd3d_encode_tile(
                        tile_data, tw, th, td,
                        p.cblk_width, p.cblk_height, p.cblk_depth,
                        p.num_resolutions_x, p.num_resolutions_y,
                        p.num_resolutions_z, p.filter, &tile_buf);
                    opj_jp3d_free(tile_data);

                    if (!ok) {
                        opj_buf_free(&tile_buf);
                        opj_buf_free(&buf);
                        return OPJ_JP3D_FALSE;
                    }

                    /* SOT */
                    opj_cs3d_write_u16(&buf, (uint16_t)OPJ_CS3D_SOT);
                    opj_cs3d_write_u32(&buf, tile_idx * volume->numcomps + c);
                    opj_cs3d_write_u32(&buf, (uint32_t)tile_buf.len);

                    /* SOD */
                    opj_cs3d_write_u16(&buf, (uint16_t)OPJ_CS3D_SOD);

                    /* Tile data */
                    opj_buf_write(&buf, tile_buf.data, tile_buf.len);
                    opj_buf_free(&tile_buf);
                }
                tile_idx++;
            }
        }
    }

    /* EOC */
    opj_cs3d_write_u16(&buf, (uint16_t)OPJ_CS3D_EOC);

    *out_data = buf.data;
    *out_size = buf.len;
    return OPJ_JP3D_TRUE;
}

/* -------------------------------------------------------------------------
 * Decode
 * ---------------------------------------------------------------------- */

opj_volume_t *opj_jp3d_decode(
    const uint8_t               *data,
    size_t                       size,
    const opj_jp3d_dec_params_t *params,
    opj_jp3d_msg_callback_t      callback,
    void                        *callback_data)
{
    (void)params;
    (void)callback;
    (void)callback_data;

    if (!data || size < 4)
        return NULL;

    size_t pos = 0;
    int    err = 0;

    /* SOC */
    uint16_t marker = opj_cs3d_read_u16(data, &pos, size, &err);
    if (err || marker != (uint16_t)OPJ_CS3D_SOC)
        return NULL;

    /* SIZ3D */
    marker = opj_cs3d_read_u16(data, &pos, size, &err);
    if (err || marker != (uint16_t)OPJ_CS3D_SIZ3D)
        return NULL;

    uint32_t x1          = opj_cs3d_read_u32(data, &pos, size, &err);
    uint32_t y1          = opj_cs3d_read_u32(data, &pos, size, &err);
    uint32_t z1          = opj_cs3d_read_u32(data, &pos, size, &err);
    uint32_t x0          = opj_cs3d_read_u32(data, &pos, size, &err);
    uint32_t y0          = opj_cs3d_read_u32(data, &pos, size, &err);
    uint32_t z0          = opj_cs3d_read_u32(data, &pos, size, &err);
    uint32_t tile_width  = opj_cs3d_read_u32(data, &pos, size, &err);
    uint32_t tile_height = opj_cs3d_read_u32(data, &pos, size, &err);
    uint32_t tile_depth  = opj_cs3d_read_u32(data, &pos, size, &err);
    uint32_t numcomps    = opj_cs3d_read_u32(data, &pos, size, &err);
    uint32_t color_space = opj_cs3d_read_u32(data, &pos, size, &err);
    if (err || numcomps == 0)
        return NULL;

    uint32_t vol_w = x1 - x0;
    uint32_t vol_h = y1 - y0;
    uint32_t vol_d = z1 - z0;

    /* Per-component metadata (not used to create vol — we use first comp dims) */
    uint32_t *comp_w    = (uint32_t *)opj_jp3d_malloc(numcomps * sizeof(uint32_t));
    uint32_t *comp_h    = (uint32_t *)opj_jp3d_malloc(numcomps * sizeof(uint32_t));
    uint32_t *comp_d    = (uint32_t *)opj_jp3d_malloc(numcomps * sizeof(uint32_t));
    uint32_t *comp_prec = (uint32_t *)opj_jp3d_malloc(numcomps * sizeof(uint32_t));
    uint32_t *comp_sgnd = (uint32_t *)opj_jp3d_malloc(numcomps * sizeof(uint32_t));
    if (!comp_w || !comp_h || !comp_d || !comp_prec || !comp_sgnd) {
        opj_jp3d_free(comp_w); opj_jp3d_free(comp_h); opj_jp3d_free(comp_d);
        opj_jp3d_free(comp_prec); opj_jp3d_free(comp_sgnd);
        return NULL;
    }

    for (uint32_t c = 0; c < numcomps; c++) {
        comp_w[c]    = opj_cs3d_read_u32(data, &pos, size, &err);
        comp_h[c]    = opj_cs3d_read_u32(data, &pos, size, &err);
        comp_d[c]    = opj_cs3d_read_u32(data, &pos, size, &err);
        comp_prec[c] = opj_cs3d_read_u32(data, &pos, size, &err);
        comp_sgnd[c] = opj_cs3d_read_u32(data, &pos, size, &err);
    }
    if (err) {
        opj_jp3d_free(comp_w); opj_jp3d_free(comp_h); opj_jp3d_free(comp_d);
        opj_jp3d_free(comp_prec); opj_jp3d_free(comp_sgnd);
        return NULL;
    }

    /* COD3D */
    marker = opj_cs3d_read_u16(data, &pos, size, &err);
    if (err || marker != (uint16_t)OPJ_CS3D_COD3D) {
        opj_jp3d_free(comp_w); opj_jp3d_free(comp_h); opj_jp3d_free(comp_d);
        opj_jp3d_free(comp_prec); opj_jp3d_free(comp_sgnd);
        return NULL;
    }
    int32_t  filter = (int32_t)opj_cs3d_read_u32(data, &pos, size, &err);
    uint32_t nx     = opj_cs3d_read_u32(data, &pos, size, &err);
    uint32_t ny     = opj_cs3d_read_u32(data, &pos, size, &err);
    uint32_t nz     = opj_cs3d_read_u32(data, &pos, size, &err);
    uint32_t cblk_w = opj_cs3d_read_u32(data, &pos, size, &err);
    uint32_t cblk_h = opj_cs3d_read_u32(data, &pos, size, &err);
    uint32_t cblk_d = opj_cs3d_read_u32(data, &pos, size, &err);
    /* num_layers */ (void)opj_cs3d_read_u32(data, &pos, size, &err);

    /* QCD3D */
    marker = opj_cs3d_read_u16(data, &pos, size, &err);
    if (err || marker != (uint16_t)OPJ_CS3D_QCD3D) {
        opj_jp3d_free(comp_w); opj_jp3d_free(comp_h); opj_jp3d_free(comp_d);
        opj_jp3d_free(comp_prec); opj_jp3d_free(comp_sgnd);
        return NULL;
    }
    /* target_rate */ (void)opj_cs3d_read_f32(data, &pos, size, &err);

    if (err) {
        opj_jp3d_free(comp_w); opj_jp3d_free(comp_h); opj_jp3d_free(comp_d);
        opj_jp3d_free(comp_prec); opj_jp3d_free(comp_sgnd);
        return NULL;
    }

    /* Create output volume */
    opj_volume_t *vol = (opj_volume_t *)opj_jp3d_calloc(1, sizeof(opj_volume_t));
    if (!vol) {
        opj_jp3d_free(comp_w); opj_jp3d_free(comp_h); opj_jp3d_free(comp_d);
        opj_jp3d_free(comp_prec); opj_jp3d_free(comp_sgnd);
        return NULL;
    }
    vol->numcomps   = numcomps;
    vol->x0 = x0; vol->y0 = y0; vol->z0 = z0;
    vol->x1 = x1; vol->y1 = y1; vol->z1 = z1;
    vol->color_space = color_space;

    vol->comps = (opj_volume_comp_t *)opj_jp3d_calloc(
        numcomps, sizeof(opj_volume_comp_t));
    if (!vol->comps) {
        opj_jp3d_free(vol);
        opj_jp3d_free(comp_w); opj_jp3d_free(comp_h); opj_jp3d_free(comp_d);
        opj_jp3d_free(comp_prec); opj_jp3d_free(comp_sgnd);
        return NULL;
    }
    for (uint32_t c = 0; c < numcomps; c++) {
        vol->comps[c].w    = comp_w[c];
        vol->comps[c].h    = comp_h[c];
        vol->comps[c].d    = comp_d[c];
        vol->comps[c].prec = comp_prec[c];
        vol->comps[c].sgnd = (int32_t)comp_sgnd[c];
        vol->comps[c].dz   = 1.0f;
        size_t nsamp = (size_t)comp_w[c] * comp_h[c] * comp_d[c];
        vol->comps[c].data = (int32_t *)opj_jp3d_calloc(nsamp, sizeof(int32_t));
        if (!vol->comps[c].data) {
            opj_jp3d_destroy_volume(vol);
            opj_jp3d_free(comp_w); opj_jp3d_free(comp_h); opj_jp3d_free(comp_d);
            opj_jp3d_free(comp_prec); opj_jp3d_free(comp_sgnd);
            return NULL;
        }
    }
    opj_jp3d_free(comp_w); opj_jp3d_free(comp_h); opj_jp3d_free(comp_d);
    opj_jp3d_free(comp_prec); opj_jp3d_free(comp_sgnd);

    /* Tiles */
    uint32_t ntx = (vol_w + tile_width  - 1) / tile_width;
    uint32_t nty = (vol_h + tile_height - 1) / tile_height;
    uint32_t ntz = (vol_d + tile_depth  - 1) / tile_depth;

    uint32_t total_tile_parts = ntx * nty * ntz * numcomps;

    for (uint32_t tp = 0; tp < total_tile_parts; tp++) {
        /* SOT */
        marker = opj_cs3d_read_u16(data, &pos, size, &err);
        if (err || marker != (uint16_t)OPJ_CS3D_SOT) {
            opj_jp3d_destroy_volume(vol);
            return NULL;
        }
        uint32_t tile_part_idx = opj_cs3d_read_u32(data, &pos, size, &err);
        uint32_t tile_data_len = opj_cs3d_read_u32(data, &pos, size, &err);

        /* SOD */
        marker = opj_cs3d_read_u16(data, &pos, size, &err);
        if (err || marker != (uint16_t)OPJ_CS3D_SOD) {
            opj_jp3d_destroy_volume(vol);
            return NULL;
        }
        if (pos + tile_data_len > size) {
            opj_jp3d_destroy_volume(vol);
            return NULL;
        }

        /* Decode tile */
        /* tile_part_idx = tile_idx * numcomps + comp */
        uint32_t comp        = tile_part_idx % numcomps;
        uint32_t tile_idx    = tile_part_idx / numcomps;
        uint32_t ti_total    = ntx * nty;
        uint32_t tz_idx      = tile_idx / ti_total;
        uint32_t tz_rem      = tile_idx % ti_total;
        uint32_t ty_idx      = tz_rem / ntx;
        uint32_t tx_idx      = tz_rem % ntx;

        uint32_t tx0 = tx_idx * tile_width;
        uint32_t tx1 = tx0 + tile_width;  if (tx1 > vol_w) tx1 = vol_w;
        uint32_t tw  = tx1 - tx0;
        uint32_t ty0 = ty_idx * tile_height;
        uint32_t ty1 = ty0 + tile_height; if (ty1 > vol_h) ty1 = vol_h;
        uint32_t th  = ty1 - ty0;
        uint32_t tz0 = tz_idx * tile_depth;
        uint32_t tz1 = tz0 + tile_depth;  if (tz1 > vol_d) tz1 = vol_d;
        uint32_t td  = tz1 - tz0;

        size_t tsamp = (size_t)tw * th * td;
        int32_t *tile_out = (int32_t *)opj_jp3d_malloc(tsamp * sizeof(int32_t));
        if (!tile_out) {
            opj_jp3d_destroy_volume(vol);
            return NULL;
        }

        if (!opj_tcd3d_decode_tile(
                data + pos, tile_data_len, tile_out,
                tw, th, td,
                cblk_w, cblk_h, cblk_d,
                nx, ny, nz, filter)) {
            opj_jp3d_free(tile_out);
            opj_jp3d_destroy_volume(vol);
            return NULL;
        }
        pos += tile_data_len;

        /* Scatter tile samples back to volume component */
        opj_volume_comp_t *vcomp = &vol->comps[comp];
        for (uint32_t z = 0; z < td; z++)
            for (uint32_t y = 0; y < th; y++)
                for (uint32_t x = 0; x < tw; x++)
                    vcomp->data[((tz0 + z) * vcomp->h + (ty0 + y))
                                * vcomp->w + (tx0 + x)] =
                        tile_out[(z * th + y) * tw + x];

        opj_jp3d_free(tile_out);
    }

    /* EOC */
    marker = opj_cs3d_read_u16(data, &pos, size, &err);
    if (err || marker != (uint16_t)OPJ_CS3D_EOC) {
        opj_jp3d_destroy_volume(vol);
        return NULL;
    }

    return vol;
}
