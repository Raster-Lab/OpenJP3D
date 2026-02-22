/*
 * Copyright (c) 2024-2026, OpenJP3D Contributors
 * All rights reserved.
 * SPDX-License-Identifier: BSD-2-Clause
 */

/**
 * @file opj_jp3d_gui.cpp
 * @brief OpenJP3D interactive GUI test application.
 *
 * Main entry point for the Dear ImGui + SDL2 + OpenGL3 GUI application.
 * Provides a dockable window layout with:
 *   - Menu bar (File, View, Tools, Help)
 *   - Toolbar (common actions)
 *   - File browser / open panel          (Phase 8B.1)
 *   - Volume info + statistics panel     (Phase 8B.4 / 8B.5)
 *   - Slice / volume viewport            (Phase 8B.2 / 8B.3)
 *   - Encode / decode / transcode panels (Phase 8C.1–8C.3)
 *   - Progress overlay with cancellation (Phase 8C.4)
 *   - Log / console panel
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <vector>
#include <string>
#include <algorithm>

/* Platform directory listing */
#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include <SDL.h>
#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <SDL_opengl.h>
#endif

/* Link against the OpenJP3D C library */
extern "C" {
#include "openjp3d.h"
}

#include "gui_theme.h"
#include "gui_volume.h"
#include "gui_codec.h"
#include "gui_roundtrip.h"
#include "gui_jpip.h"
#include "gui_log_prefs.h"

/* ================================================================== */
/*  Log panel (Phase 8F.1) — thin wrapper around GuiLogState          */
/* ================================================================== */

static GuiLogState g_log;

static void gui_log(GuiLogSeverity sev, const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    gui_log_add(&g_log, sev, "%s", buf);
}

/* ================================================================== */
/*  Global state                                                      */
/* ================================================================== */

static bool g_show_file_browser   = true;
static bool g_show_volume_info    = true;
static bool g_show_viewport       = true;
static bool g_show_about          = false;
static bool g_running             = true;

/* Central volume state (Phase 8B) */
static GuiVolumeState g_vol;

/* Codec panel state (Phase 8C) */
static GuiCodecState g_codec;

/* Round-trip / diff / batch / inspector state (Phase 8D) */
static GuiRoundtripState g_rt;

/* JPIP streaming client state (Phase 8E) */
static GuiJpipState g_jpip;

/* Preferences and keyboard shortcuts (Phase 8F) */
static GuiPrefsState     g_prefs;
static GuiShortcutsState g_shortcuts;

/* ================================================================== */
/*  Open-volume dialog state (8B.1)                                   */
/* ================================================================== */

static bool  g_open_dlg_visible    = false;
static char  g_open_dlg_path[512]  = "";
static char  g_open_dlg_prev_path[512] = "";   /* tracks path changes */
static bool  g_open_raw_params_dlg = false;
static RawOpenParams g_raw_params  = {64, 64, 64, 8, 0, 1};
static char  g_open_dlg_error[256] = "";
static bool  g_raw_autodetected    = false;     /* true when params came from auto-detect */
static int   g_raw_preset_idx      = 0;         /* selected preset index (0 = Custom) */

/* ---- Preset volume dimension profiles ---- */
struct RawPreset {
    const char *label;
    uint32_t w, h, d, prec, numcomps;
    int sgnd;
};

static const RawPreset RAW_PRESETS[] = {
    { "Custom",                      0,   0,   0,  0, 0, 0 },
    { "32\xc2\xb3  8-bit (tiny)",   32,  32,  32,  8, 1, 0 },
    { "64\xc2\xb3  8-bit",          64,  64,  64,  8, 1, 0 },
    { "64\xc2\xb3 16-bit",          64,  64,  64, 16, 1, 0 },
    { "64x64x32 16-bit",            64,  64,  32, 16, 1, 0 },
    { "128\xc2\xb3  8-bit",        128, 128, 128,  8, 1, 0 },
    { "128x128x64  8-bit",         128, 128,  64,  8, 1, 0 },
    { "256\xc2\xb3  8-bit (CT)",   256, 256, 256,  8, 1, 0 },
    { "256\xc2\xb3 16-bit (CT)",   256, 256, 256, 16, 1, 0 },
    { "512x512x128 16-bit (MRI)",  512, 512, 128, 16, 1, 0 },
    { "512x512x256 16-bit (CT)",   512, 512, 256, 16, 1, 0 },
    { "512x512x512 16-bit",        512, 512, 512, 16, 1, 0 },
    { "64\xc2\xb3  8-bit RGB (3c)", 64,  64,  64,  8, 3, 0 },
    { "256\xc2\xb3  8-bit RGB",    256, 256, 256,  8, 3, 0 },
};
static const int NUM_RAW_PRESETS = (int)(sizeof(RAW_PRESETS) / sizeof(RAW_PRESETS[0]));

/**
 * @brief Extract the base filename (without directory) from a path.
 */
static const char *basename_of(const char *path)
{
    const char *slash = strrchr(path, '/');
    const char *bslash = strrchr(path, '\\');
    const char *last = slash > bslash ? slash : bslash;
    return last ? last + 1 : path;
}

/**
 * @brief Try to auto-detect raw volume parameters from the filename.
 *
 * Recognises patterns like:
 *   name_WxHxD_Nbit.raw     (e.g. sphere_64x64x64_8bit.raw)
 *   name_WxHxD_Nbit_Mc.raw  (e.g. rgb_64x64x64_8bit_3c.raw)
 *   name_WxHxD.raw          (e.g. volume_128x128x64.raw — assumes 8-bit)
 *
 * Also checks for "xNbit" or "Nbit" to infer precision,
 * and "Nc" or "Ncomp" to infer component count.
 *
 * @return true if at least WxHxD were successfully parsed.
 */
