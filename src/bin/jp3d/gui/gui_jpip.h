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
 * @file gui_jpip.h
 * @brief JPIP 3-D streaming client panel state for the OpenJP3D GUI
 *        application (Phase 8E).
 *
 * Provides:
 *  - GuiJpipState for JPIP connection, sub-volume browsing, and
 *    network diagnostics.
 *  - Connection dialog to a JPIP server by URL (8E.1).
 *  - Interactive sub-volume browsing via JPIP (8E.2).
 *  - Network diagnostics panel (8E.3).
 */

#ifndef OPJ_JP3D_GUI_JPIP_H
#define OPJ_JP3D_GUI_JPIP_H

#include <stdint.h>
#include <stdbool.h>
#include <atomic>
#include <thread>
#include <chrono>
#include <string>
#include <vector>

extern "C" {
#include "openjp3d.h"
#include "openjpip3d.h"
#include "opj_jpip3d_server.h"
#include "opj_jpip3d_client.h"
}

#include "gui_volume.h"

/* ------------------------------------------------------------------ */
/*  Dataset info (8E.1)                                                */
/* ------------------------------------------------------------------ */

/** Summary of a dataset available on the connected server. */
struct GuiJpipDatasetInfo {
    std::string name;       /**< Dataset identifier. */
    uint32_t    width;      /**< Width (samples). */
    uint32_t    height;     /**< Height (samples). */
    uint32_t    depth;      /**< Depth (slices). */
    uint32_t    numcomps;   /**< Number of components. */
};

/* ------------------------------------------------------------------ */
/*  Session statistics (8E.3)                                          */
/* ------------------------------------------------------------------ */

/** Cumulative network/session statistics. */
struct GuiJpipStats {
    uint64_t bytes_transferred; /**< Total response payload bytes. */
    uint32_t requests_sent;     /**< Total requests submitted. */
    uint32_t cache_hits;        /**< Requests satisfied from cache. */
    double   last_latency_ms;   /**< Last request-response latency (ms). */
    double   avg_latency_ms;    /**< Running average latency (ms). */
    double   total_latency_ms;  /**< Sum of all latencies for averaging. */
};

/* ------------------------------------------------------------------ */
/*  Aggregate state                                                    */
/* ------------------------------------------------------------------ */

/**
 * @brief All GUI state for Phase 8E JPIP streaming panels.
 */
struct GuiJpipState {
    /* ---- panel visibility ---- */
    bool show_connection;     /**< Connection dialog (8E.1). */
    bool show_browser;        /**< Sub-volume browser (8E.2). */
    bool show_diagnostics;    /**< Network diagnostics (8E.3). */

    /* ---- connection (8E.1) ---- */
    char conn_url[512];       /**< Server URL / JP3D file path. */
    bool connected;           /**< True when a session is active. */
    std::string conn_error;   /**< Last connection error message. */

    opj_jpip3d_server_t  *server;   /**< In-process JPIP server. */
    opj_jpip3d_session_t *session;  /**< Active client session. */

    std::vector<GuiJpipDatasetInfo> datasets; /**< Available datasets. */
    int  selected_dataset;    /**< Index of selected dataset (-1 = none). */

    /* ---- sub-volume browser (8E.2) ---- */
    int  browse_x0, browse_y0, browse_z0; /**< Region offset. */
    int  browse_w,  browse_h,  browse_d;  /**< Region size. */
    int  browse_res_level;    /**< Resolution level (0 = full). */
    int  browse_quality;      /**< Quality layers to request. */

    std::atomic<bool> fetch_running;  /**< Background fetch in progress. */
    std::atomic<bool> fetch_done;     /**< Background fetch completed. */
    std::thread       fetch_worker;   /**< Background fetch thread. */
    std::chrono::steady_clock::time_point fetch_start;

    opj_volume_t     *fetch_result;   /**< Decoded sub-volume from JPIP. */
    std::string       fetch_error;    /**< Fetch error message. */
    double            fetch_elapsed;  /**< Fetch wall-clock time (sec). */

    /* ---- network diagnostics (8E.3) ---- */
    GuiJpipStats      stats;          /**< Cumulative session statistics. */
};

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */

/**
 * @brief Initialise JPIP state to safe defaults.
 */
void gui_jpip_state_init(GuiJpipState *js);

/**
 * @brief Release resources (close session, destroy server, join workers).
 */
void gui_jpip_state_free(GuiJpipState *js);

/**
 * @brief Poll for background-fetch completion.  Call once per frame.
 */
void gui_jpip_tick(GuiJpipState *js);

/**
 * @brief Draw the JPIP Connection Dialog panel (8E.1).
 */
void gui_jpip_draw_connection(GuiJpipState *js);

/**
 * @brief Draw the Sub-Volume Browser panel (8E.2).
 */
void gui_jpip_draw_browser(GuiJpipState *js, GuiVolumeState *vol);

/**
 * @brief Draw the Network Diagnostics panel (8E.3).
 */
void gui_jpip_draw_diagnostics(GuiJpipState *js);

#endif /* OPJ_JP3D_GUI_JPIP_H */
