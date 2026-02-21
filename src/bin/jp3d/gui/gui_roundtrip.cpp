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
 * @file gui_roundtrip.cpp
 * @brief Round-trip testing, diff viewer, batch test runner, and codestream
 *        inspector for Phase 8D of the OpenJP3D GUI.
 */

#include "gui_roundtrip.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cfloat>
#include <algorithm>
#include <string>
#include <vector>

#include "imgui.h"

extern "C" {
#include "opj_cs3d.h"
}

/* ================================================================== */
/*  Lifecycle                                                         */
/* ================================================================== */

void gui_roundtrip_state_init(GuiRoundtripState *rs)
{
    rs->show_roundtrip = false;
    rs->show_diff      = false;
    rs->show_batch     = false;
    rs->show_inspector = false;

    /* Round-trip defaults */
    rs->rt_filter      = OPJ_JP3D_FILTER_53;
    rs->rt_target_rate = 0.0f;
    rs->rt_htj2k       = false;
    rs->rt_resolutions = 3;
    rs->rt_running     = false;
    rs->rt_done        = false;
    rs->rt_elapsed_sec = 0.0;
    rs->rt_has_result  = false;
    memset(&rs->rt_metrics, 0, sizeof(rs->rt_metrics));
    rs->rt_error.clear();
    rs->rt_decoded     = NULL;

    /* Diff viewer */
    rs->diff.diff  = NULL;
    rs->diff.w     = 0;
    rs->diff.h     = 0;
    rs->diff.d     = 0;
    rs->diff.max_abs = 0.0f;
    rs->diff.valid = false;
    rs->diff_threshold  = 0;
    rs->diff_slice_idx  = 0;
    rs->diff_axis       = SLICE_AXIS_Z;
    rs->diff_tex_orig   = 0;
    rs->diff_tex_dec    = 0;
    rs->diff_tex_err    = 0;
    rs->diff_tex_w      = 0;
    rs->diff_tex_h      = 0;

    /* Batch test */
    rs->batch_entries.clear();
    rs->batch_running = false;
    rs->batch_done    = false;
    rs->batch_current = 0;
    rs->batch_total   = 0;

    /* Codestream inspector */
    rs->cs_nodes.clear();
    rs->cs_input_path[0] = '\0';
    rs->cs_parsed = false;
}

void gui_roundtrip_state_free(GuiRoundtripState *rs)
{
    /* Join round-trip worker */
    if (rs->rt_worker.joinable())
        rs->rt_worker.join();

    if (rs->rt_decoded) {
        opj_jp3d_destroy_volume(rs->rt_decoded);
        rs->rt_decoded = NULL;
    }

    /* Join batch worker */
    if (rs->batch_worker.joinable())
        rs->batch_worker.join();

    /* Free diff data */
    if (rs->diff.diff) {
        free(rs->diff.diff);
        rs->diff.diff = NULL;
    }
    rs->diff.valid = false;

    /* Delete GL textures */
    if (rs->diff_tex_orig) { glDeleteTextures(1, &rs->diff_tex_orig); rs->diff_tex_orig = 0; }
    if (rs->diff_tex_dec)  { glDeleteTextures(1, &rs->diff_tex_dec);  rs->diff_tex_dec = 0; }
    if (rs->diff_tex_err)  { glDeleteTextures(1, &rs->diff_tex_err);  rs->diff_tex_err = 0; }
}

/* ================================================================== */
/*  Tick — poll background tasks                                      */
/* ================================================================== */

void gui_roundtrip_tick(GuiRoundtripState *rs)
{
    if (rs->rt_running.load()) {
        auto now = std::chrono::steady_clock::now();
        rs->rt_elapsed_sec =
            std::chrono::duration<double>(now - rs->rt_start_time).count();
        if (rs->rt_done.load()) {
            if (rs->rt_worker.joinable())
                rs->rt_worker.join();
            rs->rt_running = false;
        }
    }

    if (rs->batch_running.load()) {
        if (rs->batch_done.load()) {
            if (rs->batch_worker.joinable())
                rs->batch_worker.join();
            rs->batch_running = false;
        }
    }
}

/* ================================================================== */
/*  Quality metrics                                                   */
/* ================================================================== */

/**
 * @brief Compare two volumes and compute quality metrics.
 *
 * Both volumes must have the same dimensions and precision.
 * Computes MSE, PSNR, max absolute error, and bit-exact check.
 */
static GuiRTMetrics compute_metrics(const opj_volume_t *orig,
                                    const opj_volume_t *decoded,
                                    size_t compressed_size,
                                    double encode_sec,
                                    double decode_sec)
{
    GuiRTMetrics m;
    memset(&m, 0, sizeof(m));
    m.compressed_size   = compressed_size;
    m.encode_sec        = encode_sec;
    m.decode_sec        = decode_sec;

    uint32_t w = orig->comps[0].w;
    uint32_t h = orig->comps[0].h;
    uint32_t d = orig->comps[0].d;
    size_t   n = (size_t)w * h * d;

    /* Compute original raw size for compression ratio */
    uint32_t prec = orig->comps[0].prec;
    size_t bytes_per_sample = (prec <= 8) ? 1 : (prec <= 16) ? 2 : 4;
    size_t raw_size = n * bytes_per_sample * orig->numcomps;
    m.compression_ratio = (compressed_size > 0)
        ? (double)raw_size / (double)compressed_size : 0.0;

    /* Compare component 0 */
    const int32_t *a = orig->comps[0].data;
    const int32_t *b = decoded->comps[0].data;

    double sum_sq = 0.0;
    double max_err = 0.0;
    bool exact = true;

    for (size_t i = 0; i < n; i++) {
        double diff = (double)a[i] - (double)b[i];
        if (diff != 0.0) exact = false;
        sum_sq += diff * diff;
        double abs_diff = fabs(diff);
        if (abs_diff > max_err) max_err = abs_diff;
    }

    m.lossless    = exact;
    m.mse         = sum_sq / (double)n;
    m.max_abs_err = max_err;

    if (m.mse < 1e-15) {
        m.psnr = INFINITY;
    } else {
        double peak = (double)((1u << prec) - 1);
        m.psnr = 10.0 * log10(peak * peak / m.mse);
    }

    return m;
}

