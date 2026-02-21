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
 * @file gui_volume.h
 * @brief Volume state management and slice-viewer utilities for the
 *        OpenJP3D GUI application (Phase 8B).
 *
 * Provides:
 *  - A centralised GuiVolumeState holding the loaded opj_volume_t,
 *    viewer settings, cached OpenGL textures, and per-component
 *    statistics / histograms.
 *  - Helper functions for loading raw (.raw, .vol) and JP3D
 *    (.jp3d, .j3d) volumes.
 *  - Slice-extraction, window/level normalisation, and OpenGL
 *    texture update helpers.
 *  - Statistics computation (min, max, mean, std-dev, 256-bin histogram).
 */

#ifndef OPJ_JP3D_GUI_VOLUME_H
#define OPJ_JP3D_GUI_VOLUME_H

#include <stdint.h>
#include <stdbool.h>

#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
/* GL_GLEXT_PROTOTYPES makes glext.h expose function prototypes directly. */
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif
#include <GL/gl.h>
#include <GL/glext.h>
#endif

extern "C" {
#include "openjp3d.h"
#include "opj_raw_io.h"
}

/* ------------------------------------------------------------------ */
/*  Constants                                                         */
/* ------------------------------------------------------------------ */

/** Number of histogram bins. */
#define GUI_HIST_BINS 256

/** Slice-axis identifiers. */
enum GuiSliceAxis {
    SLICE_AXIS_Z = 0,  /**< Axial    — navigate along Z. */
    SLICE_AXIS_X = 1,  /**< Sagittal — navigate along X. */
    SLICE_AXIS_Y = 2   /**< Coronal  — navigate along Y. */
};

/* ------------------------------------------------------------------ */
/*  Parameters for opening a raw binary volume                        */
/* ------------------------------------------------------------------ */

/**
 * @brief User-supplied parameters needed to interpret a raw binary file.
 *
 * These are shown in the "Open Raw Volume" dialog when the file
 * extension is .raw or .vol.
 */
struct RawOpenParams {
    uint32_t width;      /**< Voxel grid width  (X). */
    uint32_t height;     /**< Voxel grid height (Y). */
    uint32_t depth;      /**< Voxel grid depth  (Z). */
    uint32_t prec;       /**< Bits per sample (8, 16, or 32). */
    int      sgnd;       /**< Non-zero = signed samples. */
    uint32_t numcomps;   /**< Number of components. */
};

/* ------------------------------------------------------------------ */
/*  Per-component statistics                                          */
/* ------------------------------------------------------------------ */

/** Statistics computed over all voxels of one component. */
struct GuiCompStats {
    float    min_val;              /**< Minimum sample value. */
    float    max_val;              /**< Maximum sample value. */
    float    mean;                 /**< Mean (arithmetic). */
    float    stddev;               /**< Standard deviation. */
    float    histogram[GUI_HIST_BINS]; /**< Normalised 256-bin histogram. */
    bool     computed;             /**< True once stats have been computed. */
};

/* ------------------------------------------------------------------ */
/*  Central volume state                                              */
/* ------------------------------------------------------------------ */

/**
 * @brief All runtime state associated with a loaded volume.
 *
 * One global instance is kept in opj_jp3d_gui.cpp.  Functions in
 * gui_volume.cpp operate on this struct.
 */
struct GuiVolumeState {
    /* ---- loaded data ---- */
    opj_volume_t *vol;         /**< Decoded/loaded volume (NULL = none). */
    bool          loaded;      /**< True when a volume is successfully loaded. */
    char          filepath[512]; /**< Path of the most recently opened file. */

    /* ---- codestream info (set when a JP3D file was opened) ---- */
    bool     is_jp3d;          /**< True = loaded from a JP3D codestream. */
    bool     htj2k_mode;       /**< True = HT block coder detected. */
    int      filter_type;      /**< OPJ_JP3D_FILTER_53 or _97. */
    uint32_t num_resolutions;  /**< DWT resolution levels (from codestream). */

    /* ---- slice viewer ---- */
    GuiSliceAxis slice_axis;   /**< Currently displayed axis. */
    int          slice_idx;    /**< Index along the current axis. */
    float        win_center;   /**< Window/level centre. */
    float        win_width;    /**< Window/level width (>0). */
    bool         auto_wl;      /**< If true, recompute W/L from min/max on next update. */

    /* ---- OpenGL texture for the 2-D slice ---- */
    GLuint tex_id;             /**< OpenGL texture name (0 = not allocated). */
    int    tex_w;              /**< Current texture width  (pixels). */
    int    tex_h;              /**< Current texture height (pixels). */
    bool   tex_dirty;          /**< Slice changed — upload texture next frame. */

