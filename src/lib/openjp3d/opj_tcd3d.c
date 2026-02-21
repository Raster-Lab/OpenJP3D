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
 * @file opj_tcd3d.c
 * @brief Tile compression/decompression driver.
 */

#include "opj_tcd3d.h"
#include "opj_dwt3d.h"
#include "opj_t1_3d.h"
#include "opj_t2_3d.h"
#include "opj_mem.h"

#include <string.h>
#include <stdlib.h>

/* Upper bound on encoded code-block size:
 * header(13) + planes * ceil(nsamp/8) + sign_ceil(nsamp/8) */
static size_t cblk_encode_bound(uint32_t w, uint32_t h, uint32_t d)
{
    size_t nsamp = (size_t)w * h * d;
    /* 32 magnitude planes + 1 sign plane, each padded to bytes */
    return 13 + 33 * ((nsamp + 7) / 8 + 1);
}

int opj_tcd3d_encode_tile(
    const int32_t *tile_data,
    uint32_t tw, uint32_t th, uint32_t td,
    uint32_t cblk_w, uint32_t cblk_h, uint32_t cblk_d,
    uint32_t nx, uint32_t ny, uint32_t nz,
    int32_t filter,
    opj_buf_t *out)
{
    size_t nsamp = (size_t)tw * th * td;

    /* Work on a copy for DWT */
    int32_t *work = (int32_t *)opj_jp3d_malloc(nsamp * sizeof(int32_t));
    if (!work)
        return 0;
    memcpy(work, tile_data, nsamp * sizeof(int32_t));

    /* Forward DWT */
    if (!opj_dwt3d_fwd(work, tw, th, td, nx, ny, nz, filter)) {
        opj_jp3d_free(work);
        return 0;
    }

    /* Split into code-blocks and encode */
    /* Clamp code-block dims */
    if (cblk_w == 0) cblk_w = tw;
    if (cblk_h == 0) cblk_h = th;
    if (cblk_d == 0) cblk_d = td;

    uint32_t nbx = (tw + cblk_w - 1) / cblk_w;
    uint32_t nby = (th + cblk_h - 1) / cblk_h;
    uint32_t nbz = (td + cblk_d - 1) / cblk_d;
    uint32_t num_cblks = nbx * nby * nbz;

    /* Allocate per-block buffers */
    uint8_t **cblk_bufs = (uint8_t **)opj_jp3d_calloc(num_cblks, sizeof(uint8_t *));
    size_t   *cblk_lens = (size_t   *)opj_jp3d_calloc(num_cblks, sizeof(size_t));
    if (!cblk_bufs || !cblk_lens) {
        opj_jp3d_free(work);
        opj_jp3d_free(cblk_bufs);
        opj_jp3d_free(cblk_lens);
        return 0;
    }

    uint32_t idx = 0;
    for (uint32_t bz = 0; bz < nbz; bz++) {
        uint32_t z0 = bz * cblk_d;
        uint32_t z1 = z0 + cblk_d; if (z1 > td) z1 = td;
        uint32_t bd = z1 - z0;

        for (uint32_t by = 0; by < nby; by++) {
            uint32_t y0 = by * cblk_h;
            uint32_t y1 = y0 + cblk_h; if (y1 > th) y1 = th;
            uint32_t bh = y1 - y0;

            for (uint32_t bx = 0; bx < nbx; bx++) {
                uint32_t x0 = bx * cblk_w;
                uint32_t x1 = x0 + cblk_w; if (x1 > tw) x1 = tw;
                uint32_t bw = x1 - x0;

                size_t   bn = (size_t)bw * bh * bd;
                int32_t *cbuf = (int32_t *)opj_jp3d_malloc(bn * sizeof(int32_t));
                if (!cbuf) goto fail;

                /* Gather block samples */
                for (uint32_t z = 0; z < bd; z++)
                    for (uint32_t y = 0; y < bh; y++)
                        for (uint32_t x = 0; x < bw; x++)
                            cbuf[(z * bh + y) * bw + x] =
                                work[((z0 + z) * th + (y0 + y)) * tw + (x0 + x)];

                size_t ecap = cblk_encode_bound(bw, bh, bd);
                uint8_t *ebuf = (uint8_t *)opj_jp3d_malloc(ecap);
                if (!ebuf) { opj_jp3d_free(cbuf); goto fail; }

                size_t elen = opj_t1_3d_encode_cblk(cbuf, bw, bh, bd, ebuf, ecap);
                opj_jp3d_free(cbuf);
                if (elen == 0) { opj_jp3d_free(ebuf); goto fail; }

                cblk_bufs[idx] = ebuf;
                cblk_lens[idx] = elen;
                idx++;
            }
        }
    }

    /* Write tier-2 packet */
    int ok = opj_t2_3d_write_packet(out,
                                     (const uint8_t * const *)cblk_bufs,
                                     cblk_lens, num_cblks);

    for (uint32_t i = 0; i < num_cblks; i++)
        opj_jp3d_free(cblk_bufs[i]);
    opj_jp3d_free(cblk_bufs);
    opj_jp3d_free(cblk_lens);
    opj_jp3d_free(work);
    return ok;

fail:
    for (uint32_t i = 0; i < idx; i++)
        opj_jp3d_free(cblk_bufs[i]);
    opj_jp3d_free(cblk_bufs);
    opj_jp3d_free(cblk_lens);
    opj_jp3d_free(work);
    return 0;
}

