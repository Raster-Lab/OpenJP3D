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
 * @file opj_dwt3d.c
 * @brief 3-D separable DWT implementation (5/3 lossless, 9/7 lossy).
 */

#include "opj_dwt3d.h"
#include "opj_mem.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* =========================================================================
 * 1-D 5/3 lifting (integer, lossless)
 * ========================================================================= */

/** Forward 5/3 1-D transform on @p n samples in @p x (in-place via scratch). */
static int dwt53_fwd_1d(int32_t *x, uint32_t n, int32_t *scratch)
{
    if (n <= 1)
        return 1;

    uint32_t sn = (n + 1) / 2; /* low-pass count  */
    uint32_t dn = n / 2;       /* high-pass count */

    int32_t *low  = scratch;
    int32_t *high = scratch + sn;

    /* Predict step */
    for (uint32_t i = 0; i < dn; i++) {
        int32_t right;
        if (2 * (i + 1) < n)
            right = x[2 * (i + 1)];
        else
            right = x[2 * (i + 1) - 2]; /* symmetric extension */
        high[i] = x[2 * i + 1] - ((x[2 * i] + right) >> 1);
    }

    /* Update step */
    for (uint32_t i = 0; i < sn; i++) {
        int32_t left  = (i > 0)  ? high[i - 1] : high[0];
        int32_t right = (i < dn) ? high[i]      : high[dn > 0 ? dn - 1 : 0];
        low[i] = x[2 * i] + ((left + right + 2) >> 2);
    }

    /* Write result: [low | high] */
    memcpy(x,        low,  sn * sizeof(int32_t));
    memcpy(x + sn,   high, dn * sizeof(int32_t));

    return 1;
}

/** Inverse 5/3 1-D transform on @p n samples in @p x (in-place via scratch). */
static int dwt53_inv_1d(int32_t *x, uint32_t n, int32_t *scratch)
{
    if (n <= 1)
        return 1;

    uint32_t sn = (n + 1) / 2;
    uint32_t dn = n / 2;

    int32_t *low  = scratch;
    int32_t *high = scratch + sn;

    memcpy(low,  x,        sn * sizeof(int32_t));
    memcpy(high, x + sn,   dn * sizeof(int32_t));

    /* Inverse update */
    for (uint32_t i = 0; i < sn; i++) {
        int32_t left  = (i > 0)  ? high[i - 1] : high[0];
        int32_t right = (i < dn) ? high[i]      : high[dn > 0 ? dn - 1 : 0];
        low[i] -= ((left + right + 2) >> 2);
    }

    /* Inverse predict */
    for (uint32_t i = 0; i < dn; i++) {
        int32_t right = (i + 1 < sn) ? low[i + 1] : low[sn > 0 ? sn - 1 : 0];
        x[2 * i]     = low[i];
        x[2 * i + 1] = high[i] + ((low[i] + right) >> 1);
    }

    /* If n is odd, last sample is low[sn-1] */
    if (n & 1)
        x[n - 1] = low[sn - 1];

    return 1;
}

/* =========================================================================
 * 1-D 9/7 lifting (floating-point, lossy)
 * ========================================================================= */

#define DWT97_ALPHA  (-1.586134342059924)
#define DWT97_BETA   (-0.052980118572961)
#define DWT97_GAMMA   (0.882911075530934)
#define DWT97_DELTA   (0.443506852043971)
#define DWT97_K       (1.230174104914001)
#define DWT97_K_INV   (1.0 / DWT97_K)