/**
 * @brief Build a signed difference volume (orig - decoded).
 */
static void build_diff_volume(GuiDiffData *dd,
                               const opj_volume_t *orig,
                               const opj_volume_t *decoded)
{
    uint32_t w = orig->comps[0].w;
    uint32_t h = orig->comps[0].h;
    uint32_t d = orig->comps[0].d;
    size_t   n = (size_t)w * h * d;

    if (dd->diff) free(dd->diff);
    dd->diff = (int32_t *)malloc(n * sizeof(int32_t));
    if (!dd->diff) {
        dd->valid = false;
        return;
    }

    const int32_t *a = orig->comps[0].data;
    const int32_t *b = decoded->comps[0].data;
    float max_abs = 0.0f;

    for (size_t i = 0; i < n; i++) {
        int32_t v = a[i] - b[i];
        dd->diff[i] = v;
        float av = (float)abs(v);
        if (av > max_abs) max_abs = av;
    }

    dd->w = w;
    dd->h = h;
    dd->d = d;
    dd->max_abs = max_abs;
    dd->valid = true;
}

/* ================================================================== */
/*  Round-trip worker thread (8D.1)                                   */
/* ================================================================== */

static void roundtrip_worker(GuiRoundtripState *rs,
                              opj_volume_t *orig,
                              opj_jp3d_enc_params_t enc)
{
    /* Encode */
    auto t0 = std::chrono::steady_clock::now();

    uint8_t *cs_data = NULL;
    size_t   cs_size = 0;
    opj_jp3d_bool_t ok = opj_jp3d_encode(
        orig, &enc, &cs_data, &cs_size, NULL, NULL);

    auto t1 = std::chrono::steady_clock::now();
    double enc_sec = std::chrono::duration<double>(t1 - t0).count();

    if (!ok || !cs_data) {
        rs->rt_error = "Encoding failed.";
        rs->rt_has_result = false;
        rs->rt_done = true;
        return;
    }

    /* Decode */
    auto t2 = std::chrono::steady_clock::now();

    opj_jp3d_dec_params_t dec = {};
    dec.verbose = 0;
    opj_volume_t *decoded = opj_jp3d_decode(cs_data, cs_size, &dec, NULL, NULL);

    auto t3 = std::chrono::steady_clock::now();
    double dec_sec = std::chrono::duration<double>(t3 - t2).count();

    if (!decoded) {
        opj_jp3d_free(cs_data);
        rs->rt_error = "Decoding failed.";
        rs->rt_has_result = false;
        rs->rt_done = true;
        return;
    }

    /* Compare */
    rs->rt_metrics = compute_metrics(orig, decoded, cs_size, enc_sec, dec_sec);
    rs->rt_has_result = true;

    /* Keep decoded volume for diff viewer */
    rs->rt_decoded = decoded;

    opj_jp3d_free(cs_data);
    rs->rt_done = true;
}

/* ================================================================== */
/*  Batch test worker (8D.3)                                          */
/* ================================================================== */

static void batch_worker(GuiRoundtripState *rs, opj_volume_t *orig)
{
    int total = (int)rs->batch_entries.size();
    rs->batch_total = total;

    for (int i = 0; i < total; i++) {
        rs->batch_current = i;
        GuiBatchEntry &e = rs->batch_entries[(size_t)i];

        opj_jp3d_enc_params_t enc;
        opj_jp3d_set_default_encoder_parameters(&enc);
        enc.filter            = (int32_t)e.filter;
        enc.target_rate       = e.target_rate;
        enc.use_htj2k         = e.htj2k ? OPJ_JP3D_USE_HTJ2K : 0;
        enc.num_resolutions_x = (uint32_t)e.resolutions;
        enc.num_resolutions_y = (uint32_t)e.resolutions;
        enc.num_resolutions_z = (uint32_t)e.resolutions;
        enc.tile_width        = (uint32_t)e.tile_w;
        enc.tile_height       = (uint32_t)e.tile_h;
        enc.tile_depth        = (uint32_t)e.tile_d;
        enc.verbose           = 0;

        /* Encode */
        auto t0 = std::chrono::steady_clock::now();
        uint8_t *cs_data = NULL;
        size_t   cs_size = 0;
        opj_jp3d_bool_t ok = opj_jp3d_encode(
            orig, &enc, &cs_data, &cs_size, NULL, NULL);
        auto t1 = std::chrono::steady_clock::now();
        e.encode_sec = std::chrono::duration<double>(t1 - t0).count();

        if (!ok || !cs_data) {
            e.ran = true;
            e.pass = false;
            e.error_msg = "Encoding failed";
            continue;
        }

        /* Decode */
        auto t2 = std::chrono::steady_clock::now();
        opj_jp3d_dec_params_t dec = {};
        dec.verbose = 0;
        opj_volume_t *decoded = opj_jp3d_decode(cs_data, cs_size,
                                                &dec, NULL, NULL);
        auto t3 = std::chrono::steady_clock::now();
        e.decode_sec = std::chrono::duration<double>(t3 - t2).count();

        if (!decoded) {
            opj_jp3d_free(cs_data);
            e.ran = true;
            e.pass = false;
            e.error_msg = "Decoding failed";
            continue;
        }

        /* Metrics */
        GuiRTMetrics m = compute_metrics(orig, decoded, cs_size,
                                         e.encode_sec, e.decode_sec);

        e.ran  = true;
        e.pass = true;
        e.psnr = m.psnr;
        e.mse  = m.mse;
        e.compression_ratio = m.compression_ratio;

        /* For lossless, verify bit-exactness */
        if (e.target_rate == 0.0f && e.filter == OPJ_JP3D_FILTER_53) {
            if (!m.lossless) {
                e.pass = false;
                e.error_msg = "Lossless not bit-exact";
            }
        }

        opj_jp3d_destroy_volume(decoded);
        opj_jp3d_free(cs_data);
    }

    rs->batch_done = true;
}