static bool autodetect_raw_params_from_filename(const char *path,
                                                RawOpenParams *out)
{
    const char *name = basename_of(path);
    if (!name || !name[0]) return false;

    /* Work on a mutable lowercase copy */
    char buf[512];
    strncpy(buf, name, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    for (char *p = buf; *p; ++p) {
        if (*p >= 'A' && *p <= 'Z') *p += 32;
    }

    /* ---- Parse WxHxD ---- */
    /* Look for a pattern like "64x64x64" or "128x128x64" in the filename */
    bool found_dims = false;
    uint32_t w = 0, h = 0, d = 0;
    const char *s = buf;
    while (*s) {
        /* Find a digit that starts a potential WxHxD pattern */
        if (*s >= '0' && *s <= '9') {
            char *end1 = NULL;
            unsigned long v1 = strtoul(s, &end1, 10);
            if (end1 && (*end1 == 'x' || *end1 == 'X')) {
                char *end2 = NULL;
                unsigned long v2 = strtoul(end1 + 1, &end2, 10);
                if (end2 && (*end2 == 'x' || *end2 == 'X')) {
                    char *end3 = NULL;
                    unsigned long v3 = strtoul(end2 + 1, &end3, 10);
                    if (end3 > end2 + 1 && v1 > 0 && v2 > 0 && v3 > 0) {
                        w = (uint32_t)v1;
                        h = (uint32_t)v2;
                        d = (uint32_t)v3;
                        found_dims = true;
                        s = end3;
                        continue;
                    }
                }
            }
        }
        ++s;
    }

    if (!found_dims) return false;

    out->width  = w;
    out->height = h;
    out->depth  = d;

    /* ---- Parse bit depth (e.g. "8bit", "16bit", "32bit") ---- */
    const char *bp = strstr(buf, "bit");
    if (bp && bp > buf) {
        /* Walk backwards from "bit" to find the number */
        const char *numend = bp;
        const char *numstart = bp - 1;
        while (numstart > buf && *(numstart - 1) >= '0' && *(numstart - 1) <= '9')
            --numstart;
        if (numstart < numend) {
            unsigned long prec = strtoul(numstart, NULL, 10);
            if (prec == 8 || prec == 12 || prec == 16 || prec == 32)
                out->prec = (uint32_t)prec;
        }
    }

    /* ---- Parse component count (e.g. "3c", "3comp", "rgb") ---- */
    if (strstr(buf, "rgb")) {
        out->numcomps = 3;
    } else if (strstr(buf, "rgba")) {
        out->numcomps = 4;
    } else {
        /* Look for Nc pattern (e.g. "_3c") */
        const char *cp = buf;
        while (*cp) {
            if (*cp >= '1' && *cp <= '9') {
                char *ce = NULL;
                unsigned long nc = strtoul(cp, &ce, 10);
                if (ce && (*ce == 'c' || strncmp(ce, "comp", 4) == 0)) {
                    out->numcomps = (uint32_t)nc;
                    break;
                }
            }
            ++cp;
        }
    }

    /* ---- Signed detection ---- */
    if (strstr(buf, "signed") || strstr(buf, "_s_") || strstr(buf, "_sgnd"))
        out->sgnd = 1;

    return true;
}

/**
 * @brief Try to guess raw volume dimensions from the file size.
 *
 * Assumes 1 component, unsigned.  Tries 8-bit and 16-bit precision.
 * Prefers cubic dimensions (N³) that exactly match the file size.
 * Falls back to common medical aspect ratios (e.g. 512×512×N).
 *
 * @return true if a plausible match was found.
 */
static bool autodetect_raw_params_from_filesize(const char *path,
                                                RawOpenParams *out)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long fsz = ftell(f);
    fclose(f);
    if (fsz <= 0) return false;

    size_t file_bytes = (size_t)fsz;

    /* Try common precisions: 8-bit, then 16-bit */
    for (uint32_t prec : {8u, 16u}) {
        uint32_t bps = (prec <= 8) ? 1 : (prec <= 16) ? 2 : 4;

        /* Try to find a perfect cube */
        for (uint32_t n = 2; n <= 1024; ++n) {
            size_t vol = (size_t)n * n * n * bps;
            if (vol == file_bytes) {
                out->width  = n;
                out->height = n;
                out->depth  = n;
                out->prec   = prec;
                out->numcomps = 1;
                out->sgnd   = 0;
                return true;
            }
            if (vol > file_bytes) break;
        }

        /* Try common medical sizes: 512×512×D, 256×256×D, 128×128×D */
        static const uint32_t common_wh[] = {512, 256, 128, 64};
        for (uint32_t wh : common_wh) {
            size_t slice_bytes = (size_t)wh * wh * bps;
            if (slice_bytes > 0 && file_bytes % slice_bytes == 0) {
                uint32_t d = (uint32_t)(file_bytes / slice_bytes);
                if (d >= 1 && d <= 4096) {
                    out->width  = wh;
                    out->height = wh;
                    out->depth  = d;
                    out->prec   = prec;
                    out->numcomps = 1;
                    out->sgnd   = 0;
                    return true;
                }
            }
        }
    }

    return false;
}

/**
 * @brief Run auto-detection: first try filename, then file size heuristic.
 */
static void autodetect_raw_params(const char *path)
{
    /* Start with current defaults so partial detection keeps them */
    RawOpenParams detected = g_raw_params;
    bool ok = autodetect_raw_params_from_filename(path, &detected);
    if (!ok) {
        ok = autodetect_raw_params_from_filesize(path, &detected);
    }
    if (ok) {
        /* Apply if precision was not set by filename, default to 8 */
        if (detected.prec == 0) detected.prec = 8;
        if (detected.numcomps == 0) detected.numcomps = 1;
        g_raw_params     = detected;
        g_raw_autodetected = true;
        g_raw_preset_idx = 0; /* Custom */
    } else {
        g_raw_autodetected = false;
    }
}

/* Detect if a path looks like a JP3D codestream */
static bool path_is_jp3d(const char *p)
{
    size_t len = strlen(p);
    if (len < 4) return false;
    const char *ext = p + len - 4;
    return (strcmp(ext, ".j3d") == 0 || strcmp(ext + 1, "jp3d") == 0);
}

/* Detect raw/vol extension */
static bool path_is_raw(const char *p)
{
    size_t len = strlen(p);
    if (len < 4) return false;
    const char *ext = p + len - 4;
    return (strcmp(ext, ".raw") == 0 || strcmp(ext, ".vol") == 0);
}

/* ================================================================== */
/*  Panel drawing helpers                                             */
/* ================================================================== */

/* ---- open-volume dialog (8B.1) ---- */
static void open_volume(const char *path)
{
    char errbuf[256] = "";
    bool ok = false;

    if (path_is_jp3d(path)) {
        ok = gui_volume_load_jp3d(&g_vol, path, errbuf, sizeof(errbuf));
    } else if (path_is_raw(path)) {
        ok = gui_volume_load_raw(&g_vol, path, &g_raw_params,
                                 errbuf, sizeof(errbuf));
    } else {
        /* Try JP3D first, fall back to raw */
        ok = gui_volume_load_jp3d(&g_vol, path, errbuf, sizeof(errbuf));
        if (!ok) {
            ok = gui_volume_load_raw(&g_vol, path, &g_raw_params,
                                     errbuf, sizeof(errbuf));
        }
    }

    if (ok) {
        gui_log(GUI_LOG_INFO, "Loaded: %s  (%ux%ux%u, %u comp, %u bit%s)",
                path,
                g_vol.vol->comps[0].w,
                g_vol.vol->comps[0].h,
                g_vol.vol->comps[0].d,
                g_vol.vol->numcomps,
                g_vol.vol->comps[0].prec,
                g_vol.vol->comps[0].sgnd ? " signed" : "");
        g_open_dlg_error[0] = '\0';
    } else {
        gui_log(GUI_LOG_ERROR, "Failed to load '%s': %s", path, errbuf);
        strncpy(g_open_dlg_error, errbuf, sizeof(g_open_dlg_error) - 1);
    }
}