static int dwt97_fwd_1d(int32_t *x, uint32_t n, double *fbuf)
{
    if (n <= 1)
        return 1;

    uint32_t sn = (n + 1) / 2;
    uint32_t dn = n / 2;

    /* Lift into floating-point buffer (interleaved: even=low, odd=high) */
    double *f = fbuf;
    for (uint32_t i = 0; i < n; i++)
        f[i] = (double)x[i];

    /* Step 1: predict (alpha) — updates odd samples */
    for (uint32_t i = 0; i < dn; i++) {
        double right = (2 * (i + 1) < n) ? f[2 * (i + 1)] : f[n - 2];
        f[2 * i + 1] += DWT97_ALPHA * (f[2 * i] + right);
    }

    /* Step 2: update (beta) — updates even samples */
    for (uint32_t i = 0; i < sn; i++) {
        double left  = (i > 0)        ? f[2 * i - 1] : f[1 < n ? 1 : 0];
        double right = (2 * i + 1 < n) ? f[2 * i + 1] : f[n - 1];
        /* For n=1, skip */
        if (dn == 0) { left = 0.0; right = 0.0; }
        f[2 * i] += DWT97_BETA * (left + right);
    }

    /* Step 3: predict (gamma) */
    for (uint32_t i = 0; i < dn; i++) {
        double right = (2 * (i + 1) < n) ? f[2 * (i + 1)] : f[n - 2];
        f[2 * i + 1] += DWT97_GAMMA * (f[2 * i] + right);
    }

    /* Step 4: update (delta) */
    for (uint32_t i = 0; i < sn; i++) {
        double left  = (i > 0)        ? f[2 * i - 1] : f[1 < n ? 1 : 0];
        double right = (2 * i + 1 < n) ? f[2 * i + 1] : f[n - 1];
        if (dn == 0) { left = 0.0; right = 0.0; }
        f[2 * i] += DWT97_DELTA * (left + right);
    }

    /* Scale */
    for (uint32_t i = 0; i < sn; i++)
        f[2 * i] *= DWT97_K_INV;
    for (uint32_t i = 0; i < dn; i++)
        f[2 * i + 1] *= DWT97_K;

    /* De-interleave: low then high */
    for (uint32_t i = 0; i < sn; i++)
        x[i]      = (int32_t)floor(f[2 * i] + 0.5);
    for (uint32_t i = 0; i < dn; i++)
        x[sn + i] = (int32_t)floor(f[2 * i + 1] + 0.5);

    return 1;
}

static int dwt97_inv_1d(int32_t *x, uint32_t n, double *fbuf)
{
    if (n <= 1)
        return 1;

    uint32_t sn = (n + 1) / 2;
    uint32_t dn = n / 2;

    double *f = fbuf;
    /* Re-interleave */
    for (uint32_t i = 0; i < sn; i++)
        f[2 * i]     = (double)x[i]      * DWT97_K;
    for (uint32_t i = 0; i < dn; i++)
        f[2 * i + 1] = (double)x[sn + i] * DWT97_K_INV;

    /* Inverse step 4: undo delta */
    for (uint32_t i = 0; i < sn; i++) {
        double left  = (i > 0)        ? f[2 * i - 1] : f[1 < n ? 1 : 0];
        double right = (2 * i + 1 < n) ? f[2 * i + 1] : f[n - 1];
        if (dn == 0) { left = 0.0; right = 0.0; }
        f[2 * i] -= DWT97_DELTA * (left + right);
    }

    /* Inverse step 3: undo gamma */
    for (uint32_t i = 0; i < dn; i++) {
        double right = (2 * (i + 1) < n) ? f[2 * (i + 1)] : f[n - 2];
        f[2 * i + 1] -= DWT97_GAMMA * (f[2 * i] + right);
    }

    /* Inverse step 2: undo beta */
    for (uint32_t i = 0; i < sn; i++) {
        double left  = (i > 0)        ? f[2 * i - 1] : f[1 < n ? 1 : 0];
        double right = (2 * i + 1 < n) ? f[2 * i + 1] : f[n - 1];
        if (dn == 0) { left = 0.0; right = 0.0; }
        f[2 * i] -= DWT97_BETA * (left + right);
    }

    /* Inverse step 1: undo alpha */
    for (uint32_t i = 0; i < dn; i++) {
        double right = (2 * (i + 1) < n) ? f[2 * (i + 1)] : f[n - 2];
        f[2 * i + 1] -= DWT97_ALPHA * (f[2 * i] + right);
    }

    for (uint32_t i = 0; i < n; i++)
        x[i] = (int32_t)floor(f[i] + 0.5);

    return 1;
}

/* =========================================================================
 * 3-D forward / inverse DWT
 * ========================================================================= */

/** Maximum of two values. */
static uint32_t u32max(uint32_t a, uint32_t b) { return a > b ? a : b; }

