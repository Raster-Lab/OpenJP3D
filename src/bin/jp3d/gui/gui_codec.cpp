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
 * @file gui_codec.cpp
 * @brief Encoding, decoding, and transcoding panels and background-task
 *        management for Phase 8C of the OpenJP3D GUI.
 */

#include "gui_codec.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>

#include "imgui.h"

extern "C" {
#include "opj_raw_io.h"
}

/* ================================================================== */
/*  Lifecycle                                                         */
/* ================================================================== */

void gui_codec_state_init(GuiCodecState *cs)
{
    cs->show_encode    = false;
    cs->show_decode    = false;
    cs->show_transcode = false;

    /* Default encode parameters — match opj_jp3d_set_default_encoder_parameters */
    cs->enc_tile_w       = 0;
    cs->enc_tile_h       = 0;
    cs->enc_tile_d       = 0;
    cs->enc_resolutions  = 3;
    cs->enc_cblk_w       = 32;
    cs->enc_cblk_h       = 32;
    cs->enc_cblk_d       = 32;
    cs->enc_target_rate  = 0.0f;
    cs->enc_filter       = OPJ_JP3D_FILTER_53;
    cs->enc_htj2k        = false;
    cs->enc_threads      = 1;
    cs->enc_output_path[0] = '\0';

    /* Default decode parameters */
    cs->dec_input_path[0]  = '\0';
    cs->dec_sub_x0 = cs->dec_sub_y0 = cs->dec_sub_z0 = 0;
    cs->dec_sub_w  = cs->dec_sub_h  = cs->dec_sub_d  = 0;
    cs->dec_use_subvolume  = false;
    cs->dec_reduce_level   = 0;
    cs->dec_single_slice   = false;
    cs->dec_single_slice_idx = 0;

    /* Default transcode parameters */
    cs->tc_input_path[0]  = '\0';
    cs->tc_output_path[0] = '\0';
    cs->tc_target_mode    = 0;  /* HTJ2K */

    /* Task state */
    cs->active_op       = CODEC_OP_NONE;
    cs->task_running    = false;
    cs->task_done       = false;
    cs->task_cancel     = false;
    cs->task_success    = false;
    cs->task_elapsed_sec = 0.0;
    cs->task_status_msg.clear();
    cs->task_error_msg.clear();
    cs->decode_result   = NULL;
}

void gui_codec_state_free(GuiCodecState *cs)
{
    /* Signal cancel and wait for worker */
    cs->task_cancel = true;
    if (cs->worker.joinable())
        cs->worker.join();

    if (cs->decode_result) {
        opj_jp3d_destroy_volume(cs->decode_result);
        cs->decode_result = NULL;
    }
}

/* ================================================================== */
/*  Tick — poll for background task completion                        */
/* ================================================================== */

void gui_codec_tick(GuiCodecState *cs)
{
    if (!cs->task_running)
        return;

    /* Update elapsed time */
    auto now = std::chrono::steady_clock::now();
    cs->task_elapsed_sec =
        std::chrono::duration<double>(now - cs->task_start_time).count();

    if (cs->task_done) {
        /* Join the worker thread */
        if (cs->worker.joinable())
            cs->worker.join();
        cs->task_running = false;
    }
}

/* ================================================================== */
/*  Background task launchers                                         */
/* ================================================================== */

static void encode_worker(GuiCodecState *cs,
                          opj_volume_t *vol_copy,
                          opj_jp3d_enc_params_t enc,
                          std::string output_path)
{
    uint8_t *out_data = NULL;
    size_t   out_size = 0;

    opj_jp3d_bool_t ok = opj_jp3d_encode(
        vol_copy, &enc, &out_data, &out_size, NULL, NULL);

    if (!ok || !out_data) {
        cs->task_success = false;
        cs->task_error_msg = "Encoding failed.";
        cs->task_done = true;
        return;
    }

    /* Write to file */
    FILE *fp = fopen(output_path.c_str(), "wb");
    if (!fp) {
        opj_jp3d_free(out_data);
        cs->task_success = false;
        cs->task_error_msg = "Cannot open output file: " + output_path;
        cs->task_done = true;
        return;
    }

    size_t written = fwrite(out_data, 1, out_size, fp);
    fclose(fp);
    opj_jp3d_free(out_data);

    if (written != out_size) {
        cs->task_success = false;
        cs->task_error_msg = "Write error (incomplete write).";
    } else {
        cs->task_success = true;
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "Encoded %zu bytes to %s",
                 out_size, output_path.c_str());
        cs->task_status_msg = msg;
    }
    cs->task_done = true;
}