/* ================================================================== */
/*  Codestream parser (8D.4)                                          */
/* ================================================================== */

/** Read a big-endian uint16 from a buffer. */
static inline uint16_t rd_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] << 8 | (uint16_t)p[1]);
}

/** Read a big-endian uint32 from a buffer. */
static inline uint32_t rd_u32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}

/** Read a big-endian float32 from a buffer. */
static inline float rd_f32(const uint8_t *p)
{
    uint32_t u = rd_u32(p);
    float f;
    memcpy(&f, &u, 4);
    return f;
}

static void parse_codestream(GuiRoundtripState *rs,
                              const uint8_t *data, size_t size)
{
    rs->cs_nodes.clear();
    rs->cs_parsed = true;

    size_t pos = 0;
    while (pos + 2 <= size) {
        uint16_t marker = rd_u16(data + pos);

        if (marker == OPJ_CS3D_SOC) {
            GuiCSNode node;
            node.label  = "SOC";
            node.detail = "Start of Codestream";
            node.offset = (uint32_t)pos;
            node.marker = marker;
            rs->cs_nodes.push_back(node);
            pos += 2;
        }
        else if (marker == OPJ_CS3D_SIZ3D) {
            GuiCSNode node;
            node.label  = "SIZ3D";
            node.offset = (uint32_t)pos;
            node.marker = marker;
            pos += 2;

            if (pos + 44 <= size) {
                uint32_t x1 = rd_u32(data + pos);       pos += 4;
                uint32_t y1 = rd_u32(data + pos);       pos += 4;
                uint32_t z1 = rd_u32(data + pos);       pos += 4;
                /* skip x0, y0, z0 */
                pos += 12;
                uint32_t tw = rd_u32(data + pos);       pos += 4;
                uint32_t th = rd_u32(data + pos);       pos += 4;
                uint32_t td = rd_u32(data + pos);       pos += 4;
                uint32_t nc = rd_u32(data + pos);       pos += 4;
                /* skip colour_space */
                pos += 4;

                char det[256];
                snprintf(det, sizeof(det),
                         "Volume: %ux%ux%u, Tile: %ux%ux%u, Components: %u",
                         x1, y1, z1, tw, th, td, nc);
                node.detail = det;

                /* Per-component children */
                for (uint32_t c = 0; c < nc && pos + 20 <= size; c++) {
                    uint32_t cw   = rd_u32(data + pos); pos += 4;
                    uint32_t ch   = rd_u32(data + pos); pos += 4;
                    uint32_t cd   = rd_u32(data + pos); pos += 4;
                    uint32_t prec = rd_u32(data + pos); pos += 4;
                    uint32_t sgnd = rd_u32(data + pos); pos += 4;

                    GuiCSNode child;
                    char clbl[64], cdet[128];
                    snprintf(clbl, sizeof(clbl), "Component %u", c);
                    snprintf(cdet, sizeof(cdet),
                             "%ux%ux%u, %u-bit %s",
                             cw, ch, cd, prec,
                             sgnd ? "signed" : "unsigned");
                    child.label  = clbl;
                    child.detail = cdet;
                    child.offset = 0;
                    child.marker = 0;
                    node.children.push_back(child);
                }
            } else {
                node.detail = "(truncated)";
            }
            rs->cs_nodes.push_back(node);
        }
        else if (marker == OPJ_CS3D_COD3D) {
            GuiCSNode node;
            node.label  = "COD3D";
            node.offset = (uint32_t)pos;
            node.marker = marker;
            pos += 2;

            if (pos + 36 <= size) {
                uint32_t filter  = rd_u32(data + pos);  pos += 4;
                uint32_t htj2k   = rd_u32(data + pos);  pos += 4;
                uint32_t rx      = rd_u32(data + pos);  pos += 4;
                uint32_t ry      = rd_u32(data + pos);  pos += 4;
                uint32_t rz      = rd_u32(data + pos);  pos += 4;
                uint32_t cbw     = rd_u32(data + pos);  pos += 4;
                uint32_t cbh     = rd_u32(data + pos);  pos += 4;
                uint32_t cbd     = rd_u32(data + pos);  pos += 4;
                uint32_t layers  = rd_u32(data + pos);  pos += 4;

                char det[256];
                snprintf(det, sizeof(det),
                         "Filter: %s, HTJ2K: %s, DWT: %ux%ux%u, "
                         "CBlk: %ux%ux%u, Layers: %u",
                         filter == (uint32_t)OPJ_JP3D_FILTER_97
                             ? "9/7" : "5/3",
                         htj2k ? "Yes" : "No",
                         rx, ry, rz, cbw, cbh, cbd, layers);
                node.detail = det;
            } else {
                node.detail = "(truncated)";
            }
            rs->cs_nodes.push_back(node);
        }
        else if (marker == OPJ_CS3D_QCD3D) {
            GuiCSNode node;
            node.label  = "QCD3D";
            node.offset = (uint32_t)pos;
            node.marker = marker;
            pos += 2;

            if (pos + 4 <= size) {
                float rate = rd_f32(data + pos); pos += 4;
                char det[128];
                if (rate == 0.0f)
                    snprintf(det, sizeof(det), "Target rate: lossless");
                else
                    snprintf(det, sizeof(det), "Target rate: %.4f bps", (double)rate);
                node.detail = det;
            } else {
                node.detail = "(truncated)";
            }
            rs->cs_nodes.push_back(node);
        }
        else if (marker == OPJ_CS3D_SOT) {
            GuiCSNode node;
            node.label  = "SOT";
            node.offset = (uint32_t)pos;
            node.marker = marker;
            pos += 2;

            if (pos + 8 <= size) {
                uint32_t tp_idx  = rd_u32(data + pos);  pos += 4;
                uint32_t tp_len  = rd_u32(data + pos);  pos += 4;

                char det[128];
                snprintf(det, sizeof(det),
                         "Tile-part %u, data length: %u bytes", tp_idx, tp_len);
                node.detail = det;
            } else {
                node.detail = "(truncated)";
                pos += 2;
            }
            rs->cs_nodes.push_back(node);
        }
        else if (marker == OPJ_CS3D_SOD) {
            GuiCSNode node;
            node.label  = "SOD";
            node.detail = "Start of Data";
            node.offset = (uint32_t)pos;
            node.marker = marker;
            pos += 2;

            /* Skip over compressed data until next marker (0xFF prefix) */
            while (pos + 1 < size) {
                if (data[pos] == 0xFF) break;
                pos++;
            }
            rs->cs_nodes.push_back(node);
        }
        else if (marker == OPJ_CS3D_EOC) {
            GuiCSNode node;
            node.label  = "EOC";
            node.detail = "End of Codestream";
            node.offset = (uint32_t)pos;
            node.marker = marker;
            rs->cs_nodes.push_back(node);
            break;
        }
        else {
            /* Unknown marker — skip */
            GuiCSNode node;
            char lbl[32];
            snprintf(lbl, sizeof(lbl), "0x%04X", marker);
            node.label  = lbl;
            node.detail = "Unknown marker";
            node.offset = (uint32_t)pos;
            node.marker = marker;
            rs->cs_nodes.push_back(node);
            pos += 2;
        }
    }
}

