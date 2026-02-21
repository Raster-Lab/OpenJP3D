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
 * @file gui_roundtrip.h
 * @brief Round-trip testing, diff viewer, batch test runner, and codestream
 *        inspector panel state for the OpenJP3D GUI application (Phase 8D).
 *
 * Provides:
 *  - GuiRoundtripState for one-click encode→decode→compare (8D.1).
 *  - Diff viewer comparing original vs decoded volumes (8D.2).
 *  - Batch test runner with CSV export (8D.3).
 *  - Codestream inspector tree-view (8D.4).
 */

#ifndef OPJ_JP3D_GUI_ROUNDTRIP_H
#define OPJ_JP3D_GUI_ROUNDTRIP_H

#include <stdint.h>
#include <stdbool.h>
#include <atomic>
#include <thread>
#include <chrono>
#include <string>
#include <vector>

extern "C" {
#include "openjp3d.h"
}

#include "gui_volume.h"

/* ------------------------------------------------------------------ */
/*  Round-trip result (8D.1)                                          */
/* ------------------------------------------------------------------ */

/** Holds quality metrics computed after a round-trip. */
struct GuiRTMetrics {
    bool   lossless;       /**< True if volumes are bit-exact. */
    double psnr;           /**< Peak Signal-to-Noise Ratio (dB). */
    double mse;            /**< Mean Squared Error. */
    double max_abs_err;    /**< Maximum absolute error across all voxels. */
    size_t compressed_size;/**< Compressed codestream size in bytes. */
    double compression_ratio; /**< Original size / compressed size. */
    double encode_sec;     /**< Encode wall-clock time (seconds). */
    double decode_sec;     /**< Decode wall-clock time (seconds). */
};

/* ------------------------------------------------------------------ */
/*  Diff viewer data (8D.2)                                           */
/* ------------------------------------------------------------------ */

/** Holds the difference volume used by the diff viewer. */
struct GuiDiffData {
    int32_t *diff;         /**< Signed per-voxel difference (orig − decoded). */
    uint32_t w, h, d;      /**< Dimensions matching the compared volumes. */
    float    max_abs;       /**< Max |diff| for normalisation. */
    bool     valid;         /**< True when diff data is populated. */
};

/* ------------------------------------------------------------------ */
/*  Batch test entry (8D.3)                                           */
/* ------------------------------------------------------------------ */

/** A single row in the batch test results table. */
struct GuiBatchEntry {
    /* Parameters */
    int   filter;          /**< OPJ_JP3D_FILTER_53 or _97. */
    float target_rate;     /**< 0 = lossless. */
    bool  htj2k;           /**< HTJ2K mode. */
    int   resolutions;     /**< DWT levels. */
    int   tile_w, tile_h, tile_d; /**< Tile size (0 = whole volume). */

    /* Results */
    bool   ran;            /**< True once this entry has been executed. */
    bool   pass;           /**< True if round-trip succeeded. */
    double psnr;           /**< PSNR (dB) or INFINITY for lossless. */
    double mse;            /**< MSE. */
    double compression_ratio; /**< Ratio. */
    double encode_sec;     /**< Encode time. */
    double decode_sec;     /**< Decode time. */
    std::string error_msg; /**< Error message if failed. */
};

/* ------------------------------------------------------------------ */
/*  Codestream inspector entry (8D.4)                                 */
/* ------------------------------------------------------------------ */

/** A single node in the codestream inspector tree. */
struct GuiCSNode {
    std::string label;     /**< Display text (e.g. "SIZ3D"). */
    std::string detail;    /**< Detail text (e.g. "256x256x64, 1 comp"). */
    uint32_t    offset;    /**< Byte offset of the marker in the stream. */
    uint16_t    marker;    /**< Marker code (e.g. 0xFF51). */
    std::vector<GuiCSNode> children; /**< Child nodes (e.g. components). */
};

/* ------------------------------------------------------------------ */
/*  Aggregate state                                                   */
/* ------------------------------------------------------------------ */

/**
 * @brief All GUI state for Phase 8D panels.
 */
struct GuiRoundtripState {
    /* ---- panel visibility ---- */
    bool show_roundtrip;     /**< Round-trip wizard (8D.1). */
    bool show_diff;          /**< Diff viewer (8D.2). */
    bool show_batch;         /**< Batch test runner (8D.3). */
    bool show_inspector;     /**< Codestream inspector (8D.4). */

    /* ---- round-trip wizard (8D.1) ---- */
    int   rt_filter;         /**< Filter selection. */
    float rt_target_rate;    /**< Target rate (0 = lossless). */
    bool  rt_htj2k;          /**< HTJ2K toggle. */
    int   rt_resolutions;    /**< DWT levels. */

    std::atomic<bool> rt_running;
    std::atomic<bool> rt_done;
    std::thread       rt_worker;
    std::chrono::steady_clock::time_point rt_start_time;
    double            rt_elapsed_sec;
    bool              rt_has_result;
    GuiRTMetrics      rt_metrics;
    std::string       rt_error;

    /** Decoded round-trip volume (for diff viewer). */
    opj_volume_t     *rt_decoded;

    /* ---- diff viewer (8D.2) ---- */
    GuiDiffData       diff;
    int               diff_threshold;    /**< Visibility threshold. */
    int               diff_slice_idx;    /**< Slice index in diff viewer. */
    GuiSliceAxis      diff_axis;         /**< Axis for diff viewer. */
    GLuint            diff_tex_orig;     /**< GL texture for original slice. */
    GLuint            diff_tex_dec;      /**< GL texture for decoded slice. */
    GLuint            diff_tex_err;      /**< GL texture for error map. */
    int               diff_tex_w;        /**< Texture width. */
    int               diff_tex_h;        /**< Texture height. */

    /* ---- batch test runner (8D.3) ---- */
    std::vector<GuiBatchEntry> batch_entries;
    std::atomic<bool>          batch_running;
    std::atomic<bool>          batch_done;
    std::thread                batch_worker;
    int                        batch_current;  /**< Index being processed. */
    int                        batch_total;     /**< Total entries. */

    /* ---- codestream inspector (8D.4) ---- */
    std::vector<GuiCSNode>     cs_nodes;
    char                       cs_input_path[512];
    bool                       cs_parsed;
};

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */

/**
 * @brief Initialise roundtrip state to safe defaults.
 */
void gui_roundtrip_state_init(GuiRoundtripState *rs);

/**
 * @brief Release resources (join workers, free textures/data).
 */
void gui_roundtrip_state_free(GuiRoundtripState *rs);

/**
 * @brief Poll for background-task completion.  Call once per frame.
 */
void gui_roundtrip_tick(GuiRoundtripState *rs);

/**
 * @brief Draw the Round-Trip Test Wizard panel (8D.1).
 */
void gui_roundtrip_draw_wizard(GuiRoundtripState *rs, GuiVolumeState *vol);

/**
 * @brief Draw the Diff Viewer panel (8D.2).
 */
void gui_roundtrip_draw_diff(GuiRoundtripState *rs, GuiVolumeState *vol);

/**
 * @brief Draw the Batch Test Runner panel (8D.3).
 */
void gui_roundtrip_draw_batch(GuiRoundtripState *rs, GuiVolumeState *vol);

/**
 * @brief Draw the Codestream Inspector panel (8D.4).
 */
void gui_roundtrip_draw_inspector(GuiRoundtripState *rs);

#endif /* OPJ_JP3D_GUI_ROUNDTRIP_H */