static void decode_worker(GuiCodecState *cs, std::string input_path)
{
    FILE *f = fopen(input_path.c_str(), "rb");
    if (!f) {
        cs->task_success = false;
        cs->task_error_msg = "Cannot open file: " + input_path;
        cs->task_done = true;
        return;
    }
    fseek(f, 0, SEEK_END);
    long fsz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsz <= 0) {
        fclose(f);
        cs->task_success = false;
        cs->task_error_msg = "File is empty.";
        cs->task_done = true;
        return;
    }

    std::vector<uint8_t> buf((size_t)fsz);
    if (fread(buf.data(), 1, (size_t)fsz, f) != (size_t)fsz) {
        fclose(f);
        cs->task_success = false;
        cs->task_error_msg = "Read error.";
        cs->task_done = true;
        return;
    }
    fclose(f);

    opj_jp3d_dec_params_t dec = {};
    dec.verbose = 0;
    opj_volume_t *vol = opj_jp3d_decode(buf.data(), buf.size(),
                                        &dec, NULL, NULL);
    if (!vol) {
        cs->task_success = false;
        cs->task_error_msg = "opj_jp3d_decode failed.";
        cs->task_done = true;
        return;
    }

    cs->decode_result = vol;
    cs->task_success  = true;
    char msg[256];
    snprintf(msg, sizeof(msg),
             "Decoded %ux%ux%u volume from %s",
             vol->comps[0].w, vol->comps[0].h, vol->comps[0].d,
             input_path.c_str());
    cs->task_status_msg = msg;
    cs->task_done = true;
}

static void transcode_worker(GuiCodecState *cs,
                              std::string input_path,
                              std::string output_path,
                              int target_mode)
{
    FILE *f = fopen(input_path.c_str(), "rb");
    if (!f) {
        cs->task_success = false;
        cs->task_error_msg = "Cannot open file: " + input_path;
        cs->task_done = true;
        return;
    }
    fseek(f, 0, SEEK_END);
    long fsz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsz <= 0) {
        fclose(f);
        cs->task_success = false;
        cs->task_error_msg = "File is empty.";
        cs->task_done = true;
        return;
    }

    std::vector<uint8_t> src((size_t)fsz);
    if (fread(src.data(), 1, (size_t)fsz, f) != (size_t)fsz) {
        fclose(f);
        cs->task_success = false;
        cs->task_error_msg = "Read error.";
        cs->task_done = true;
        return;
    }
    fclose(f);

    uint8_t *out_data = NULL;
    size_t   out_size = 0;
    opj_jp3d_bool_t ok;

    if (target_mode == 0) {
        /* Transcode to HTJ2K */
        ok = opj_jp3d_transcode_to_ht(
            src.data(), src.size(), NULL,
            &out_data, &out_size, NULL, NULL);
    } else {
        /* Transcode to EBCOT: decode then re-encode without HTJ2K */
        opj_jp3d_dec_params_t dec = {};
        dec.verbose = 0;
        opj_volume_t *vol = opj_jp3d_decode(src.data(), src.size(),
                                            &dec, NULL, NULL);
        if (!vol) {
            cs->task_success = false;
            cs->task_error_msg = "Decode failed during transcode.";
            cs->task_done = true;
            return;
        }
        opj_jp3d_enc_params_t enc;
        opj_jp3d_set_default_encoder_parameters(&enc);
        enc.use_htj2k = 0;
        ok = opj_jp3d_encode(vol, &enc, &out_data, &out_size, NULL, NULL);
        opj_jp3d_destroy_volume(vol);
    }

    if (!ok || !out_data) {
        cs->task_success = false;
        cs->task_error_msg = "Transcoding failed.";
        cs->task_done = true;
        return;
    }

    FILE *fp = fopen(output_path.c_str(), "wb");
    if (!fp) {
        opj_jp3d_free(out_data);
        cs->task_success = false;
        cs->task_error_msg = "Cannot open output file: " + output_path;
        cs->task_done = true;
        return;
    }

    size_t written = fwrite(out_data, 1, out_size, fp);
    fclose(fp);
    opj_jp3d_free(out_data);

    if (written != out_size) {
        cs->task_success = false;
        cs->task_error_msg = "Write error (incomplete write).";
    } else {
        cs->task_success = true;
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "Transcoded to %s (%zu bytes) → %s",
                 target_mode == 0 ? "HTJ2K" : "EBCOT",
                 out_size, output_path.c_str());
        cs->task_status_msg = msg;
    }
    cs->task_done = true;
}

