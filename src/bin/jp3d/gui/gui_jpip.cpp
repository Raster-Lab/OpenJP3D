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
 * @file gui_jpip.cpp
 * @brief JPIP 3-D streaming client panels for Phase 8E of the OpenJP3D GUI.
 *
 * Implements:
 *  - Connection dialog (8E.1): connect to a JPIP server, list datasets.
 *  - Sub-volume browser (8E.2): interactive region request and display.
 *  - Network diagnostics (8E.3): session statistics display.
 */

#include "gui_jpip.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <string>
#include <vector>

#include "imgui.h"

/* ================================================================== */
/*  Lifecycle                                                         */
/* ================================================================== */

void gui_jpip_state_init(GuiJpipState *js)
{
    js->show_connection  = false;
    js->show_browser     = false;
    js->show_diagnostics = false;

    js->conn_url[0]      = '\0';
    js->connected        = false;
    js->conn_error.clear();

    js->server           = NULL;
    js->session          = NULL;

    js->datasets.clear();
    js->selected_dataset = -1;

    js->browse_x0 = 0;
    js->browse_y0 = 0;
    js->browse_z0 = 0;
    js->browse_w  = 0;
    js->browse_h  = 0;
    js->browse_d  = 0;
    js->browse_res_level = 0;
    js->browse_quality   = 1;

    js->fetch_running = false;
    js->fetch_done    = false;
    js->fetch_result  = NULL;
    js->fetch_error.clear();
    js->fetch_elapsed = 0.0;

    memset(&js->stats, 0, sizeof(js->stats));
}

void gui_jpip_state_free(GuiJpipState *js)
{
    /* Join background fetch thread if running */
    if (js->fetch_worker.joinable()) {
        js->fetch_running = false;
        js->fetch_worker.join();
    }

    /* Free fetched volume */
    if (js->fetch_result) {
        opj_jp3d_destroy_volume(js->fetch_result);
        js->fetch_result = NULL;
    }

    /* Close session and server */
    if (js->session) {
        opj_jpip3d_session_close(js->session);
        js->session = NULL;
    }
    if (js->server) {
        opj_jpip3d_server_destroy(js->server);
        js->server = NULL;
    }

    js->connected = false;
    js->datasets.clear();
    js->selected_dataset = -1;
}

/* ================================================================== */
/*  Tick (poll background tasks)                                      */
/* ================================================================== */

void gui_jpip_tick(GuiJpipState *js)
{
    if (js->fetch_done.load() && js->fetch_worker.joinable()) {
        js->fetch_worker.join();
        js->fetch_running = false;
        js->fetch_done    = false;

        auto now = std::chrono::steady_clock::now();
        js->fetch_elapsed =
            std::chrono::duration<double>(now - js->fetch_start).count();
    }
}

/* ================================================================== */
/*  Internal: connect / disconnect helpers                            */
/* ================================================================== */

static void jpip_disconnect(GuiJpipState *js)
{
    if (js->fetch_worker.joinable()) {
        js->fetch_running = false;
        js->fetch_worker.join();
    }
    if (js->fetch_result) {
        opj_jp3d_destroy_volume(js->fetch_result);
        js->fetch_result = NULL;
    }
    if (js->session) {
        opj_jpip3d_session_close(js->session);
        js->session = NULL;
    }
    if (js->server) {
        opj_jpip3d_server_destroy(js->server);
        js->server = NULL;
    }
    js->connected = false;
    js->datasets.clear();
    js->selected_dataset = -1;
    memset(&js->stats, 0, sizeof(js->stats));
}