int opj_dwt3d_fwd(int32_t *data,
                  uint32_t w, uint32_t h, uint32_t d,
                  uint32_t nx, uint32_t ny, uint32_t nz,
                  int32_t filter)
{
    if (!data || w == 0 || h == 0 || d == 0)
        return 0;

    uint32_t max_len = u32max(u32max(w, h), d);
    int32_t  *scratch_i = NULL;
    double   *scratch_f = NULL;

    if (filter == OPJ_JP3D_FILTER_53) {
        scratch_i = (int32_t *)opj_jp3d_malloc(max_len * sizeof(int32_t));
        if (!scratch_i) return 0;
    } else {
        scratch_f = (double *)opj_jp3d_malloc(max_len * sizeof(double));
        if (!scratch_f) return 0;
    }

    /* Current sub-band extents for recursive decomposition */
    uint32_t cur_w = w, cur_h = h, cur_d = d;
    uint32_t levels = u32max(u32max(nx, ny), nz);

    for (uint32_t lev = 0; lev < levels; lev++) {
        uint32_t do_x = (lev < nx) ? 1 : 0;
        uint32_t do_y = (lev < ny) ? 1 : 0;
        uint32_t do_z = (lev < nz) ? 1 : 0;

        /* Transform along X for every (z, y) pair */
        if (do_x && cur_w > 1) {
            for (uint32_t z = 0; z < cur_d; z++) {
                for (uint32_t y = 0; y < cur_h; y++) {
                    int32_t *row = data + (z * h + y) * w;
                    if (filter == OPJ_JP3D_FILTER_53)
                        dwt53_fwd_1d(row, cur_w, scratch_i);
                    else
                        dwt97_fwd_1d(row, cur_w, scratch_f);
                }
            }
        }

        /* Transform along Y for every (z, x) pair */
        if (do_y && cur_h > 1) {
            int32_t *col = (int32_t *)opj_jp3d_malloc(cur_h * sizeof(int32_t));
            if (!col) {
                opj_jp3d_free(scratch_i);
                opj_jp3d_free(scratch_f);
                return 0;
            }
            int32_t *col_scratch_i = NULL;
            double  *col_scratch_f = NULL;
            if (filter == OPJ_JP3D_FILTER_53) {
                col_scratch_i = (int32_t *)opj_jp3d_malloc(cur_h * sizeof(int32_t));
                if (!col_scratch_i) { opj_jp3d_free(col); opj_jp3d_free(scratch_i); return 0; }
            } else {
                col_scratch_f = (double *)opj_jp3d_malloc(cur_h * sizeof(double));
                if (!col_scratch_f) { opj_jp3d_free(col); opj_jp3d_free(scratch_f); return 0; }
            }
            for (uint32_t z = 0; z < cur_d; z++) {
                for (uint32_t x = 0; x < cur_w; x++) {
                    /* gather */
                    for (uint32_t y = 0; y < cur_h; y++)
                        col[y] = data[(z * h + y) * w + x];
                    if (filter == OPJ_JP3D_FILTER_53)
                        dwt53_fwd_1d(col, cur_h, col_scratch_i);
                    else
                        dwt97_fwd_1d(col, cur_h, col_scratch_f);
                    /* scatter */
                    for (uint32_t y = 0; y < cur_h; y++)
                        data[(z * h + y) * w + x] = col[y];
                }
            }
            opj_jp3d_free(col);
            opj_jp3d_free(col_scratch_i);
            opj_jp3d_free(col_scratch_f);
        }

        /* Transform along Z for every (y, x) pair */
        if (do_z && cur_d > 1) {
            int32_t *pillar = (int32_t *)opj_jp3d_malloc(cur_d * sizeof(int32_t));
            if (!pillar) {
                opj_jp3d_free(scratch_i);
                opj_jp3d_free(scratch_f);
                return 0;
            }
            int32_t *pil_scratch_i = NULL;
            double  *pil_scratch_f = NULL;
            if (filter == OPJ_JP3D_FILTER_53) {
                pil_scratch_i = (int32_t *)opj_jp3d_malloc(cur_d * sizeof(int32_t));
                if (!pil_scratch_i) { opj_jp3d_free(pillar); opj_jp3d_free(scratch_i); return 0; }
            } else {
                pil_scratch_f = (double *)opj_jp3d_malloc(cur_d * sizeof(double));
                if (!pil_scratch_f) { opj_jp3d_free(pillar); opj_jp3d_free(scratch_f); return 0; }
            }
            for (uint32_t y = 0; y < cur_h; y++) {
                for (uint32_t x = 0; x < cur_w; x++) {
                    for (uint32_t z = 0; z < cur_d; z++)
                        pillar[z] = data[(z * h + y) * w + x];
                    if (filter == OPJ_JP3D_FILTER_53)
                        dwt53_fwd_1d(pillar, cur_d, pil_scratch_i);
                    else
                        dwt97_fwd_1d(pillar, cur_d, pil_scratch_f);
                    for (uint32_t z = 0; z < cur_d; z++)
                        data[(z * h + y) * w + x] = pillar[z];
                }
            }
            opj_jp3d_free(pillar);
            opj_jp3d_free(pil_scratch_i);
            opj_jp3d_free(pil_scratch_f);
        }

        /* Advance to next LL..L subband */
        if (do_x) cur_w = (cur_w + 1) / 2;
        if (do_y) cur_h = (cur_h + 1) / 2;
        if (do_z) cur_d = (cur_d + 1) / 2;
    }

    opj_jp3d_free(scratch_i);
    opj_jp3d_free(scratch_f);
    return 1;
}