/* ================================================================== */
/*  Helper: start a background task                                   */
/* ================================================================== */

static bool can_start_task(GuiCodecState *cs)
{
    return !cs->task_running;
}

static void begin_task(GuiCodecState *cs, GuiCodecOp op)
{
    cs->active_op       = op;
    cs->task_running    = true;
    cs->task_done       = false;
    cs->task_cancel     = false;
    cs->task_success    = false;
    cs->task_elapsed_sec = 0.0;
    cs->task_status_msg.clear();
    cs->task_error_msg.clear();
    cs->task_start_time = std::chrono::steady_clock::now();

    if (cs->decode_result) {
        opj_jp3d_destroy_volume(cs->decode_result);
        cs->decode_result = NULL;
    }
}

/* ================================================================== */
/*  Encode panel (8C.1)                                               */
/* ================================================================== */

void gui_codec_draw_encode_panel(GuiCodecState *cs, GuiVolumeState *vol)
{
    if (!cs->show_encode) return;

    ImGui::SetNextWindowSize(ImVec2(460, 480), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Encode", &cs->show_encode,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    bool busy = cs->task_running.load();
    bool has_vol = vol && vol->loaded;

    if (!has_vol) {
        ImGui::TextWrapped("Load a volume first (File > Open Volume) "
                           "before encoding.");
        ImGui::End();
        return;
    }

    ImGui::SeparatorText("Tile Size");
    ImGui::TextDisabled("(0 = whole volume, no tiling)");
    ImGui::PushItemWidth(100);
    ImGui::InputInt("Tile W", &cs->enc_tile_w);
    ImGui::SameLine();
    ImGui::InputInt("Tile H", &cs->enc_tile_h);
    ImGui::SameLine();
    ImGui::InputInt("Tile D", &cs->enc_tile_d);
    if (cs->enc_tile_w < 0) cs->enc_tile_w = 0;
    if (cs->enc_tile_h < 0) cs->enc_tile_h = 0;
    if (cs->enc_tile_d < 0) cs->enc_tile_d = 0;
    ImGui::PopItemWidth();

    ImGui::SeparatorText("DWT Decomposition Levels");
    ImGui::SliderInt("Levels", &cs->enc_resolutions, 0, 8);

    ImGui::SeparatorText("Code-Block Size");
    ImGui::PushItemWidth(100);
    ImGui::InputInt("CBlk W", &cs->enc_cblk_w);
    ImGui::SameLine();
    ImGui::InputInt("CBlk H", &cs->enc_cblk_h);
    ImGui::SameLine();
    ImGui::InputInt("CBlk D", &cs->enc_cblk_d);
    if (cs->enc_cblk_w < 4) cs->enc_cblk_w = 4;
    if (cs->enc_cblk_h < 4) cs->enc_cblk_h = 4;
    if (cs->enc_cblk_d < 4) cs->enc_cblk_d = 4;
    ImGui::PopItemWidth();

    ImGui::SeparatorText("Compression");
    const char *filter_items[] = { "5/3 Lossless", "9/7 Lossy" };
    ImGui::Combo("Filter", &cs->enc_filter, filter_items, 2);

    ImGui::SliderFloat("Target Rate",
                       &cs->enc_target_rate, 0.0f, 8.0f,
                       cs->enc_target_rate == 0.0f ? "Lossless" : "%.2f bps");
    ImGui::TextDisabled("(0 = lossless)");

    ImGui::Checkbox("HTJ2K (High-Throughput)", &cs->enc_htj2k);

    ImGui::SeparatorText("Threading");
    ImGui::SliderInt("Threads", &cs->enc_threads, 1, 16);

    ImGui::SeparatorText("Output");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##enc_out", cs->enc_output_path,
                     sizeof(cs->enc_output_path));
    ImGui::TextDisabled("Output JP3D file path (.jp3d)");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    bool can_encode = has_vol && !busy && cs->enc_output_path[0] != '\0';
    if (!can_encode) ImGui::BeginDisabled();
    if (ImGui::Button("Encode", ImVec2(120, 0))) {
        /* Build encoder params */
        opj_jp3d_enc_params_t enc;
        opj_jp3d_set_default_encoder_parameters(&enc);
        enc.tile_width        = (uint32_t)cs->enc_tile_w;
        enc.tile_height       = (uint32_t)cs->enc_tile_h;
        enc.tile_depth        = (uint32_t)cs->enc_tile_d;
        enc.num_resolutions_x = (uint32_t)cs->enc_resolutions;
        enc.num_resolutions_y = (uint32_t)cs->enc_resolutions;
        enc.num_resolutions_z = (uint32_t)cs->enc_resolutions;
        enc.cblk_width        = (uint32_t)cs->enc_cblk_w;
        enc.cblk_height       = (uint32_t)cs->enc_cblk_h;
        enc.cblk_depth        = (uint32_t)cs->enc_cblk_d;
        enc.filter            = (int32_t)cs->enc_filter;
        enc.target_rate       = cs->enc_target_rate;
        enc.use_htj2k         = cs->enc_htj2k ? OPJ_JP3D_USE_HTJ2K : 0;
        enc.verbose           = 0;

        std::string out_path(cs->enc_output_path);
        opj_volume_t *v = vol->vol;

        begin_task(cs, CODEC_OP_ENCODE);
        cs->worker = std::thread(encode_worker, cs, v, enc, out_path);
    }
    if (!can_encode) ImGui::EndDisabled();

    if (busy && cs->active_op == CODEC_OP_ENCODE) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Encoding...");
    }

    ImGui::End();
}