static void draw_open_dialog(void)
{
    if (!g_open_dlg_visible) return;

    ImGui::SetNextWindowSize(ImVec2(560, 340), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Open Volume", &g_open_dlg_visible,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    ImGui::TextWrapped("Enter the path to a JP3D codestream (.jp3d/.j3d) "
                       "or a raw binary volume (.raw/.vol).");
    ImGui::Separator();

    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##path", g_open_dlg_path, sizeof(g_open_dlg_path))) {
        /* Path changed — trigger auto-detection for raw files */
        if (strcmp(g_open_dlg_path, g_open_dlg_prev_path) != 0) {
            strncpy(g_open_dlg_prev_path, g_open_dlg_path, sizeof(g_open_dlg_prev_path) - 1);
            if (path_is_raw(g_open_dlg_path)) {
                autodetect_raw_params(g_open_dlg_path);
            }
        }
    }

    ImGui::Spacing();
    bool is_raw = path_is_raw(g_open_dlg_path);

    if (is_raw) {
        /* ---- Auto-detect status ---- */
        if (g_raw_autodetected) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
            ImGui::TextWrapped("\xe2\x9c\x93 Dimensions auto-detected from filename");
            ImGui::PopStyleColor();
        } else {
            ImGui::TextDisabled("Dimensions not detected — select a preset or set manually:");
        }

        /* ---- Preset dropdown ---- */
        ImGui::PushItemWidth(280);
        if (ImGui::BeginCombo("Preset", RAW_PRESETS[g_raw_preset_idx].label)) {
            for (int i = 0; i < NUM_RAW_PRESETS; ++i) {
                bool selected = (i == g_raw_preset_idx);
                if (ImGui::Selectable(RAW_PRESETS[i].label, selected)) {
                    g_raw_preset_idx = i;
                    if (i > 0) {
                        /* Apply preset values */
                        g_raw_params.width    = RAW_PRESETS[i].w;
                        g_raw_params.height   = RAW_PRESETS[i].h;
                        g_raw_params.depth    = RAW_PRESETS[i].d;
                        g_raw_params.prec     = RAW_PRESETS[i].prec;
                        g_raw_params.numcomps = RAW_PRESETS[i].numcomps;
                        g_raw_params.sgnd     = RAW_PRESETS[i].sgnd;
                        g_raw_autodetected    = false;
                    }
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();

        ImGui::SameLine();
        if (ImGui::Button("Auto-detect")) {
            autodetect_raw_params(g_open_dlg_path);
            if (!g_raw_autodetected) {
                gui_log(GUI_LOG_WARNING,
                        "Could not auto-detect dimensions for '%s'",
                        g_open_dlg_path);
            }
        }

        /* ---- Dimension fields ---- */
        ImGui::Spacing();
        ImGui::PushItemWidth(120);
        ImGui::InputInt("Width",     (int *)&g_raw_params.width);
        ImGui::SameLine();
        ImGui::InputInt("Height",    (int *)&g_raw_params.height);
        ImGui::SameLine();
        ImGui::InputInt("Depth",     (int *)&g_raw_params.depth);
        ImGui::InputInt("Precision", (int *)&g_raw_params.prec);
        ImGui::SameLine();
        ImGui::InputInt("Components",(int *)&g_raw_params.numcomps);
        ImGui::SameLine();
        bool sgnd = (g_raw_params.sgnd != 0);
        if (ImGui::Checkbox("Signed", &sgnd))
            g_raw_params.sgnd = sgnd ? 1 : 0;
        ImGui::PopItemWidth();

        /* Reset preset to Custom when user manually edits fields */
        if (g_raw_preset_idx > 0) {
            const RawPreset &pr = RAW_PRESETS[g_raw_preset_idx];
            if (g_raw_params.width != pr.w || g_raw_params.height != pr.h ||
                g_raw_params.depth != pr.d || g_raw_params.prec != pr.prec ||
                g_raw_params.numcomps != pr.numcomps || g_raw_params.sgnd != pr.sgnd) {
                g_raw_preset_idx = 0;
            }
        }
        /* Clamp to valid ranges */
        if (g_raw_params.width     < 1) g_raw_params.width     = 1;
        if (g_raw_params.height    < 1) g_raw_params.height    = 1;
        if (g_raw_params.depth     < 1) g_raw_params.depth     = 1;
        if (g_raw_params.prec      < 1) g_raw_params.prec      = 1;
        if (g_raw_params.prec     > 32) g_raw_params.prec      = 32;
        if (g_raw_params.numcomps < 1)  g_raw_params.numcomps  = 1;

        /* ---- File size vs expected size feedback ---- */
        if (g_open_dlg_path[0]) {
            uint32_t bps = (g_raw_params.prec <= 8) ? 1 :
                           (g_raw_params.prec <= 16) ? 2 : 4;
            size_t expected = (size_t)g_raw_params.width *
                              g_raw_params.height *
                              g_raw_params.depth *
                              g_raw_params.numcomps * bps;
            FILE *fcheck = fopen(g_open_dlg_path, "rb");
            if (fcheck) {
                fseek(fcheck, 0, SEEK_END);
                long actual = ftell(fcheck);
                fclose(fcheck);
                if (actual > 0) {
                    ImGui::Spacing();
                    if ((size_t)actual == expected) {
                        ImGui::PushStyleColor(ImGuiCol_Text,
                            ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
                        ImGui::Text("\xe2\x9c\x93 File size matches: %zu bytes",
                                    expected);
                        ImGui::PopStyleColor();
                    } else {
                        ImGui::PushStyleColor(ImGuiCol_Text,
                            ImVec4(1.0f, 0.6f, 0.2f, 1.0f));
                        ImGui::Text("\xe2\x9a\xa0 Size mismatch: file=%ld, "
                                    "expected=%zu bytes",
                                    actual, expected);
                        ImGui::PopStyleColor();
                    }
                }
            }
        }
    }

    ImGui::Spacing();
    if (ImGui::Button("Load", ImVec2(80, 0))) {
        open_volume(g_open_dlg_path);
        if (!g_open_dlg_error[0])
            g_open_dlg_visible = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(80, 0)))
        g_open_dlg_visible = false;

    if (g_open_dlg_error[0]) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        ImGui::TextWrapped("Error: %s", g_open_dlg_error);
        ImGui::PopStyleColor();
    }

    ImGui::End();
}