/* ================================================================== */
/*  Diff texture helpers (8D.2)                                       */
/* ================================================================== */

/**
 * @brief Create or update a GL texture from RGBA pixel data.
 */
static void upload_rgba_texture(GLuint *tex, int w, int h,
                                 const uint8_t *pixels)
{
    if (*tex == 0) glGenTextures(1, tex);
    glBindTexture(GL_TEXTURE_2D, *tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);
}

/**
 * @brief Extract a 2-D slice from volume data and produce RGBA pixels.
 *
 * Uses simple min/max normalisation.
 */
static void extract_slice_rgba(const opj_volume_t *vol,
                                GuiSliceAxis axis, int slice_idx,
                                std::vector<uint8_t> &pixels,
                                int *out_w, int *out_h)
{
    uint32_t w = vol->comps[0].w;
    uint32_t h = vol->comps[0].h;
    uint32_t d = vol->comps[0].d;
    const int32_t *data = vol->comps[0].data;

    int sw, sh;
    switch (axis) {
    case SLICE_AXIS_Z: sw = (int)w; sh = (int)h; break;
    case SLICE_AXIS_X: sw = (int)h; sh = (int)d; break;
    case SLICE_AXIS_Y: sw = (int)w; sh = (int)d; break;
    default:           sw = (int)w; sh = (int)h; break;
    }

    *out_w = sw;
    *out_h = sh;
    pixels.resize((size_t)sw * (size_t)sh * 4);

    /* Find range for normalisation */
    int32_t vmin = INT32_MAX, vmax = INT32_MIN;
    size_t n = (size_t)w * h * d;
    for (size_t i = 0; i < n; i++) {
        if (data[i] < vmin) vmin = data[i];
        if (data[i] > vmax) vmax = data[i];
    }
    float range = (float)(vmax - vmin);
    if (range < 1.0f) range = 1.0f;

    for (int y = 0; y < sh; y++) {
        for (int x = 0; x < sw; x++) {
            int32_t val = 0;
            switch (axis) {
            case SLICE_AXIS_Z:
                val = data[(size_t)slice_idx * w * h + (size_t)y * w + (size_t)x];
                break;
            case SLICE_AXIS_X:
                val = data[(size_t)y * w * h + (size_t)x * w + (size_t)slice_idx];
                break;
            case SLICE_AXIS_Y:
                val = data[(size_t)y * w * h + (size_t)slice_idx * w + (size_t)x];
                break;
            }
            uint8_t g = (uint8_t)(((float)(val - vmin) / range) * 255.0f);
            size_t off = ((size_t)y * sw + x) * 4;
            pixels[off + 0] = g;
            pixels[off + 1] = g;
            pixels[off + 2] = g;
            pixels[off + 3] = 255;
        }
    }
}