/* ================================================================== */
/*  Decode panel (8C.2)                                               */
/* ================================================================== */

void gui_codec_draw_decode_panel(GuiCodecState *cs, GuiVolumeState *vol)
{
    if (!cs->show_decode) return;

    ImGui::SetNextWindowSize(ImVec2(460, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Decode", &cs->show_decode,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    bool busy = cs->task_running.load();

    ImGui::SeparatorText("Input");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##dec_in", cs->dec_input_path,
                     sizeof(cs->dec_input_path));
    ImGui::TextDisabled("JP3D codestream path (.jp3d / .j3d)");

    ImGui::SeparatorText("Sub-Volume Extraction");
    ImGui::Checkbox("Extract sub-volume", &cs->dec_use_subvolume);
    if (cs->dec_use_subvolume) {
        ImGui::PushItemWidth(100);
        ImGui::InputInt("Offset X", &cs->dec_sub_x0);
        ImGui::SameLine();
        ImGui::InputInt("Offset Y", &cs->dec_sub_y0);
        ImGui::SameLine();
        ImGui::InputInt("Offset Z", &cs->dec_sub_z0);
        ImGui::InputInt("Width",  &cs->dec_sub_w);
        ImGui::SameLine();
        ImGui::InputInt("Height", &cs->dec_sub_h);
        ImGui::SameLine();
        ImGui::InputInt("Depth",  &cs->dec_sub_d);
        /* Clamp to non-negative */
        if (cs->dec_sub_x0 < 0) cs->dec_sub_x0 = 0;
        if (cs->dec_sub_y0 < 0) cs->dec_sub_y0 = 0;
        if (cs->dec_sub_z0 < 0) cs->dec_sub_z0 = 0;
        if (cs->dec_sub_w  < 1) cs->dec_sub_w  = 1;
        if (cs->dec_sub_h  < 1) cs->dec_sub_h  = 1;
        if (cs->dec_sub_d  < 1) cs->dec_sub_d  = 1;
        ImGui::PopItemWidth();
    }

    ImGui::SeparatorText("Resolution");
    ImGui::SliderInt("Reduce level", &cs->dec_reduce_level, 0, 5);
    ImGui::TextDisabled("(0 = full resolution)");

    ImGui::SeparatorText("Single-Slice Mode");
    ImGui::Checkbox("Decode single slice only", &cs->dec_single_slice);
    if (cs->dec_single_slice) {
        ImGui::InputInt("Slice index", &cs->dec_single_slice_idx);
        if (cs->dec_single_slice_idx < 0) cs->dec_single_slice_idx = 0;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    bool can_decode = !busy && cs->dec_input_path[0] != '\0';
    if (!can_decode) ImGui::BeginDisabled();
    if (ImGui::Button("Decode", ImVec2(120, 0))) {
        std::string in_path(cs->dec_input_path);
        begin_task(cs, CODEC_OP_DECODE);
        cs->worker = std::thread(decode_worker, cs, in_path);
    }
    if (!can_decode) ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::TextDisabled("Result auto-loads into viewer.");

    if (busy && cs->active_op == CODEC_OP_DECODE) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Decoding...");
    }

    ImGui::End();
}

/* ================================================================== */
/*  Transcode panel (8C.3)                                            */
/* ================================================================== */

void gui_codec_draw_transcode_panel(GuiCodecState *cs)
{
    if (!cs->show_transcode) return;

    ImGui::SetNextWindowSize(ImVec2(460, 280), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Transcode", &cs->show_transcode,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    bool busy = cs->task_running.load();

    ImGui::SeparatorText("Input Codestream");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##tc_in", cs->tc_input_path,
                     sizeof(cs->tc_input_path));
    ImGui::TextDisabled("JP3D codestream path (.jp3d / .j3d)");

    ImGui::SeparatorText("Target Mode");
    ImGui::RadioButton("HTJ2K (High-Throughput)", &cs->tc_target_mode, 0);
    ImGui::SameLine();
    ImGui::RadioButton("EBCOT (Standard)", &cs->tc_target_mode, 1);

    ImGui::SeparatorText("Output");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##tc_out", cs->tc_output_path,
                     sizeof(cs->tc_output_path));
    ImGui::TextDisabled("Output JP3D file path (.jp3d)");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    bool can_tc = !busy &&
                  cs->tc_input_path[0]  != '\0' &&
                  cs->tc_output_path[0] != '\0';
    if (!can_tc) ImGui::BeginDisabled();
    if (ImGui::Button("Transcode", ImVec2(120, 0))) {
        std::string in_path(cs->tc_input_path);
        std::string out_path(cs->tc_output_path);
        int mode = cs->tc_target_mode;
        begin_task(cs, CODEC_OP_TRANSCODE);
        cs->worker = std::thread(transcode_worker, cs,
                                  in_path, out_path, mode);
    }
    if (!can_tc) ImGui::EndDisabled();

    if (busy && cs->active_op == CODEC_OP_TRANSCODE) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
                           "Transcoding...");
    }

    ImGui::End();
}