static bool jpip_connect(GuiJpipState *js)
{
    jpip_disconnect(js);

    /* Create server and load the dataset from the file path */
    js->server = opj_jpip3d_server_create();
    if (!js->server) {
        js->conn_error = "Failed to create JPIP server.";
        return false;
    }

    /* Extract a dataset name from the file path */
    const char *slash = strrchr(js->conn_url, '/');
#ifdef _WIN32
    const char *bslash = strrchr(js->conn_url, '\\');
    if (bslash && (!slash || bslash > slash)) slash = bslash;
#endif
    const char *ds_name = slash ? slash + 1 : js->conn_url;

    if (!opj_jpip3d_server_load_dataset(js->server, ds_name, js->conn_url)) {
        js->conn_error = std::string("Failed to load dataset: ") + js->conn_url;
        opj_jpip3d_server_destroy(js->server);
        js->server = NULL;
        return false;
    }

    /* Open a client session */
    js->session = opj_jpip3d_session_open(js->server);
    if (!js->session) {
        js->conn_error = "Failed to open JPIP session.";
        opj_jpip3d_server_destroy(js->server);
        js->server = NULL;
        return false;
    }

    /* Enumerate available datasets */
    js->datasets.clear();
    for (size_t i = 0; i < js->server->num_datasets; ++i) {
        const opj_jpip3d_dataset_t *ds = &js->server->datasets[i];
        GuiJpipDatasetInfo info;
        info.name     = ds->name;
        info.width    = ds->width;
        info.height   = ds->height;
        info.depth    = ds->depth;
        info.numcomps = ds->numcomps;
        js->datasets.push_back(info);
    }

    if (!js->datasets.empty()) {
        js->selected_dataset = 0;
        /* Pre-populate browse region with full dataset extents */
        const GuiJpipDatasetInfo &ds = js->datasets[0];
        js->browse_x0 = 0;
        js->browse_y0 = 0;
        js->browse_z0 = 0;
        js->browse_w  = (int)ds.width;
        js->browse_h  = (int)ds.height;
        js->browse_d  = (int)ds.depth;
    }

    js->connected  = true;
    js->conn_error.clear();
    memset(&js->stats, 0, sizeof(js->stats));
    return true;
}

/* ================================================================== */
/*  8E.1 — Connection dialog                                          */
/* ================================================================== */