/**
 * @brief Build an error-map slice as RGBA.
 *
 * Absolute difference is mapped: 0 → black, >threshold → red gradient.
 */
static void build_errmap_slice(const GuiDiffData *dd,
                                GuiSliceAxis axis, int slice_idx,
                                int threshold,
                                std::vector<uint8_t> &pixels,
                                int *out_w, int *out_h)
{
    uint32_t w = dd->w;
    uint32_t h = dd->h;

    int sw, sh;
    switch (axis) {
    case SLICE_AXIS_Z: sw = (int)w; sh = (int)h; break;
    case SLICE_AXIS_X: sw = (int)h; sh = (int)dd->d; break;
    case SLICE_AXIS_Y: sw = (int)w; sh = (int)dd->d; break;
    default:           sw = (int)w; sh = (int)h; break;
    }

    *out_w = sw;
    *out_h = sh;
    pixels.resize((size_t)sw * (size_t)sh * 4);

    float norm = dd->max_abs > 0.0f ? dd->max_abs : 1.0f;

    for (int y = 0; y < sh; y++) {
        for (int x = 0; x < sw; x++) {
            int32_t val = 0;
            switch (axis) {
            case SLICE_AXIS_Z:
                val = dd->diff[(size_t)slice_idx * w * h + (size_t)y * w + (size_t)x];
                break;
            case SLICE_AXIS_X:
                val = dd->diff[(size_t)y * w * h + (size_t)x * w + (size_t)slice_idx];
                break;
            case SLICE_AXIS_Y:
                val = dd->diff[(size_t)y * w * h + (size_t)slice_idx * w + (size_t)x];
                break;
            }

            int32_t absval = abs(val);
            size_t off = ((size_t)y * sw + x) * 4;

            if (absval <= threshold) {
                /* Below threshold: black/dark */
                pixels[off + 0] = 0;
                pixels[off + 1] = 0;
                pixels[off + 2] = 0;
                pixels[off + 3] = 255;
            } else {
                /* Above threshold: red gradient */
                float t = (float)absval / norm;
                if (t > 1.0f) t = 1.0f;
                uint8_t r = (uint8_t)(t * 255.0f);
                pixels[off + 0] = r;
                pixels[off + 1] = 0;
                pixels[off + 2] = 0;
                pixels[off + 3] = 255;
            }
        }
    }
}

/* ================================================================== */
/*  Round-Trip Test Wizard (8D.1)                                     */
/* ================================================================== */

void gui_roundtrip_draw_wizard(GuiRoundtripState *rs, GuiVolumeState *vol)
{
    if (!rs->show_roundtrip) return;

    ImGui::SetNextWindowSize(ImVec2(520, 480), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Round-Trip Test", &rs->show_roundtrip,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    bool busy = rs->rt_running.load();
    bool has_vol = vol && vol->loaded;

    if (!has_vol) {
        ImGui::TextWrapped("Load a volume first (File > Open Volume) "
                           "to run a round-trip test.");
        ImGui::End();
        return;
    }

    ImGui::SeparatorText("Encode Parameters");

    const char *filter_items[] = { "5/3 Lossless", "9/7 Lossy" };
    ImGui::Combo("Filter", &rs->rt_filter, filter_items, 2);
    ImGui::SliderFloat("Target Rate", &rs->rt_target_rate, 0.0f, 8.0f,
                       rs->rt_target_rate == 0.0f ? "Lossless" : "%.2f bps");
    ImGui::Checkbox("HTJ2K", &rs->rt_htj2k);
    ImGui::SliderInt("DWT Levels", &rs->rt_resolutions, 0, 8);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    /* Run button */
    if (busy) ImGui::BeginDisabled();
    if (ImGui::Button("Run Round-Trip", ImVec2(160, 0))) {
        /* Build encoder params */
        opj_jp3d_enc_params_t enc;
        opj_jp3d_set_default_encoder_parameters(&enc);
        enc.filter            = (int32_t)rs->rt_filter;
        enc.target_rate       = rs->rt_target_rate;
        enc.use_htj2k         = rs->rt_htj2k ? OPJ_JP3D_USE_HTJ2K : 0;
        enc.num_resolutions_x = (uint32_t)rs->rt_resolutions;
        enc.num_resolutions_y = (uint32_t)rs->rt_resolutions;
        enc.num_resolutions_z = (uint32_t)rs->rt_resolutions;
        enc.verbose           = 0;

        /* Reset state */
        if (rs->rt_decoded) {
            opj_jp3d_destroy_volume(rs->rt_decoded);
            rs->rt_decoded = NULL;
        }
        rs->rt_has_result = false;
        rs->rt_error.clear();
        rs->rt_running     = true;
        rs->rt_done        = false;
        rs->rt_elapsed_sec = 0.0;
        rs->rt_start_time  = std::chrono::steady_clock::now();

        /* Free old diff data */
        if (rs->diff.diff) { free(rs->diff.diff); rs->diff.diff = NULL; }
        rs->diff.valid = false;

        rs->rt_worker = std::thread(roundtrip_worker, rs, vol->vol, enc);
    }
    if (busy) ImGui::EndDisabled();

    /* Progress */
    if (busy) {
        ImGui::SameLine();
        float t = (float)fmod(rs->rt_elapsed_sec * 0.5, 1.0);
        ImGui::ProgressBar(t, ImVec2(200, 0), "Running...");
        int secs = (int)rs->rt_elapsed_sec;
        ImGui::SameLine();
        ImGui::Text("%d:%02d", secs / 60, secs % 60);
    }

    /* Results */
    if (rs->rt_has_result) {
        ImGui::Spacing();
        ImGui::SeparatorText("Results");

        const GuiRTMetrics &m = rs->rt_metrics;

        if (m.lossless) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f),
                               "PASS — Lossless (bit-exact)");
        } else if (m.psnr > 0.0) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
                               "LOSSY — Not bit-exact");
        }

        ImGui::Text("PSNR:              %.2f dB",
                     isinf(m.psnr) ? INFINITY : m.psnr);
        ImGui::Text("MSE:               %.6f", m.mse);
        ImGui::Text("Max Abs Error:     %.0f", m.max_abs_err);
        ImGui::Text("Compressed Size:   %zu bytes", m.compressed_size);
        ImGui::Text("Compression Ratio: %.2f : 1", m.compression_ratio);
        ImGui::Text("Encode Time:       %.3f s", m.encode_sec);
        ImGui::Text("Decode Time:       %.3f s", m.decode_sec);

        ImGui::Spacing();
        if (rs->rt_decoded) {
            if (ImGui::Button("Open Diff Viewer")) {
                /* Build diff volume */
                build_diff_volume(&rs->diff, vol->vol, rs->rt_decoded);
                rs->diff_slice_idx = 0;
                rs->diff_axis      = SLICE_AXIS_Z;
                rs->diff_threshold = 0;
                rs->show_diff      = true;
            }
        }
    }

    if (!rs->rt_error.empty()) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        ImGui::TextWrapped("Error: %s", rs->rt_error.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::End();
}