    /* ---- statistics, one entry per component ---- */
    GuiCompStats *comp_stats;  /**< Array [numcomps], or NULL. */

    /* ---- 3-D ray-cast renderer ---- */
    GLuint vol_tex_id;         /**< 3-D texture for ray-casting (0 = not built). */
    bool   vol_tex_dirty;      /**< Volume changed — rebuild 3-D texture. */
    bool   show_3d;            /**< Toggle between slice and ray-cast view. */
    GLuint rc_fbo;             /**< Ray-cast framebuffer. */
    GLuint rc_color_tex;       /**< Ray-cast colour attachment. */
    GLuint rc_program;         /**< Ray-cast GLSL program (0 = not compiled). */
    int    rc_w;               /**< Ray-cast render width. */
    int    rc_h;               /**< Ray-cast render height. */
    float  rc_azimuth;         /**< Rotation azimuth  (degrees). */
    float  rc_elevation;       /**< Rotation elevation (degrees). */
    float  rc_density;         /**< Opacity density scale. */
};

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */

/**
 * @brief Initialise a GuiVolumeState to a safe, empty default.
 * @param s  State to initialise.
 */
void gui_volume_state_init(GuiVolumeState *s);

/**
 * @brief Release all resources owned by @p s (volume, textures, stats).
 *
 * After this call the state is equivalent to a freshly initialised one.
 * @param s  State to clear.
 */
void gui_volume_state_free(GuiVolumeState *s);

/**
 * @brief Load a raw binary volume from @p path using @p params.
 *
 * On success @p s->vol is set, @p s->loaded is true, and statistics
 * are marked dirty.  On failure a non-empty error string is returned.
 *
 * @param s       Volume state to populate.
 * @param path    Path to the .raw / .vol file.
 * @param params  Dimensions, bit-depth and component count.
 * @param errbuf  Buffer to receive an error message on failure.
 * @param errlen  Size of @p errbuf.
 * @return True on success.
 */
bool gui_volume_load_raw(GuiVolumeState *s,
                         const char *path,
                         const RawOpenParams *params,
                         char *errbuf, size_t errlen);

/**
 * @brief Load a JP3D codestream from @p path.
 *
 * Reads the file into memory and calls opj_jp3d_decode().
 *
 * @param s       Volume state to populate.
 * @param path    Path to the .jp3d / .j3d file.
 * @param errbuf  Buffer to receive an error message on failure.
 * @param errlen  Size of @p errbuf.
 * @return True on success.
 */
bool gui_volume_load_jp3d(GuiVolumeState *s,
                          const char *path,
                          char *errbuf, size_t errlen);

/**
 * @brief Compute per-component statistics and histograms.
 *
 * Populates @p s->comp_stats[c] for every component c.
 * Also sets @p s->win_center / @p s->win_width if @p s->auto_wl is true.
 *
 * @param s  Volume state with a loaded volume.
 */
void gui_volume_compute_stats(GuiVolumeState *s);

/**
 * @brief Build / rebuild the 2-D slice GL texture.
 *
 * Extracts the slice described by @p s->slice_axis and @p s->slice_idx,
 * applies window/level, and uploads an RGB8 texture to @p s->tex_id.
 * Clears @p s->tex_dirty on success.
 *
 * @param s  Volume state.
 */
void gui_volume_update_slice_texture(GuiVolumeState *s);

/**
 * @brief Build the 3-D volume texture used by the ray-cast renderer.
 *
 * Converts component 0 to a normalised single-channel 3-D texture.
 * Clears @p s->vol_tex_dirty on success.
 *
 * @param s  Volume state.
 */
void gui_volume_build_3d_texture(GuiVolumeState *s);

/**
 * @brief Compile (if needed) and run the ray-cast shader, writing to
 *        @p s->rc_fbo at @p width × @p height.
 *
 * The result can be displayed with ImGui::Image((void*)(intptr_t)s->rc_color_tex, ...).
 *
 * @param s       Volume state.
 * @param width   Desired render width  (pixels).
 * @param height  Desired render height (pixels).
 */
void gui_volume_render_raycast(GuiVolumeState *s, int width, int height);

/**
 * @brief Return the number of slices along the current axis.
 *
 * Returns 1 if no volume is loaded.
 *
 * @param s  Volume state.
 */
int gui_volume_axis_depth(const GuiVolumeState *s);

/**
 * @brief Clamp @p s->slice_idx to the valid range for the current axis.
 * @param s  Volume state.
 */
void gui_volume_clamp_slice(GuiVolumeState *s);

#endif /* OPJ_JP3D_GUI_VOLUME_H */