int opj_dwt3d_inv(int32_t *data,
                  uint32_t w, uint32_t h, uint32_t d,
                  uint32_t nx, uint32_t ny, uint32_t nz,
                  int32_t filter)
{
    if (!data || w == 0 || h == 0 || d == 0)
        return 0;

    /* Compute the subband extents at each level */
    uint32_t levels = u32max(u32max(nx, ny), nz);

    /* Store the subband sizes so we can invert from coarsest to finest */
    uint32_t *sw = (uint32_t *)opj_jp3d_malloc((levels + 1) * sizeof(uint32_t));
    uint32_t *sh = (uint32_t *)opj_jp3d_malloc((levels + 1) * sizeof(uint32_t));
    uint32_t *sd = (uint32_t *)opj_jp3d_malloc((levels + 1) * sizeof(uint32_t));
    if (!sw || !sh || !sd) {
        opj_jp3d_free(sw); opj_jp3d_free(sh); opj_jp3d_free(sd);
        return 0;
    }

    sw[0] = w; sh[0] = h; sd[0] = d;
    for (uint32_t lev = 0; lev < levels; lev++) {
        sw[lev + 1] = (lev < nx) ? (sw[lev] + 1) / 2 : sw[lev];
        sh[lev + 1] = (lev < ny) ? (sh[lev] + 1) / 2 : sh[lev];
        sd[lev + 1] = (lev < nz) ? (sd[lev] + 1) / 2 : sd[lev];
    }

    uint32_t max_len = u32max(u32max(w, h), d);

    /* Apply inverse from coarsest (levels) to finest (0) */
    for (int32_t lev = (int32_t)levels - 1; lev >= 0; lev--) {
        uint32_t cur_w = sw[lev];
        uint32_t cur_h = sh[lev];
        uint32_t cur_d = sd[lev];

        (void)max_len;

        uint32_t do_x = ((uint32_t)lev < nx) ? 1 : 0;
        uint32_t do_y = ((uint32_t)lev < ny) ? 1 : 0;
        uint32_t do_z = ((uint32_t)lev < nz) ? 1 : 0;

        /* Inverse Z */
        if (do_z && cur_d > 1) {
            int32_t *pillar = (int32_t *)opj_jp3d_malloc(cur_d * sizeof(int32_t));
            int32_t *pil_scratch_i = NULL;
            double  *pil_scratch_f = NULL;
            if (filter == OPJ_JP3D_FILTER_53) {
                pil_scratch_i = (int32_t *)opj_jp3d_malloc(cur_d * sizeof(int32_t));
            } else {
                pil_scratch_f = (double *)opj_jp3d_malloc(cur_d * sizeof(double));
            }
            if (!pillar || (!pil_scratch_i && !pil_scratch_f)) {
                opj_jp3d_free(pillar); opj_jp3d_free(pil_scratch_i);
                opj_jp3d_free(pil_scratch_f);
                opj_jp3d_free(sw); opj_jp3d_free(sh); opj_jp3d_free(sd);
                return 0;
            }
            for (uint32_t y = 0; y < cur_h; y++) {
                for (uint32_t x = 0; x < cur_w; x++) {
                    for (uint32_t z = 0; z < cur_d; z++)
                        pillar[z] = data[(z * h + y) * w + x];
                    if (filter == OPJ_JP3D_FILTER_53)
                        dwt53_inv_1d(pillar, cur_d, pil_scratch_i);
                    else
                        dwt97_inv_1d(pillar, cur_d, pil_scratch_f);
                    for (uint32_t z = 0; z < cur_d; z++)
                        data[(z * h + y) * w + x] = pillar[z];
                }
            }
            opj_jp3d_free(pillar);
            opj_jp3d_free(pil_scratch_i);
            opj_jp3d_free(pil_scratch_f);
        }

        /* Inverse Y */
        if (do_y && cur_h > 1) {
            int32_t *col = (int32_t *)opj_jp3d_malloc(cur_h * sizeof(int32_t));
            int32_t *col_scratch_i = NULL;
            double  *col_scratch_f = NULL;
            if (filter == OPJ_JP3D_FILTER_53) {
                col_scratch_i = (int32_t *)opj_jp3d_malloc(cur_h * sizeof(int32_t));
            } else {
                col_scratch_f = (double *)opj_jp3d_malloc(cur_h * sizeof(double));
            }
            if (!col || (!col_scratch_i && !col_scratch_f)) {
                opj_jp3d_free(col); opj_jp3d_free(col_scratch_i);
                opj_jp3d_free(col_scratch_f);
                opj_jp3d_free(sw); opj_jp3d_free(sh); opj_jp3d_free(sd);
                return 0;
            }
            for (uint32_t z = 0; z < cur_d; z++) {
                for (uint32_t x = 0; x < cur_w; x++) {
                    for (uint32_t y = 0; y < cur_h; y++)
                        col[y] = data[(z * h + y) * w + x];
                    if (filter == OPJ_JP3D_FILTER_53)
                        dwt53_inv_1d(col, cur_h, col_scratch_i);
                    else
                        dwt97_inv_1d(col, cur_h, col_scratch_f);
                    for (uint32_t y = 0; y < cur_h; y++)
                        data[(z * h + y) * w + x] = col[y];
                }
            }
            opj_jp3d_free(col);
            opj_jp3d_free(col_scratch_i);
            opj_jp3d_free(col_scratch_f);
        }

        /* Inverse X */
        if (do_x && cur_w > 1) {
            int32_t *scratch_i = NULL;
            double  *scratch_f = NULL;
            if (filter == OPJ_JP3D_FILTER_53) {
                scratch_i = (int32_t *)opj_jp3d_malloc(cur_w * sizeof(int32_t));
                if (!scratch_i) { opj_jp3d_free(sw); opj_jp3d_free(sh); opj_jp3d_free(sd); return 0; }
            } else {
                scratch_f = (double *)opj_jp3d_malloc(cur_w * sizeof(double));
                if (!scratch_f) { opj_jp3d_free(sw); opj_jp3d_free(sh); opj_jp3d_free(sd); return 0; }
            }
            for (uint32_t z = 0; z < cur_d; z++) {
                for (uint32_t y = 0; y < cur_h; y++) {
                    int32_t *row = data + (z * h + y) * w;
                    if (filter == OPJ_JP3D_FILTER_53)
                        dwt53_inv_1d(row, cur_w, scratch_i);
                    else
                        dwt97_inv_1d(row, cur_w, scratch_f);
                }
            }
            opj_jp3d_free(scratch_i);
            opj_jp3d_free(scratch_f);
        }
    }

    opj_jp3d_free(sw);
    opj_jp3d_free(sh);
    opj_jp3d_free(sd);
    return 1;
}