/* ================================================================== */
/*  Diff Viewer (8D.2)                                                */
/* ================================================================== */

void gui_roundtrip_draw_diff(GuiRoundtripState *rs, GuiVolumeState *vol)
{
    if (!rs->show_diff) return;

    ImGui::SetNextWindowSize(ImVec2(800, 500), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Diff Viewer", &rs->show_diff,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    if (!rs->diff.valid || !vol || !vol->loaded || !rs->rt_decoded) {
        ImGui::TextWrapped("Run a round-trip test first to compare volumes.");
        ImGui::End();
        return;
    }

    /* Controls */
    const char *axes[] = { "Axial (Z)", "Sagittal (X)", "Coronal (Y)" };
    int ax = (int)rs->diff_axis;
    ImGui::PushItemWidth(130);
    if (ImGui::Combo("Axis##diff", &ax, axes, 3)) {
        rs->diff_axis = (GuiSliceAxis)ax;
        rs->diff_slice_idx = 0;
    }
    ImGui::PopItemWidth();

    int max_slice = 0;
    switch (rs->diff_axis) {
    case SLICE_AXIS_Z: max_slice = (int)rs->diff.d - 1; break;
    case SLICE_AXIS_X: max_slice = (int)rs->diff.w - 1; break;
    case SLICE_AXIS_Y: max_slice = (int)rs->diff.h - 1; break;
    }
    if (max_slice < 0) max_slice = 0;

    ImGui::SameLine();
    ImGui::PushItemWidth(200);
    ImGui::SliderInt("Slice##diff", &rs->diff_slice_idx, 0, max_slice);
    ImGui::PopItemWidth();

    ImGui::SameLine();
    ImGui::PushItemWidth(120);
    ImGui::SliderInt("Threshold", &rs->diff_threshold, 0, 255);
    ImGui::PopItemWidth();

    if (rs->diff_slice_idx > max_slice)
        rs->diff_slice_idx = max_slice;

    ImGui::Separator();

    /* Build slice textures */
    std::vector<uint8_t> pixels;
    int tw, th;

    /* Original */
    extract_slice_rgba(vol->vol, rs->diff_axis, rs->diff_slice_idx,
                       pixels, &tw, &th);
    upload_rgba_texture(&rs->diff_tex_orig, tw, th, pixels.data());

    /* Decoded */
    extract_slice_rgba(rs->rt_decoded, rs->diff_axis, rs->diff_slice_idx,
                       pixels, &tw, &th);
    upload_rgba_texture(&rs->diff_tex_dec, tw, th, pixels.data());

    /* Error map */
    int ew, eh;
    build_errmap_slice(&rs->diff, rs->diff_axis, rs->diff_slice_idx,
                       rs->diff_threshold, pixels, &ew, &eh);
    upload_rgba_texture(&rs->diff_tex_err, ew, eh, pixels.data());

    rs->diff_tex_w = tw;
    rs->diff_tex_h = th;

    /* Layout: three images side-by-side */
    float avail_w = ImGui::GetContentRegionAvail().x;
    float avail_h = ImGui::GetContentRegionAvail().y - 20.0f;
    float img_w = (avail_w - 20.0f) / 3.0f;
    float aspect = (tw > 0 && th > 0) ? (float)tw / (float)th : 1.0f;
    float img_h = img_w / aspect;
    if (img_h > avail_h) {
        img_h = avail_h;
        img_w = img_h * aspect;
    }

    ImGui::Text("Original");
    ImGui::SameLine(img_w + 10.0f);
    ImGui::Text("Decoded");
    ImGui::SameLine(2.0f * (img_w + 10.0f));
    ImGui::Text("Error Map");

    if (rs->diff_tex_orig) {
        ImGui::Image((ImTextureID)(intptr_t)rs->diff_tex_orig,
                     ImVec2(img_w, img_h));
    }
    ImGui::SameLine();
    if (rs->diff_tex_dec) {
        ImGui::Image((ImTextureID)(intptr_t)rs->diff_tex_dec,
                     ImVec2(img_w, img_h));
    }
    ImGui::SameLine();
    if (rs->diff_tex_err) {
        ImGui::Image((ImTextureID)(intptr_t)rs->diff_tex_err,
                     ImVec2(img_w, img_h));
    }

    ImGui::End();
}

