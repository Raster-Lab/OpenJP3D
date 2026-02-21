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
 * @file gui_codec.h
 * @brief Encoding, decoding, and transcoding panel state for the
 *        OpenJP3D GUI application (Phase 8C).
 *
 * Provides:
 *  - GuiCodecState holding encode/decode/transcode parameters, progress
 *    tracking, and background-task state.
 *  - Functions to initialise, tick (poll completion), and draw the
 *    Encode, Decode, Transcode, and Progress panels.
 */

#ifndef OPJ_JP3D_GUI_CODEC_H
#define OPJ_JP3D_GUI_CODEC_H

#include <stdint.h>
#include <stdbool.h>
#include <atomic>
#include <thread>
#include <chrono>
#include <string>

extern "C" {
#include "openjp3d.h"
}

#include "gui_volume.h"

/* ------------------------------------------------------------------ */
/*  Background task                                                    */
/* ------------------------------------------------------------------ */

/** Identifies the currently running (or most recently finished) operation. */
enum GuiCodecOp {
    CODEC_OP_NONE      = 0,
    CODEC_OP_ENCODE    = 1,
    CODEC_OP_DECODE    = 2,
    CODEC_OP_TRANSCODE = 3
};

/* ------------------------------------------------------------------ */
/*  Codec panel state                                                  */
/* ------------------------------------------------------------------ */

/**
 * @brief All GUI state for encoding, decoding, and transcoding panels.
 */
struct GuiCodecState {
    /* ---- panel visibility ---- */
    bool show_encode;
    bool show_decode;
    bool show_transcode;

    /* ---- encode parameters (8C.1) ---- */
    int  enc_tile_w;
    int  enc_tile_h;
    int  enc_tile_d;
    int  enc_resolutions;
    int  enc_cblk_w;
    int  enc_cblk_h;
    int  enc_cblk_d;
    float enc_target_rate;
    int  enc_filter;          /* 0 = 5/3 lossless, 1 = 9/7 lossy */
    bool enc_htj2k;
    int  enc_threads;
    char enc_output_path[512];

    /* ---- decode parameters (8C.2) ---- */
    char dec_input_path[512];
    int  dec_sub_x0, dec_sub_y0, dec_sub_z0;
    int  dec_sub_w,  dec_sub_h,  dec_sub_d;
    bool dec_use_subvolume;
    int  dec_reduce_level;
    bool dec_single_slice;
    int  dec_single_slice_idx;

    /* ---- transcode parameters (8C.3) ---- */
    char tc_input_path[512];
    char tc_output_path[512];
    int  tc_target_mode;   /* 0 = HTJ2K, 1 = EBCOT */

    /* ---- progress & cancellation (8C.4) ---- */
    GuiCodecOp         active_op;
    std::atomic<bool>  task_running;
    std::atomic<bool>  task_done;
    std::atomic<bool>  task_cancel;
    std::atomic<bool>  task_success;
    std::thread        worker;
    std::chrono::steady_clock::time_point task_start_time;
    double             task_elapsed_sec;
    std::string        task_status_msg;
    std::string        task_error_msg;

    /* ---- decode result (loaded into viewer on completion) ---- */
    opj_volume_t      *decode_result;
};

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */

/**
 * @brief Initialise codec state to safe defaults.
 */
void gui_codec_state_init(GuiCodecState *cs);

/**
 * @brief Release resources (join worker thread if running).
 */
void gui_codec_state_free(GuiCodecState *cs);

/**
 * @brief Poll for background-task completion.
 *
 * Call once per frame from the main loop.  When a task finishes, sets
 * status messages and (for decode) populates @p cs->decode_result.
 */
void gui_codec_tick(GuiCodecState *cs);

/**
 * @brief Draw the Encode panel (8C.1).
 */
void gui_codec_draw_encode_panel(GuiCodecState *cs, GuiVolumeState *vol);

/**
 * @brief Draw the Decode panel (8C.2).
 */
void gui_codec_draw_decode_panel(GuiCodecState *cs, GuiVolumeState *vol);

/**
 * @brief Draw the Transcode panel (8C.3).
 */
void gui_codec_draw_transcode_panel(GuiCodecState *cs);

/**
 * @brief Draw the Progress overlay (8C.4).
 */
void gui_codec_draw_progress(GuiCodecState *cs);

#endif /* OPJ_JP3D_GUI_CODEC_H */