/* ================================================================== */
/*  Progress overlay (8C.4)                                           */
/* ================================================================== */

void gui_codec_draw_progress(GuiCodecState *cs)
{
    bool busy = cs->task_running.load();
    bool done = cs->task_done.load();

    /* Show progress window when a task is running or just finished */
    if (!busy && cs->active_op == CODEC_OP_NONE)
        return;

    ImGui::SetNextWindowSize(ImVec2(400, 140), ImGuiCond_FirstUseEver);
    bool open = true;
    if (!ImGui::Begin("Progress", &open,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    const char *op_name = "Idle";
    switch (cs->active_op) {
    case CODEC_OP_ENCODE:    op_name = "Encoding";    break;
    case CODEC_OP_DECODE:    op_name = "Decoding";    break;
    case CODEC_OP_TRANSCODE: op_name = "Transcoding"; break;
    default: break;
    }

    if (busy && !done) {
        /* Indeterminate progress bar */
        float t = (float)fmod(cs->task_elapsed_sec * 0.5, 1.0);
        ImGui::ProgressBar(t, ImVec2(-1, 0), op_name);

        /* Elapsed time */
        int secs = (int)cs->task_elapsed_sec;
        int mins = secs / 60;
        secs %= 60;
        ImGui::Text("Elapsed: %d:%02d", mins, secs);

        /* Cancel button */
        ImGui::Spacing();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            cs->task_cancel = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(cancellation may not take effect immediately)");
    } else {
        /* Task finished */
        float elapsed = (float)cs->task_elapsed_sec;
        bool success = cs->task_success.load();

        if (success) {
            ImGui::ProgressBar(1.0f, ImVec2(-1, 0), "Complete");
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f),
                               "%s completed successfully.", op_name);
        } else {
            ImGui::ProgressBar(1.0f, ImVec2(-1, 0), "Failed");
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                               "%s failed.", op_name);
        }

        int secs = (int)elapsed;
        int mins = secs / 60;
        secs %= 60;
        ImGui::Text("Elapsed: %d:%02d", mins, secs);

        if (!cs->task_status_msg.empty()) {
            ImGui::TextWrapped("%s", cs->task_status_msg.c_str());
        }
        if (!cs->task_error_msg.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            ImGui::TextWrapped("Error: %s", cs->task_error_msg.c_str());
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();
        if (ImGui::Button("Dismiss", ImVec2(100, 0))) {
            cs->active_op = CODEC_OP_NONE;
        }
    }

    if (!open) {
        cs->active_op = CODEC_OP_NONE;
    }

    ImGui::End();
}