/* ================================================================== */
/*  Batch Test Runner (8D.3)                                          */
/* ================================================================== */

void gui_roundtrip_draw_batch(GuiRoundtripState *rs, GuiVolumeState *vol)
{
    if (!rs->show_batch) return;

    ImGui::SetNextWindowSize(ImVec2(860, 480), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Batch Test Runner", &rs->show_batch,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    bool has_vol = vol && vol->loaded;
    bool busy    = rs->batch_running.load();

    if (!has_vol) {
        ImGui::TextWrapped("Load a volume first to run batch tests.");
        ImGui::End();
        return;
    }

    /* ---- Add test entry ---- */
    ImGui::SeparatorText("Add Test Configuration");

    static int   b_filter = OPJ_JP3D_FILTER_53;
    static float b_rate   = 0.0f;
    static bool  b_htj2k  = false;
    static int   b_res    = 3;
    static int   b_tw = 0, b_th = 0, b_td = 0;

    const char *filt_items[] = { "5/3 Lossless", "9/7 Lossy" };
    ImGui::PushItemWidth(130);
    ImGui::Combo("Filter##b", &b_filter, filt_items, 2);
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::PushItemWidth(120);
    ImGui::SliderFloat("Rate##b", &b_rate, 0.0f, 8.0f,
                       b_rate == 0.0f ? "Lossless" : "%.2f");
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::Checkbox("HTJ2K##b", &b_htj2k);
    ImGui::SameLine();
    ImGui::PushItemWidth(80);
    ImGui::SliderInt("DWT##b", &b_res, 0, 8);
    ImGui::PopItemWidth();

    ImGui::PushItemWidth(80);
    ImGui::InputInt("TileW##b", &b_tw); ImGui::SameLine();
    ImGui::InputInt("TileH##b", &b_th); ImGui::SameLine();
    ImGui::InputInt("TileD##b", &b_td);
    ImGui::PopItemWidth();
    if (b_tw < 0) b_tw = 0;
    if (b_th < 0) b_th = 0;
    if (b_td < 0) b_td = 0;

    ImGui::SameLine();
    if (ImGui::Button("Add")) {
        GuiBatchEntry e;
        e.filter      = b_filter;
        e.target_rate = b_rate;
        e.htj2k       = b_htj2k;
        e.resolutions = b_res;
        e.tile_w = b_tw; e.tile_h = b_th; e.tile_d = b_td;
        e.ran = false; e.pass = false;
        e.psnr = 0.0; e.mse = 0.0;
        e.compression_ratio = 0.0;
        e.encode_sec = 0.0; e.decode_sec = 0.0;
        rs->batch_entries.push_back(e);
    }
    ImGui::SameLine();
    if (ImGui::Button("Add Defaults")) {
        /* Add a standard set of test configurations */
        int filters[] = { OPJ_JP3D_FILTER_53, OPJ_JP3D_FILTER_97,
                          OPJ_JP3D_FILTER_53 };
        float rates[] = { 0.0f, 2.0f, 0.0f };
        bool  hts[]   = { false, false, true };
        for (int i = 0; i < 3; i++) {
            GuiBatchEntry e;
            e.filter = filters[i]; e.target_rate = rates[i];
            e.htj2k = hts[i]; e.resolutions = 3;
            e.tile_w = 0; e.tile_h = 0; e.tile_d = 0;
            e.ran = false; e.pass = false;
            e.psnr = 0.0; e.mse = 0.0;
            e.compression_ratio = 0.0;
            e.encode_sec = 0.0; e.decode_sec = 0.0;
            rs->batch_entries.push_back(e);
        }
    }

    ImGui::Spacing();

    /* ---- Control buttons ---- */
    if (busy) ImGui::BeginDisabled();
    if (ImGui::Button("Run All", ImVec2(100, 0))) {
        /* Reset results */
        for (auto &e : rs->batch_entries) {
            e.ran = false; e.pass = false; e.error_msg.clear();
        }
        rs->batch_running = true;
        rs->batch_done    = false;
        rs->batch_current = 0;
        rs->batch_worker = std::thread(batch_worker, rs, vol->vol);
    }
    if (busy) ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Clear All", ImVec2(100, 0)) && !busy) {
        rs->batch_entries.clear();
    }

    ImGui::SameLine();
    if (ImGui::Button("Export CSV", ImVec2(100, 0)) && !busy) {
        /* Write results to batch_results.csv */
        FILE *fp = fopen("batch_results.csv", "w");
        if (fp) {
            fprintf(fp, "Filter,Rate,HTJ2K,DWT,TileW,TileH,TileD,"
                        "Pass,PSNR,MSE,Ratio,EncTime,DecTime,Error\n");
            for (const auto &e : rs->batch_entries) {
                fprintf(fp, "%s,%.2f,%s,%d,%d,%d,%d,%s,%.2f,%.6f,%.2f,"
                            "%.3f,%.3f,%s\n",
                        e.filter == OPJ_JP3D_FILTER_97 ? "9/7" : "5/3",
                        (double)e.target_rate,
                        e.htj2k ? "Yes" : "No",
                        e.resolutions,
                        e.tile_w, e.tile_h, e.tile_d,
                        e.ran ? (e.pass ? "PASS" : "FAIL") : "—",
                        e.psnr, e.mse, e.compression_ratio,
                        e.encode_sec, e.decode_sec,
                        e.error_msg.c_str());
            }
            fclose(fp);
        }
    }

    if (busy) {
        ImGui::SameLine();
        ImGui::Text("Running %d / %d ...",
                     rs->batch_current + 1, rs->batch_total);
    }

    ImGui::Spacing();

    /* ---- Results table ---- */
    ImGui::SeparatorText("Results");

    if (ImGui::BeginTable("BatchResults", 10,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
            ImVec2(0, ImGui::GetContentRegionAvail().y))) {

        ImGui::TableSetupColumn("Filter",  ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableSetupColumn("Rate",    ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("HTJ2K",   ImGuiTableColumnFlags_WidthFixed, 45);
        ImGui::TableSetupColumn("DWT",     ImGuiTableColumnFlags_WidthFixed, 35);
        ImGui::TableSetupColumn("Pass",    ImGuiTableColumnFlags_WidthFixed, 45);
        ImGui::TableSetupColumn("PSNR",    ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupColumn("MSE",     ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Ratio",   ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Enc(s)",  ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Dec(s)",  ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < rs->batch_entries.size(); i++) {
            const GuiBatchEntry &e = rs->batch_entries[i];
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", e.filter == OPJ_JP3D_FILTER_97 ? "9/7" : "5/3");

            ImGui::TableSetColumnIndex(1);
            if (e.target_rate == 0.0f)
                ImGui::Text("Lossless");
            else
                ImGui::Text("%.2f", (double)e.target_rate);

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", e.htj2k ? "Yes" : "No");

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%d", e.resolutions);

            ImGui::TableSetColumnIndex(4);
            if (!e.ran)
                ImGui::TextDisabled("--");
            else if (e.pass)
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "PASS");
            else
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "FAIL");

            ImGui::TableSetColumnIndex(5);
            if (e.ran)
                ImGui::Text("%.2f", e.psnr);
            else
                ImGui::TextDisabled("--");

            ImGui::TableSetColumnIndex(6);
            if (e.ran)
                ImGui::Text("%.6f", e.mse);
            else
                ImGui::TextDisabled("--");

            ImGui::TableSetColumnIndex(7);
            if (e.ran)
                ImGui::Text("%.2f", e.compression_ratio);
            else
                ImGui::TextDisabled("--");

            ImGui::TableSetColumnIndex(8);
            if (e.ran)
                ImGui::Text("%.3f", e.encode_sec);
            else
                ImGui::TextDisabled("--");

            ImGui::TableSetColumnIndex(9);
            if (e.ran)
                ImGui::Text("%.3f", e.decode_sec);
            else
                ImGui::TextDisabled("--");
        }
        ImGui::EndTable();
    }

    ImGui::End();
}

/* ================================================================== */
/*  Codestream Inspector (8D.4)                                       */
/* ================================================================== */

/** Recursively draw a tree node. */
static void draw_cs_tree_node(const GuiCSNode &node)
{
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
    if (node.children.empty())
        flags |= ImGuiTreeNodeFlags_Leaf;

    char label[128];
    snprintf(label, sizeof(label), "[0x%04X] %s  (offset %u)",
             node.marker, node.label.c_str(), node.offset);

    bool open = ImGui::TreeNodeEx(label, flags);
    if (!node.detail.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled(" — %s", node.detail.c_str());
    }

    if (open) {
        for (const auto &child : node.children) {
            draw_cs_tree_node(child);
        }
        ImGui::TreePop();
    }
}

void gui_roundtrip_draw_inspector(GuiRoundtripState *rs)
{
    if (!rs->show_inspector) return;

    ImGui::SetNextWindowSize(ImVec2(640, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Codestream Inspector", &rs->show_inspector,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    ImGui::SeparatorText("Input Codestream");
    ImGui::SetNextItemWidth(-110);
    ImGui::InputText("##cs_path", rs->cs_input_path,
                     sizeof(rs->cs_input_path));
    ImGui::SameLine();
    if (ImGui::Button("Parse", ImVec2(100, 0))) {
        /* Read file and parse */
        FILE *f = fopen(rs->cs_input_path, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long fsz = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (fsz > 0) {
                std::vector<uint8_t> buf((size_t)fsz);
                if (fread(buf.data(), 1, (size_t)fsz, f) == (size_t)fsz) {
                    parse_codestream(rs, buf.data(), buf.size());
                }
            }
            fclose(f);
        } else {
            rs->cs_nodes.clear();
            rs->cs_parsed = false;
        }
    }

    ImGui::Spacing();

    if (rs->cs_parsed && !rs->cs_nodes.empty()) {
        ImGui::SeparatorText("Codestream Structure");
        ImGui::BeginChild("CSTree", ImVec2(0, 0), ImGuiChildFlags_Border,
                          ImGuiWindowFlags_HorizontalScrollbar);
        for (const auto &node : rs->cs_nodes) {
            draw_cs_tree_node(node);
        }
        ImGui::EndChild();
    } else if (rs->cs_parsed) {
        ImGui::TextDisabled("No markers found (empty or invalid codestream).");
    } else {
        ImGui::TextDisabled("Enter a JP3D file path and click Parse.");
    }

    ImGui::End();
}