void gui_jpip_draw_connection(GuiJpipState *js)
{
    if (!js->show_connection) return;

    ImGui::SetNextWindowSize(ImVec2(560, 360), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("JPIP Connection", &js->show_connection,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    ImGui::TextWrapped(
        "Connect to a JPIP 3-D server by providing the path to a JP3D "
        "codestream file.  The file is loaded into an in-process JPIP "
        "server, and a client session is opened for interactive browsing.");

    ImGui::Separator();
    ImGui::Spacing();

    /* ---- Server URL / file path ---- */
    ImGui::Text("JP3D File Path:");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##jpip_url", js->conn_url, sizeof(js->conn_url));

    ImGui::Spacing();

    /* ---- Connect / Disconnect ---- */
    if (!js->connected) {
        bool can_connect = (js->conn_url[0] != '\0') &&
                           !js->fetch_running.load();
        if (!can_connect) ImGui::BeginDisabled();
        if (ImGui::Button("Connect", ImVec2(100, 0))) {
            if (jpip_connect(js)) {
                js->show_browser = true;
            }
        }
        if (!can_connect) ImGui::EndDisabled();
    } else {
        if (ImGui::Button("Disconnect", ImVec2(100, 0))) {
            jpip_disconnect(js);
        }
    }

    /* ---- Error display ---- */
    if (!js->conn_error.empty()) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        ImGui::TextWrapped("Error: %s", js->conn_error.c_str());
        ImGui::PopStyleColor();
    }

    /* ---- Session info ---- */
    if (js->connected && js->session) {
        ImGui::Spacing();
        ImGui::SeparatorText("Session Info");
        ImGui::Text("Session ID: %u", js->session->session_id);
        ImGui::Text("Datasets:   %u", (unsigned)js->datasets.size());

        /* ---- Dataset list ---- */
        if (!js->datasets.empty()) {
            ImGui::Spacing();
            ImGui::SeparatorText("Available Datasets");

            ImGui::BeginChild("DatasetList", ImVec2(0, 0), ImGuiChildFlags_Borders,
                              ImGuiWindowFlags_None);
            for (int i = 0; i < (int)js->datasets.size(); ++i) {
                const GuiJpipDatasetInfo &ds = js->datasets[i];
                char label[512];
                snprintf(label, sizeof(label),
                         "%s  (%ux%ux%u, %u comp)",
                         ds.name.c_str(), ds.width, ds.height, ds.depth,
                         ds.numcomps);

                bool selected = (js->selected_dataset == i);
                if (ImGui::Selectable(label, selected)) {
                    js->selected_dataset = i;
                    js->browse_x0 = 0;
                    js->browse_y0 = 0;
                    js->browse_z0 = 0;
                    js->browse_w  = (int)ds.width;
                    js->browse_h  = (int)ds.height;
                    js->browse_d  = (int)ds.depth;
                }
            }
            ImGui::EndChild();
        }
    }

    ImGui::End();
}

/* ================================================================== */
/*  8E.2 — Sub-volume browser                                         */
/* ================================================================== */

void gui_jpip_draw_browser(GuiJpipState *js, GuiVolumeState *vol)
{
    if (!js->show_browser) return;

    ImGui::SetNextWindowSize(ImVec2(480, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("JPIP Sub-Volume Browser", &js->show_browser,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    if (!js->connected) {
        ImGui::TextWrapped("Not connected.  Use the JPIP Connection dialog "
                           "to connect first.");
        ImGui::End();
        return;
    }

    if (js->selected_dataset < 0 ||
        js->selected_dataset >= (int)js->datasets.size()) {
        ImGui::TextDisabled("No dataset selected.");
        ImGui::End();
        return;
    }

    const GuiJpipDatasetInfo &ds = js->datasets[js->selected_dataset];

    ImGui::Text("Dataset: %s  (%ux%ux%u)", ds.name.c_str(),
                ds.width, ds.height, ds.depth);
    ImGui::Separator();
    ImGui::Spacing();

    /* ---- Region controls ---- */
    ImGui::SeparatorText("Region of Interest");
    ImGui::PushItemWidth(120);
    ImGui::InputInt("X Offset", &js->browse_x0);
    ImGui::SameLine();
    ImGui::InputInt("Width##bw", &js->browse_w);

    ImGui::InputInt("Y Offset", &js->browse_y0);
    ImGui::SameLine();
    ImGui::InputInt("Height##bh", &js->browse_h);

    ImGui::InputInt("Z Offset", &js->browse_z0);
    ImGui::SameLine();
    ImGui::InputInt("Depth##bd", &js->browse_d);

    ImGui::Spacing();
    ImGui::InputInt("Resolution Level", &js->browse_res_level);
    ImGui::InputInt("Quality Layers", &js->browse_quality);
    ImGui::PopItemWidth();

    /* Clamp values */
    if (js->browse_x0 < 0) js->browse_x0 = 0;
    if (js->browse_y0 < 0) js->browse_y0 = 0;
    if (js->browse_z0 < 0) js->browse_z0 = 0;
    if (js->browse_w  < 1) js->browse_w  = 1;
    if (js->browse_h  < 1) js->browse_h  = 1;
    if (js->browse_d  < 1) js->browse_d  = 1;
    if (js->browse_res_level < 0) js->browse_res_level = 0;
    if (js->browse_quality   < 1) js->browse_quality   = 1;

    /* Clamp to dataset extents */
    if ((uint32_t)(js->browse_x0 + js->browse_w) > ds.width)
        js->browse_w = (int)ds.width - js->browse_x0;
    if ((uint32_t)(js->browse_y0 + js->browse_h) > ds.height)
        js->browse_h = (int)ds.height - js->browse_y0;
    if ((uint32_t)(js->browse_z0 + js->browse_d) > ds.depth)
        js->browse_d = (int)ds.depth - js->browse_z0;

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    /* ---- Fetch button ---- */
    bool fetching = js->fetch_running.load();
    if (fetching) ImGui::BeginDisabled();
    if (ImGui::Button("Fetch Sub-Volume", ImVec2(160, 0))) {
        /* Free previous result */
        if (js->fetch_result) {
            opj_jp3d_destroy_volume(js->fetch_result);
            js->fetch_result = NULL;
        }
        js->fetch_error.clear();

        js->fetch_running = true;
        js->fetch_done    = false;
        js->fetch_start   = std::chrono::steady_clock::now();

        /* Capture parameters for the worker thread */
        int x0 = js->browse_x0, y0 = js->browse_y0, z0 = js->browse_z0;
        int w = js->browse_w, h = js->browse_h, d = js->browse_d;
        int res = js->browse_res_level;
        int ql  = js->browse_quality;
        int sel = js->selected_dataset;

        opj_jpip3d_session_t *sess = js->session;
        const char *ds_name_cstr = js->datasets[sel].name.c_str();
        std::string ds_name_copy(ds_name_cstr);
        uint32_t dsw = ds.width, dsh = ds.height, dsd = ds.depth;

        /* Statistics tracking pointers */
        GuiJpipStats *stats = &js->stats;

        js->fetch_worker = std::thread(
            [js, sess, ds_name_copy, x0, y0, z0, w, h, d,
             dsw, dsh, dsd, res, ql, stats]()
        {
            opj_jpip3d_request_t req;
            memset(&req, 0, sizeof(req));
            strncpy(req.dataset, ds_name_copy.c_str(),
                    OPJ_JPIP3D_DATASET_NAME_MAX - 1);
            req.dataset[OPJ_JPIP3D_DATASET_NAME_MAX - 1] = '\0';
            req.fsiz3d[0] = dsw;
            req.fsiz3d[1] = dsh;
            req.fsiz3d[2] = dsd;
            req.roff3d[0] = (uint32_t)x0;
            req.roff3d[1] = (uint32_t)y0;
            req.roff3d[2] = (uint32_t)z0;
            req.rsiz3d[0] = (uint32_t)w;
            req.rsiz3d[1] = (uint32_t)h;
            req.rsiz3d[2] = (uint32_t)d;
            req.resolution_level = (uint32_t)res;
            req.quality_layers   = (uint32_t)ql;
            req.stream_type      = OPJ_JPIP3D_STREAM_JP3D;

            auto t0 = std::chrono::steady_clock::now();

            opj_jpip3d_response_t resp;
            memset(&resp, 0, sizeof(resp));

            int ok = opj_jpip3d_session_request_region(sess, &req, &resp);

            auto t1 = std::chrono::steady_clock::now();
            double lat =
                std::chrono::duration<double, std::milli>(t1 - t0).count();

            /* Update statistics */
            stats->requests_sent++;
            stats->last_latency_ms = lat;
            stats->total_latency_ms += lat;
            stats->avg_latency_ms =
                stats->total_latency_ms / stats->requests_sent;

            if (!ok) {
                js->fetch_error = "JPIP request failed.";
                js->fetch_done = true;
                return;
            }

            if (resp.size == 0 || resp.data == NULL) {
                /* Region already cached */
                stats->cache_hits++;
                js->fetch_error = "Region already cached (no new data).";
                opj_jp3d_free(resp.data);
                js->fetch_done = true;
                return;
            }

            stats->bytes_transferred += resp.size;

            /* Decode the response */
            opj_volume_t *vol_result =
                opj_jp3d_decode(resp.data, resp.size, NULL, NULL, NULL);
            opj_jp3d_free(resp.data);

            if (!vol_result) {
                js->fetch_error = "Failed to decode JPIP response.";
                js->fetch_done = true;
                return;
            }

            js->fetch_result = vol_result;
            js->fetch_done = true;
        });
    }
    if (fetching) ImGui::EndDisabled();

    /* ---- Progress indicator ---- */
    if (fetching) {
        ImGui::SameLine();
        auto now = std::chrono::steady_clock::now();
        double elapsed =
            std::chrono::duration<double>(now - js->fetch_start).count();
        ImGui::Text("Fetching... %.1f s", elapsed);
    }

    /* ---- Fetch result ---- */
    if (js->fetch_result && !js->fetch_running.load()) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
        ImGui::Text("Sub-volume received (%ux%ux%u) in %.2f s",
                    js->fetch_result->comps[0].w,
                    js->fetch_result->comps[0].h,
                    js->fetch_result->comps[0].d,
                    js->fetch_elapsed);
        ImGui::PopStyleColor();

        if (ImGui::Button("Load into Viewer", ImVec2(160, 0))) {
            /* Transfer ownership to the viewer */
            gui_volume_state_free(vol);
            vol->vol       = js->fetch_result;
            vol->loaded    = true;
            vol->is_jp3d   = true;
            vol->htj2k_mode      = false;
            vol->filter_type     = OPJ_JP3D_FILTER_53;
            vol->num_resolutions = 0;
            vol->slice_axis      = SLICE_AXIS_Z;
            vol->slice_idx       = 0;
            vol->tex_dirty       = true;
            vol->vol_tex_dirty   = true;
            vol->auto_wl         = true;
            snprintf(vol->filepath, sizeof(vol->filepath), "jpip://%s",
                     js->datasets[js->selected_dataset].name.c_str());
            gui_volume_compute_stats(vol);
            js->fetch_result = NULL;
        }
    }

    /* ---- Fetch error ---- */
    if (!js->fetch_error.empty() && !js->fetch_running.load()) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
        ImGui::TextWrapped("%s", js->fetch_error.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::End();
}

/* ================================================================== */
/*  8E.3 — Network diagnostics                                       */
/* ================================================================== */

void gui_jpip_draw_diagnostics(GuiJpipState *js)
{
    if (!js->show_diagnostics) return;

    ImGui::SetNextWindowSize(ImVec2(400, 280), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("JPIP Network Diagnostics", &js->show_diagnostics,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    if (!js->connected) {
        ImGui::TextDisabled("Not connected to a JPIP server.");
        ImGui::End();
        return;
    }

    ImGui::SeparatorText("Session Statistics");

    ImGui::Text("Session ID:       %u",
                js->session ? js->session->session_id : 0);
    ImGui::Text("Active Datasets:  %u", (unsigned)js->datasets.size());

    ImGui::Spacing();
    ImGui::SeparatorText("Transfer Statistics");

    /* Bytes transferred with human-readable formatting */
    uint64_t bytes = js->stats.bytes_transferred;
    if (bytes < 1024) {
        ImGui::Text("Bytes Transferred: %u B", (unsigned)bytes);
    } else if (bytes < 1024 * 1024) {
        ImGui::Text("Bytes Transferred: %.1f KB", (double)bytes / 1024.0);
    } else {
        ImGui::Text("Bytes Transferred: %.2f MB",
                    (double)bytes / (1024.0 * 1024.0));
    }

    ImGui::Text("Requests Sent:     %u", js->stats.requests_sent);
    ImGui::Text("Cache Hits:        %u", js->stats.cache_hits);

    /* Cache hit ratio */
    if (js->stats.requests_sent > 0) {
        float ratio = (float)js->stats.cache_hits /
                      (float)js->stats.requests_sent * 100.0f;
        ImGui::Text("Cache Hit Ratio:   %.1f%%", (double)ratio);
    } else {
        ImGui::Text("Cache Hit Ratio:   —");
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Latency");

    ImGui::Text("Last Latency:      %.2f ms", js->stats.last_latency_ms);
    ImGui::Text("Average Latency:   %.2f ms", js->stats.avg_latency_ms);

    ImGui::Spacing();

    if (ImGui::Button("Reset Statistics", ImVec2(140, 0))) {
        memset(&js->stats, 0, sizeof(js->stats));
    }

    ImGui::End();
}