int opj_tcd3d_decode_tile(
    const uint8_t *tile_data, size_t tile_size,
    int32_t *out_data,
    uint32_t tw, uint32_t th, uint32_t td,
    uint32_t cblk_w, uint32_t cblk_h, uint32_t cblk_d,
    uint32_t nx, uint32_t ny, uint32_t nz,
    int32_t filter)
{
    size_t nsamp = (size_t)tw * th * td;

    if (cblk_w == 0) cblk_w = tw;
    if (cblk_h == 0) cblk_h = th;
    if (cblk_d == 0) cblk_d = td;

    uint32_t nbx = (tw + cblk_w - 1) / cblk_w;
    uint32_t nby = (th + cblk_h - 1) / cblk_h;
    uint32_t nbz = (td + cblk_d - 1) / cblk_d;
    uint32_t num_cblks = nbx * nby * nbz;

    /* Parse tier-2 packet */
    uint8_t **cblk_bufs = NULL;
    size_t   *cblk_lens = NULL;
    uint32_t  got_cblks = 0;

    if (!opj_t2_3d_read_packet(tile_data, tile_size,
                                &cblk_bufs, &cblk_lens, &got_cblks))
        return 0;

    if (got_cblks != num_cblks) goto fail;

    /* Allocate coefficient buffer */
    int32_t *work = (int32_t *)opj_jp3d_calloc(nsamp, sizeof(int32_t));
    if (!work) goto fail;

    uint32_t idx = 0;
    for (uint32_t bz = 0; bz < nbz; bz++) {
        uint32_t z0 = bz * cblk_d;
        uint32_t z1 = z0 + cblk_d; if (z1 > td) z1 = td;
        uint32_t bd = z1 - z0;

        for (uint32_t by = 0; by < nby; by++) {
            uint32_t y0 = by * cblk_h;
            uint32_t y1 = y0 + cblk_h; if (y1 > th) y1 = th;
            uint32_t bh = y1 - y0;

            for (uint32_t bx = 0; bx < nbx; bx++) {
                uint32_t x0 = bx * cblk_w;
                uint32_t x1 = x0 + cblk_w; if (x1 > tw) x1 = tw;
                uint32_t bw = x1 - x0;

                size_t   bn = (size_t)bw * bh * bd;
                int32_t *cbuf = (int32_t *)opj_jp3d_malloc(bn * sizeof(int32_t));
                if (!cbuf) { opj_jp3d_free(work); goto fail; }

                if (!opj_t1_3d_decode_cblk(cblk_bufs[idx], cblk_lens[idx],
                                             cbuf, bw, bh, bd)) {
                    opj_jp3d_free(cbuf);
                    opj_jp3d_free(work);
                    goto fail;
                }

                /* Scatter block samples */
                for (uint32_t z = 0; z < bd; z++)
                    for (uint32_t y = 0; y < bh; y++)
                        for (uint32_t x = 0; x < bw; x++)
                            work[((z0 + z) * th + (y0 + y)) * tw + (x0 + x)] =
                                cbuf[(z * bh + y) * bw + x];

                opj_jp3d_free(cbuf);
                idx++;
            }
        }
    }

    /* Inverse DWT */
    if (!opj_dwt3d_inv(work, tw, th, td, nx, ny, nz, filter)) {
        opj_jp3d_free(work);
        goto fail;
    }

    memcpy(out_data, work, nsamp * sizeof(int32_t));
    opj_jp3d_free(work);

    for (uint32_t i = 0; i < got_cblks; i++)
        opj_jp3d_free(cblk_bufs[i]);
    opj_jp3d_free(cblk_bufs);
    opj_jp3d_free(cblk_lens);
    return 1;

fail:
    for (uint32_t i = 0; i < got_cblks; i++)
        opj_jp3d_free(cblk_bufs[i]);
    opj_jp3d_free(cblk_bufs);
    opj_jp3d_free(cblk_lens);
    return 0;
}