/* ---- menu bar ---- */
static void draw_menu_bar(void)
{
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open Volume...", "Ctrl+O")) {
                g_open_dlg_visible   = true;
                g_open_dlg_error[0]  = '\0';
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Quit", "Ctrl+Q")) {
                g_running = false;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("File Browser",  NULL, &g_show_file_browser);
            ImGui::MenuItem("Volume Info",   NULL, &g_show_volume_info);
            ImGui::MenuItem("Viewport",      NULL, &g_show_viewport);
            ImGui::MenuItem("Log Console",   NULL, &g_log.show_log);
            ImGui::Separator();
            if (ImGui::MenuItem("Toggle Theme", "Ctrl+T")) {
                enum opj_gui_theme t = opj_gui_toggle_theme();
                g_prefs.theme = (t == OPJ_GUI_THEME_DARK) ? 0 : 1;
                gui_log(GUI_LOG_INFO, "Theme switched to %s",
                        t == OPJ_GUI_THEME_DARK ? "Dark" : "Light");
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Tools")) {
            if (ImGui::MenuItem("Encode...", NULL, false,
                                !g_codec.task_running.load())) {
                g_codec.show_encode = true;
            }
            if (ImGui::MenuItem("Decode...", NULL, false,
                                !g_codec.task_running.load())) {
                g_codec.show_decode = true;
            }
            if (ImGui::MenuItem("Transcode...", NULL, false,
                                !g_codec.task_running.load())) {
                g_codec.show_transcode = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Round-Trip Test...", NULL, false,
                                !g_rt.rt_running.load())) {
                g_rt.show_roundtrip = true;
            }
            if (ImGui::MenuItem("Diff Viewer...", NULL, false,
                                g_rt.diff.valid)) {
                g_rt.show_diff = true;
            }
            if (ImGui::MenuItem("Batch Test Runner...", NULL, false,
                                !g_rt.batch_running.load())) {
                g_rt.show_batch = true;
            }
            if (ImGui::MenuItem("Codestream Inspector...")) {
                g_rt.show_inspector = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("JPIP Connection...", NULL, false,
                                !g_jpip.fetch_running.load())) {
                g_jpip.show_connection = true;
            }
            if (ImGui::MenuItem("JPIP Browser...", NULL, false,
                                g_jpip.connected)) {
                g_jpip.show_browser = true;
            }
            if (ImGui::MenuItem("JPIP Diagnostics...", NULL, false,
                                g_jpip.connected)) {
                g_jpip.show_diagnostics = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Preferences...")) {
                g_prefs.show_prefs = true;
            }
            if (ImGui::MenuItem("Keyboard Shortcuts...")) {
                g_shortcuts.show_shortcuts = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About OpenJP3D GUI")) {
                g_show_about = true;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

/* ---- toolbar ---- */
static void draw_toolbar(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));

    ImGuiWindowFlags toolbar_flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings;

    float toolbar_h = ImGui::GetFrameHeight() + 8.0f;
    ImGuiViewport *vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y));
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, toolbar_h));

    if (ImGui::Begin("##Toolbar", NULL, toolbar_flags)) {
        if (ImGui::Button("Open")) {
            g_open_dlg_visible  = true;
            g_open_dlg_error[0] = '\0';
        }
        ImGui::SameLine();
        if (ImGui::Button("Encode")) {
            g_codec.show_encode = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Decode")) {
            g_codec.show_decode = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Transcode")) {
            g_codec.show_transcode = true;
        }
        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();
        if (ImGui::Button("Round-Trip")) {
            g_rt.show_roundtrip = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Batch")) {
            g_rt.show_batch = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Inspector")) {
            g_rt.show_inspector = true;
        }
        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();
        if (ImGui::Button("JPIP")) {
            g_jpip.show_connection = true;
        }
        ImGui::SameLine();

        /* Right-aligned theme toggle */
        float theme_btn_w = ImGui::CalcTextSize("Theme: Dark ").x +
                            ImGui::GetStyle().FramePadding.x * 2;
        ImGui::SameLine(ImGui::GetWindowWidth() - theme_btn_w - 8.0f);
        const char *theme_label =
            (opj_gui_get_current_theme() == OPJ_GUI_THEME_DARK) ?
            "Theme: Dark" : "Theme: Light";
        if (ImGui::Button(theme_label)) {
            enum opj_gui_theme t = opj_gui_toggle_theme();
            g_prefs.theme = (t == OPJ_GUI_THEME_DARK) ? 0 : 1;
            gui_log(GUI_LOG_INFO, "Theme switched to %s",
                    t == OPJ_GUI_THEME_DARK ? "Dark" : "Light");
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

/* ---- file browser / open panel (8B.1) ---- */

/* ---- Directory listing state ---- */
struct DirEntry {
    char name[256];
    bool is_dir;
    long size;       /* file size in bytes, -1 for dirs */
};

static char           g_browse_dir[512]  = "";
static bool           g_browse_needs_scan = true;
static std::vector<DirEntry> g_browse_entries;
static char           g_browse_filter[64] = "";  /* filename filter */

/** Determine the test_data/ directory relative to the executable. */
static void find_test_data_dir(char *out, size_t out_sz)
{
    /* Try common locations relative to the build/install tree */
    const char *candidates[] = {
        "test_data",
        "../test_data",
        "../../test_data",
        "../../../test_data",
        "../../../../test_data",
    };
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        FILE *probe = fopen((std::string(candidates[i]) + "/README_TEST_DATA.txt").c_str(), "r");
        if (probe) {
            fclose(probe);
            /* Resolve to a usable path */
            strncpy(out, candidates[i], out_sz - 1);
            out[out_sz - 1] = '\0';
            return;
        }
    }
    /* Fallback: use current directory */
    strncpy(out, ".", out_sz - 1);
    out[out_sz - 1] = '\0';
}

/** Check if filename has a supported volume extension. */
static bool is_volume_file(const char *name)
{
    size_t len = strlen(name);
    if (len < 4) return false;
    const char *ext = name + len - 4;
    if (strcmp(ext, ".raw") == 0) return true;
    if (strcmp(ext, ".vol") == 0) return true;
    if (strcmp(ext, ".j3d") == 0) return true;
    if (len >= 5 && strcmp(name + len - 5, ".jp3d") == 0) return true;
    return false;
}

/** Scan a directory and populate g_browse_entries. */
static void scan_directory(const char *dir)
{
    g_browse_entries.clear();

    /* Use SDL's filesystem or platform API to list directory.
       For portability, we use a simple approach with dirent. */
#if defined(_WIN32)
    std::string pattern = std::string(dir) + "\\*";
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;
    do {
        if (strcmp(fd.cFileName, ".") == 0) continue;
        DirEntry e;
        strncpy(e.name, fd.cFileName, sizeof(e.name) - 1);
        e.name[sizeof(e.name) - 1] = '\0';
        e.is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        e.size = e.is_dir ? -1 : (long)((uint64_t)fd.nFileSizeHigh << 32 | fd.nFileSizeLow);
        g_browse_entries.push_back(e);
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
#else
    /* POSIX */
    DIR *dp = opendir(dir);
    if (!dp) return;
    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        if (strcmp(de->d_name, ".") == 0) continue;
        DirEntry e;
        strncpy(e.name, de->d_name, sizeof(e.name) - 1);
        e.name[sizeof(e.name) - 1] = '\0';
        e.is_dir = (de->d_type == DT_DIR);
        e.size = -1;
        if (!e.is_dir) {
            std::string full = std::string(dir) + "/" + de->d_name;
            struct stat st;
            if (stat(full.c_str(), &st) == 0)
                e.size = (long)st.st_size;
        }
        g_browse_entries.push_back(e);
    }
    closedir(dp);
#endif

    /* Sort: directories first (with ".." at top), then files alphabetically */
    std::sort(g_browse_entries.begin(), g_browse_entries.end(),
        [](const DirEntry &a, const DirEntry &b) {
            /* ".." always first */
            if (strcmp(a.name, "..") == 0) return true;
            if (strcmp(b.name, "..") == 0) return false;
            if (a.is_dir != b.is_dir) return a.is_dir;
            return strcmp(a.name, b.name) < 0;
        });

    g_browse_needs_scan = false;
}

/** Format a file size for display. */
static const char *format_size(long bytes, char *buf, size_t buf_sz)
{
    if (bytes < 0) {
        snprintf(buf, buf_sz, "---");
    } else if (bytes < 1024) {
        snprintf(buf, buf_sz, "%ld B", bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(buf, buf_sz, "%.1f KB", bytes / 1024.0);
    } else {
        snprintf(buf, buf_sz, "%.1f MB", bytes / (1024.0 * 1024.0));
    }
    return buf;
}

static void draw_file_browser(void)
{
    if (!g_show_file_browser) return;
    if (!ImGui::Begin("File Browser", &g_show_file_browser)) {
        ImGui::End();
        return;
    }

    /* ---- Currently loaded volume status ---- */
    if (g_vol.loaded) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.9f, 0.3f, 1.0f));
        const char *slash = strrchr(g_vol.filepath, '/');
#ifdef _WIN32
        const char *bslash = strrchr(g_vol.filepath, '\\');
        if (bslash && (!slash || bslash > slash)) slash = bslash;
#endif
        ImGui::Text("\xe2\x9c\x93 %s", slash ? slash + 1 : g_vol.filepath);
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::SmallButton("Close")) {
            gui_volume_state_free(&g_vol);
            gui_log(GUI_LOG_INFO, "Volume closed.");
        }
        ImGui::Text("  %ux%ux%u, %u-bit, %u comp%s",
                     g_vol.vol->comps[0].w, g_vol.vol->comps[0].h,
                     g_vol.vol->comps[0].d, g_vol.vol->comps[0].prec,
                     g_vol.vol->numcomps,
                     g_vol.vol->numcomps > 1 ? "s" : "");
        ImGui::Separator();
    }

    /* ---- Tab bar: Test Data | Browse ---- */
    if (ImGui::BeginTabBar("BrowserTabs")) {

        /* ============================================================ */
        /*  TAB 1: Test Data — quick-access to generated test volumes   */
        /* ============================================================ */
        if (ImGui::BeginTabItem("Test Data")) {
            static char test_data_dir[512] = "";
            static bool test_data_scanned = false;
            static std::vector<DirEntry> test_files;

            /* Find test_data directory on first access */
            if (!test_data_scanned) {
                find_test_data_dir(test_data_dir, sizeof(test_data_dir));
                /* Scan it */
                test_files.clear();
#ifndef _WIN32
                DIR *dp = opendir(test_data_dir);
                if (dp) {
                    struct dirent *de;
                    while ((de = readdir(dp)) != NULL) {
                        if (de->d_name[0] == '.') continue;
                        if (!is_volume_file(de->d_name)) continue;
                        DirEntry e;
                        strncpy(e.name, de->d_name, sizeof(e.name) - 1);
                        e.name[sizeof(e.name) - 1] = '\0';
                        e.is_dir = false;
                        e.size = -1;
                        std::string full = std::string(test_data_dir) + "/" + de->d_name;
                        struct stat st;
                        if (stat(full.c_str(), &st) == 0)
                            e.size = (long)st.st_size;
                        test_files.push_back(e);
                    }
                    closedir(dp);
                }
#endif
                std::sort(test_files.begin(), test_files.end(),
                    [](const DirEntry &a, const DirEntry &b) {
                        return strcmp(a.name, b.name) < 0;
                    });
                test_data_scanned = true;
            }

            if (test_files.empty()) {
                ImGui::TextWrapped("No test data found. Run:\n"
                    "  generate_test_data test_data/\n"
                    "from the build directory to create test volumes.");
                if (ImGui::Button("Rescan")) {
                    test_data_scanned = false;
                }
            } else {
                ImGui::TextDisabled("Click a file to load it instantly:");
                ImGui::Spacing();

                /* Section: Raw Volumes */
                if (ImGui::CollapsingHeader("Raw Volumes (.raw)",
                        ImGuiTreeNodeFlags_DefaultOpen)) {
                    for (size_t i = 0; i < test_files.size(); ++i) {
                        const DirEntry &f = test_files[i];
                        size_t nlen = strlen(f.name);
                        bool is_raw_file = (nlen >= 4 &&
                            (strcmp(f.name + nlen - 4, ".raw") == 0 ||
                             strcmp(f.name + nlen - 4, ".vol") == 0));
                        if (!is_raw_file) continue;

                        char sz_buf[32];
                        format_size(f.size, sz_buf, sizeof(sz_buf));

                        /* Auto-detect info for display */
                        RawOpenParams info = {0, 0, 0, 8, 0, 1};
                        std::string full_path = std::string(test_data_dir) + "/" + f.name;
                        bool detected = autodetect_raw_params_from_filename(f.name, &info);

                        ImGui::PushID((int)i);
                        char label[384];
                        if (detected) {
                            snprintf(label, sizeof(label),
                                "%s  [%ux%ux%u, %u-bit, %uc]  %s",
                                f.name, info.width, info.height, info.depth,
                                info.prec ? info.prec : 8,
                                info.numcomps ? info.numcomps : 1,
                                sz_buf);
                        } else {
                            snprintf(label, sizeof(label), "%s  %s",
                                     f.name, sz_buf);
                        }

                        if (ImGui::Selectable(label)) {
                            /* Auto-detect and load in one click */
                            strncpy(g_open_dlg_path, full_path.c_str(),
                                    sizeof(g_open_dlg_path) - 1);
                            if (detected) {
                                if (info.prec == 0) info.prec = 8;
                                if (info.numcomps == 0) info.numcomps = 1;
                                g_raw_params = info;
                                g_raw_autodetected = true;
                            } else {
                                autodetect_raw_params(full_path.c_str());
                            }
                            open_volume(full_path.c_str());
                            gui_log(GUI_LOG_INFO,
                                "Test data: loaded %s", f.name);
                        }
                        ImGui::PopID();
                    }
                }

                /* Section: JP3D Codestreams */
                if (ImGui::CollapsingHeader("JP3D Codestreams (.jp3d)",
                        ImGuiTreeNodeFlags_DefaultOpen)) {
                    for (size_t i = 0; i < test_files.size(); ++i) {
                        const DirEntry &f = test_files[i];
                        size_t nlen = strlen(f.name);
                        bool is_jp3d_file = (nlen >= 5 &&
                            strcmp(f.name + nlen - 5, ".jp3d") == 0) ||
                            (nlen >= 4 &&
                            strcmp(f.name + nlen - 4, ".j3d") == 0);
                        if (!is_jp3d_file) continue;

                        char sz_buf[32];
                        format_size(f.size, sz_buf, sizeof(sz_buf));

                        /* Derive description from filename */
                        const char *desc = "";
                        if (strstr(f.name, "lossless")) desc = "lossless 5/3";
                        else if (strstr(f.name, "lossy"))  desc = "lossy 9/7";

                        ImGui::PushID(1000 + (int)i);
                        char label[384];
                        if (desc[0]) {
                            snprintf(label, sizeof(label),
                                "%s  [%s]  %s", f.name, desc, sz_buf);
                        } else {
                            snprintf(label, sizeof(label),
                                "%s  %s", f.name, sz_buf);
                        }

                        if (ImGui::Selectable(label)) {
                            std::string full_path = std::string(test_data_dir) + "/" + f.name;
                            strncpy(g_open_dlg_path, full_path.c_str(),
                                    sizeof(g_open_dlg_path) - 1);
                            open_volume(full_path.c_str());
                            gui_log(GUI_LOG_INFO,
                                "Test data: loaded %s", f.name);
                        }
                        ImGui::PopID();
                    }
                }

                ImGui::Spacing();
                if (ImGui::Button("Rescan")) {
                    test_data_scanned = false;
                }
                ImGui::SameLine();
                ImGui::TextDisabled("(%s)", test_data_dir);
            }
            ImGui::EndTabItem();
        }

        /* ============================================================ */
        /*  TAB 2: Browse — general directory file browser              */
        /* ============================================================ */
        if (ImGui::BeginTabItem("Browse")) {
            /* Initialise browse dir to project root on first use */
            if (g_browse_dir[0] == '\0') {
                /* Try to find the project root */
                const char *try_dirs[] = {
                    ".", "..", "../..", "../../..", "../../../.."
                };
                bool found = false;
                for (size_t i = 0; i < sizeof(try_dirs) / sizeof(try_dirs[0]); ++i) {
                    std::string check = std::string(try_dirs[i]) + "/CMakeLists.txt";
                    FILE *fp = fopen(check.c_str(), "r");
                    if (fp) {
                        fclose(fp);
                        strncpy(g_browse_dir, try_dirs[i], sizeof(g_browse_dir) - 1);
                        found = true;
                        break;
                    }
                }
                if (!found) strncpy(g_browse_dir, ".", sizeof(g_browse_dir) - 1);
                g_browse_needs_scan = true;
            }

            /* Directory path + Rescan */
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 70);
            if (ImGui::InputText("##dir", g_browse_dir, sizeof(g_browse_dir),
                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
                g_browse_needs_scan = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Scan", ImVec2(60, 0))) {
                g_browse_needs_scan = true;
            }

            /* Filter */
            ImGui::SetNextItemWidth(200);
            ImGui::InputTextWithHint("##filter", "Filter...",
                                     g_browse_filter, sizeof(g_browse_filter));
            ImGui::SameLine();
            ImGui::TextDisabled("(%d items)", (int)g_browse_entries.size());

            if (g_browse_needs_scan) {
                scan_directory(g_browse_dir);
            }

            /* File listing */
            ImGui::BeginChild("FileList", ImVec2(0, 0), ImGuiChildFlags_Borders,
                              ImGuiWindowFlags_None);
            for (size_t i = 0; i < g_browse_entries.size(); ++i) {
                const DirEntry &e = g_browse_entries[i];

                /* Apply filter */
                if (g_browse_filter[0]) {
                    /* Case-insensitive substring search */
                    char lower_name[256], lower_filt[64];
                    strncpy(lower_name, e.name, sizeof(lower_name) - 1);
                    lower_name[sizeof(lower_name) - 1] = '\0';
                    strncpy(lower_filt, g_browse_filter, sizeof(lower_filt) - 1);
                    lower_filt[sizeof(lower_filt) - 1] = '\0';
                    for (char *c = lower_name; *c; ++c)
                        if (*c >= 'A' && *c <= 'Z') *c += 32;
                    for (char *c = lower_filt; *c; ++c)
                        if (*c >= 'A' && *c <= 'Z') *c += 32;
                    if (!strstr(lower_name, lower_filt))
                        continue;
                }

                ImGui::PushID((int)i);
                if (e.is_dir) {
                    /* Directory entry — icon + navigate on click */
                    char dir_label[280];
                    snprintf(dir_label, sizeof(dir_label),
                             "\xf0\x9f\x93\x81 %s/", e.name);
                    if (ImGui::Selectable(dir_label)) {
                        if (strcmp(e.name, "..") == 0) {
                            /* Go up */
                            char *last_sep = strrchr(g_browse_dir, '/');
#ifdef _WIN32
                            char *last_bsep = strrchr(g_browse_dir, '\\');
                            if (last_bsep > last_sep) last_sep = last_bsep;
#endif
                            if (last_sep && last_sep != g_browse_dir) {
                                *last_sep = '\0';
                            }
                        } else {
                            size_t dlen = strlen(g_browse_dir);
                            if (dlen > 0 && g_browse_dir[dlen - 1] != '/')
                                strncat(g_browse_dir, "/",
                                        sizeof(g_browse_dir) - dlen - 1);
                            strncat(g_browse_dir, e.name,
                                    sizeof(g_browse_dir) - strlen(g_browse_dir) - 1);
                        }
                        g_browse_needs_scan = true;
                    }
                } else {
                    /* File entry */
                    bool is_vol = is_volume_file(e.name);
                    char sz_buf[32];
                    format_size(e.size, sz_buf, sizeof(sz_buf));

                    if (is_vol) {
                        /* Highlighted selectable for volume files */
                        char file_label[320];
                        snprintf(file_label, sizeof(file_label),
                                 "\xf0\x9f\x93\x84 %s  (%s)", e.name, sz_buf);
                        if (ImGui::Selectable(file_label)) {
                            std::string full = std::string(g_browse_dir) + "/" + e.name;
                            strncpy(g_open_dlg_path, full.c_str(),
                                    sizeof(g_open_dlg_path) - 1);
                            /* Auto-detect for raw files */
                            if (path_is_raw(full.c_str())) {
                                autodetect_raw_params(full.c_str());
                            }
                            open_volume(full.c_str());
                            gui_log(GUI_LOG_INFO, "Loaded: %s", e.name);
                        }
                    } else {
                        /* Non-volume file — just display, greyed out */
                        ImGui::TextDisabled("   %s  (%s)", e.name, sz_buf);
                    }
                }
                ImGui::PopID();
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

/* ---- volume info + statistics panel (8B.4 / 8B.5) ---- */
static void draw_volume_info(void)
{
    if (!g_show_volume_info) return;
    if (ImGui::Begin("Volume Info", &g_show_volume_info)) {
        if (!g_vol.loaded) {
            ImGui::TextWrapped("Volume information will be displayed here "
                               "after loading a dataset.");
            ImGui::Separator();
            ImGui::Text("Dimensions:   —");
            ImGui::Text("Bit depth:    —");
            ImGui::Text("Components:   —");
            ImGui::Text("Tile grid:    —");
            ImGui::Text("DWT levels:   —");
            ImGui::Text("Compression:  —");
        } else {
            const opj_volume_t *v  = g_vol.vol;
            const opj_volume_comp_t *c0 = &v->comps[0];

            /* ---- metadata (8B.4) ---- */
            ImGui::SeparatorText("Metadata");
            ImGui::Text("Dimensions:  %u x %u x %u",
                        c0->w, c0->h, c0->d);
            ImGui::Text("Bit depth:   %u (%s)",
                        c0->prec, c0->sgnd ? "signed" : "unsigned");
            ImGui::Text("Components:  %u", v->numcomps);
            ImGui::Text("Voxel dz:    %.3f", (double)c0->dz);

            if (g_vol.is_jp3d) {
                ImGui::Text("DWT levels:  %u", g_vol.num_resolutions);
                ImGui::Text("Compression: %s%s",
                            g_vol.filter_type == OPJ_JP3D_FILTER_97
                                ? "9/7 (lossy)" : "5/3 (lossless)",
                            g_vol.htj2k_mode ? " + HTJ2K" : "");
            } else {
                ImGui::Text("DWT levels:  —  (raw file)");
                ImGui::Text("Compression: —  (raw file)");
            }

            /* ---- statistics (8B.5) ---- */
            if (g_vol.comp_stats && g_vol.comp_stats[0].computed) {
                ImGui::Spacing();
                ImGui::SeparatorText("Statistics (Component 0)");

                const GuiCompStats *st = &g_vol.comp_stats[0];
                ImGui::Text("Min:    %.1f", (double)st->min_val);
                ImGui::Text("Max:    %.1f", (double)st->max_val);
                ImGui::Text("Mean:   %.2f", (double)st->mean);
                ImGui::Text("StdDev: %.2f", (double)st->stddev);

                /* ---- histogram ---- */
                ImGui::Spacing();
                ImGui::TextDisabled("Intensity histogram");
                float hist_w = ImGui::GetContentRegionAvail().x;
                ImGui::PlotHistogram("##hist",
                                     st->histogram, GUI_HIST_BINS,
                                     0, NULL, 0.0f, 1.0f,
                                     ImVec2(hist_w, 80));

                /* Window / level controls */
                ImGui::Spacing();
                ImGui::SeparatorText("Window / Level");
                float wl_min = st->min_val;
                float wl_max = st->max_val;
                float range  = wl_max - wl_min;
                if (range < 1.0f) range = 1.0f;

                bool changed = false;
                changed |= ImGui::SliderFloat("Center",
                               &g_vol.win_center, wl_min, wl_max);
                changed |= ImGui::SliderFloat("Width",
                               &g_vol.win_width,  1.0f, range);
                if (changed) {
                    g_vol.auto_wl    = false;
                    g_vol.tex_dirty  = true;
                }
                if (ImGui::Button("Reset W/L")) {
                    g_vol.auto_wl   = true;
                    g_vol.win_width  = range;
                    g_vol.win_center = wl_min + range * 0.5f;
                    g_vol.tex_dirty  = true;
                }
            }
        }
    }
    ImGui::End();
}

/* ---- slice viewport (8B.2 / 8B.3) ---- */
static void draw_viewport(void)
{
    if (!g_show_viewport) return;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    if (ImGui::Begin("Viewport", &g_show_viewport)) {
        if (!g_vol.loaded) {
            ImGui::TextDisabled("Load a volume to begin viewing.");
        } else {
            /* ---- View-mode toggle ---- */
            ImGui::RadioButton("Slice", (int *)&g_vol.show_3d, 0);
            ImGui::SameLine();
            ImGui::RadioButton("3-D Ray-Cast", (int *)&g_vol.show_3d, 1);
            ImGui::SameLine();

            /* ---- Axis selector (slice mode only) ---- */
            if (!g_vol.show_3d) {
                ImGui::Separator();
                ImGui::SameLine();
                const char *axes[] = { "Axial (Z)", "Sagittal (X)", "Coronal (Y)" };
                int ax = (int)g_vol.slice_axis;
                ImGui::PushItemWidth(130);
                if (ImGui::Combo("Axis", &ax, axes, 3)) {
                    g_vol.slice_axis = (GuiSliceAxis)ax;
                    gui_volume_clamp_slice(&g_vol);
                    g_vol.tex_dirty = true;
                }
                ImGui::PopItemWidth();

                /* Slice index slider */
                int depth = gui_volume_axis_depth(&g_vol);
                ImGui::SameLine();
                ImGui::PushItemWidth(200);
                if (ImGui::SliderInt("Slice", &g_vol.slice_idx, 0, depth - 1)) {
                    g_vol.tex_dirty = true;
                }
                ImGui::PopItemWidth();
            } else {
                /* 3-D controls */
                ImGui::SameLine();
                ImGui::PushItemWidth(120);
                ImGui::SliderFloat("Az",  &g_vol.rc_azimuth,   -180.0f, 180.0f);
                ImGui::SameLine();
                ImGui::SliderFloat("El",  &g_vol.rc_elevation,  -90.0f,  90.0f);
                ImGui::SameLine();
                ImGui::SliderFloat("Density", &g_vol.rc_density, 0.01f, 5.0f);
                ImGui::PopItemWidth();
            }

            ImGui::Separator();

            /* ---- Image area ---- */
            ImVec2 avail = ImGui::GetContentRegionAvail();
            int iw = (int)avail.x;
            int ih = (int)avail.y;
            if (iw < 4) iw = 4;
            if (ih < 4) ih = 4;

            if (!g_vol.show_3d) {
                /* --- Slice viewer --- */
                if (g_vol.tex_dirty) {
                    gui_volume_update_slice_texture(&g_vol);
                }
                if (g_vol.tex_id && g_vol.tex_w > 0 && g_vol.tex_h > 0) {
                    /* Fit texture into available area preserving aspect */
                    float aspect = (float)g_vol.tex_w / (float)g_vol.tex_h;
                    float disp_w = (float)iw;
                    float disp_h = disp_w / aspect;
                    if (disp_h > (float)ih) {
                        disp_h = (float)ih;
                        disp_w = disp_h * aspect;
                    }
                    /* Centre it */
                    float off_x = ((float)iw - disp_w) * 0.5f;
                    float off_y = ((float)ih - disp_h) * 0.5f;
                    ImVec2 cursor = ImGui::GetCursorScreenPos();
                    ImGui::SetCursorScreenPos(
                        ImVec2(cursor.x + off_x, cursor.y + off_y));
                    ImGui::Image(
                        (ImTextureID)(intptr_t)g_vol.tex_id,
                        ImVec2(disp_w, disp_h));

                    /* Scroll-wheel slice navigation */
                    if (ImGui::IsItemHovered()) {
                        float wheel = ImGui::GetIO().MouseWheel;
                        if (wheel != 0.0f) {
                            g_vol.slice_idx -= (int)wheel;
                            gui_volume_clamp_slice(&g_vol);
                            g_vol.tex_dirty = true;
                        }
                    }
                }
            } else {
                /* --- 3-D ray-cast view --- */
                gui_volume_render_raycast(&g_vol, iw, ih);
                if (g_vol.rc_color_tex) {
                    ImGui::Image(
                        (ImTextureID)(intptr_t)g_vol.rc_color_tex,
                        ImVec2((float)iw, (float)ih));
                }
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

/* ---- log console (Phase 8F.1 — delegates to GuiLogState) ---- */
static void draw_log_console(void)
{
    gui_log_draw_panel(&g_log);
}

/* ---- about dialog ---- */
static void draw_about_dialog(void)
{
    if (!g_show_about) return;
    ImGui::SetNextWindowSize(ImVec2(420, 240), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("About OpenJP3D GUI", &g_show_about,
                     ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("OpenJP3D GUI v%s", OPJ_JP3D_VERSION);
        ImGui::Separator();
        ImGui::TextWrapped(
            "Interactive GUI test application for the OpenJP3D "
            "JPEG 2000 Part 10 (JP3D) volumetric codec.\n\n"
            "Built with Dear ImGui + SDL2 + OpenGL 3.3.\n\n"
            "Licensed under BSD-2-Clause.");
        ImGui::Separator();
        ImGui::Text("Dear ImGui %s", IMGUI_VERSION);
        ImGui::Text("SDL %d.%d.%d",
                    SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL);
    }
    ImGui::End();
}

/* ================================================================== */
/*  Main                                                              */
/* ================================================================== */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    /* -------------------------------------------------------------- */
    /*  SDL2 + OpenGL context                                         */
    /* -------------------------------------------------------------- */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "Error: SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

#if defined(__APPLE__)
    const char *glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,
                        SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    const char *glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#endif

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_Window *window = SDL_CreateWindow(
        "OpenJP3D GUI — JPEG 2000 Part 10 Volumetric Codec",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 800,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        fprintf(stderr, "Error: SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); /* vsync */

    /* -------------------------------------------------------------- */
    /*  Dear ImGui context                                            */
    /* -------------------------------------------------------------- */
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    /* Apply default theme */
    opj_gui_apply_theme(OPJ_GUI_THEME_DARK);

    /* Platform / renderer backends */
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    /* Initialise log, preferences, and shortcuts (Phase 8F) */
    gui_log_state_init(&g_log);
    gui_prefs_state_init(&g_prefs);
    gui_prefs_resolve_config_path(&g_prefs);
    gui_prefs_load(&g_prefs);
    gui_shortcuts_state_init(&g_shortcuts);
    gui_shortcuts_load(&g_shortcuts, g_prefs.config_path);
    /* Apply persisted theme */
    opj_gui_apply_theme(g_prefs.theme == 0 ? OPJ_GUI_THEME_DARK
                                           : OPJ_GUI_THEME_LIGHT);

    /* Initialise volume state */
    gui_volume_state_init(&g_vol);

    /* Initialise codec panel state */
    gui_codec_state_init(&g_codec);

    /* Initialise round-trip state (Phase 8D) */
    gui_roundtrip_state_init(&g_rt);

    /* Initialise JPIP streaming client state (Phase 8E) */
    gui_jpip_state_init(&g_jpip);

    gui_log(GUI_LOG_INFO, "OpenJP3D GUI started (v%s)", OPJ_JP3D_VERSION);
    gui_log(GUI_LOG_INFO, "Dear ImGui %s, SDL %d.%d.%d",
            IMGUI_VERSION, SDL_MAJOR_VERSION, SDL_MINOR_VERSION,
            SDL_PATCHLEVEL);
    if (g_prefs.config_path[0])
        gui_log(GUI_LOG_INFO, "Preferences: %s", g_prefs.config_path);

    /* -------------------------------------------------------------- */
    /*  Main loop                                                     */
    /* -------------------------------------------------------------- */
    while (g_running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                g_running = false;
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(window))
                g_running = false;
        }

        /* Keyboard shortcuts (Phase 8F.4 — configurable bindings) */
        if (gui_shortcuts_check(&g_shortcuts, GUI_ACTION_QUIT))
            g_running = false;
        if (gui_shortcuts_check(&g_shortcuts, GUI_ACTION_TOGGLE_THEME)) {
            enum opj_gui_theme t = opj_gui_toggle_theme();
            g_prefs.theme = (t == OPJ_GUI_THEME_DARK) ? 0 : 1;
            gui_log(GUI_LOG_INFO, "Theme switched to %s",
                    t == OPJ_GUI_THEME_DARK ? "Dark" : "Light");
        }
        if (gui_shortcuts_check(&g_shortcuts, GUI_ACTION_OPEN)) {
            g_open_dlg_visible  = true;
            g_open_dlg_error[0] = '\0';
        }
        if (gui_shortcuts_check(&g_shortcuts, GUI_ACTION_ENCODE))
            g_codec.show_encode = true;
        if (gui_shortcuts_check(&g_shortcuts, GUI_ACTION_DECODE))
            g_codec.show_decode = true;
        if (g_vol.loaded) {
            if (gui_shortcuts_check(&g_shortcuts, GUI_ACTION_NEXT_SLICE)) {
                g_vol.slice_idx++;
                gui_volume_clamp_slice(&g_vol);
                g_vol.tex_dirty = true;
            }
            if (gui_shortcuts_check(&g_shortcuts, GUI_ACTION_PREV_SLICE)) {
                g_vol.slice_idx--;
                gui_volume_clamp_slice(&g_vol);
                g_vol.tex_dirty = true;
            }
        }

        /* New frame */
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        /* ---- Dockspace over the entire viewport (below toolbar) --- */
        ImGuiViewport *vp = ImGui::GetMainViewport();
        float toolbar_h = ImGui::GetFrameHeight() + 8.0f;
        ImGui::SetNextWindowPos(
            ImVec2(vp->WorkPos.x, vp->WorkPos.y + toolbar_h));
        ImGui::SetNextWindowSize(
            ImVec2(vp->WorkSize.x, vp->WorkSize.y - toolbar_h));
        ImGui::SetNextWindowViewport(vp->ID);

        ImGuiWindowFlags dock_flags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("DockSpace", NULL, dock_flags);
        ImGui::PopStyleVar(3);

        ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0, 0),
                         ImGuiDockNodeFlags_PassthruCentralNode);

        /* --- Build default layout on first run --- */
        static bool first_run = true;
        if (first_run) {
            first_run = false;
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id,
                                      ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, vp->WorkSize);

            ImGuiID dock_left, dock_right;
            ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left,
                                        0.22f, &dock_left, &dock_right);

            ImGuiID dock_left_top, dock_left_bottom;
            ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Up,
                                        0.35f, &dock_left_top,
                                        &dock_left_bottom);

            ImGuiID dock_center, dock_bottom;
            ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Down,
                                        0.22f, &dock_bottom, &dock_center);

            ImGui::DockBuilderDockWindow("File Browser",  dock_left_top);
            ImGui::DockBuilderDockWindow("Volume Info",   dock_left_bottom);
            ImGui::DockBuilderDockWindow("Viewport",      dock_center);
            ImGui::DockBuilderDockWindow("Log Console",   dock_bottom);
            ImGui::DockBuilderFinish(dockspace_id);
        }

        ImGui::End(); /* DockSpace */

        /* ---- Tick codec state (poll background tasks) ---- */
        gui_codec_tick(&g_codec);

        /* ---- Tick round-trip state (poll background tasks) ---- */
        gui_roundtrip_tick(&g_rt);

        /* ---- Tick JPIP state (poll background fetch) ---- */
        gui_jpip_tick(&g_jpip);

        /* ---- Handle decode result: load into viewer ---- */
        if (g_codec.decode_result && !g_codec.task_running.load()) {
            /* Replace current volume with decoded result */
            gui_volume_state_free(&g_vol);
            g_vol.vol    = g_codec.decode_result;
            g_vol.loaded = true;
            g_vol.is_jp3d     = true;
            g_vol.htj2k_mode  = false;
            g_vol.filter_type = OPJ_JP3D_FILTER_53;
            g_vol.num_resolutions = 0;
            g_vol.slice_axis  = SLICE_AXIS_Z;
            g_vol.slice_idx   = 0;
            g_vol.tex_dirty      = true;
            g_vol.vol_tex_dirty  = true;
            g_vol.auto_wl        = true;
            strncpy(g_vol.filepath, g_codec.dec_input_path,
                    sizeof(g_vol.filepath) - 1);
            g_vol.filepath[sizeof(g_vol.filepath) - 1] = '\0';
            gui_volume_compute_stats(&g_vol);
            g_codec.decode_result = NULL;
            gui_log(GUI_LOG_INFO, "Decoded volume loaded into viewer.");
        }

        /* ---- Draw panels ---- */
        draw_menu_bar();
        draw_toolbar();
        draw_file_browser();
        draw_volume_info();
        draw_viewport();
        draw_log_console();
        draw_about_dialog();
        draw_open_dialog();

        /* ---- Codec panels (Phase 8C) ---- */
        gui_codec_draw_encode_panel(&g_codec, &g_vol);
        gui_codec_draw_decode_panel(&g_codec, &g_vol);
        gui_codec_draw_transcode_panel(&g_codec);
        gui_codec_draw_progress(&g_codec);

        /* ---- Round-trip / diff / batch / inspector (Phase 8D) ---- */
        gui_roundtrip_draw_wizard(&g_rt, &g_vol);
        gui_roundtrip_draw_diff(&g_rt, &g_vol);
        gui_roundtrip_draw_batch(&g_rt, &g_vol);
        gui_roundtrip_draw_inspector(&g_rt);

        /* ---- JPIP streaming client panels (Phase 8E) ---- */
        gui_jpip_draw_connection(&g_jpip);
        gui_jpip_draw_browser(&g_jpip, &g_vol);
        gui_jpip_draw_diagnostics(&g_jpip);

        /* ---- Logging, preferences, and shortcuts (Phase 8F) ---- */
        gui_prefs_draw(&g_prefs);
        gui_shortcuts_draw(&g_shortcuts);

        /* ---- Render ---- */
        ImGui::Render();
        int display_w, display_h;
        SDL_GL_GetDrawableSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);

        ImVec4 bg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
        glClearColor(bg.x, bg.y, bg.z, bg.w);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    /* -------------------------------------------------------------- */
    /*  Cleanup                                                       */
    /* -------------------------------------------------------------- */
    /* Save shortcuts and prefs on exit (Phase 8F) */
    gui_shortcuts_save(&g_shortcuts, g_prefs.config_path);
    if (g_prefs.dirty)
        gui_prefs_save(&g_prefs);

    gui_jpip_state_free(&g_jpip);
    gui_roundtrip_state_free(&g_rt);
    gui_codec_state_free(&g_codec);
    gui_volume_state_free(&g_vol);
    gui_log_state_free(&g_log);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
