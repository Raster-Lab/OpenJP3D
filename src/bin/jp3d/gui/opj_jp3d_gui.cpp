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
static bool  g_open_raw_params_dlg = false;
static RawOpenParams g_raw_params  = {64, 64, 64, 8, 0, 1};
static char  g_open_dlg_error[256] = "";

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

    ImGui::SetNextWindowSize(ImVec2(520, 220), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Open Volume", &g_open_dlg_visible,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    ImGui::TextWrapped("Enter the path to a JP3D codestream (.jp3d/.j3d) "
                       "or a raw binary volume (.raw/.vol).");
    ImGui::Separator();

    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##path", g_open_dlg_path, sizeof(g_open_dlg_path));

    ImGui::Spacing();
    bool is_raw = path_is_raw(g_open_dlg_path);

    if (is_raw) {
        ImGui::TextDisabled("Raw file detected — set dimensions below:");
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
        /* Clamp to valid ranges */
        if (g_raw_params.width     < 1) g_raw_params.width     = 1;
        if (g_raw_params.height    < 1) g_raw_params.height    = 1;
        if (g_raw_params.depth     < 1) g_raw_params.depth     = 1;
        if (g_raw_params.prec      < 1) g_raw_params.prec      = 1;
        if (g_raw_params.prec     > 32) g_raw_params.prec      = 32;
        if (g_raw_params.numcomps < 1)  g_raw_params.numcomps  = 1;
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
static void draw_file_browser(void)
{
    if (!g_show_file_browser) return;
    if (ImGui::Begin("File Browser", &g_show_file_browser)) {
        ImGui::TextWrapped("Use File > Open Volume (Ctrl+O) or the "
                           "Open toolbar button to load a volume.");
        ImGui::Separator();

        if (g_vol.loaded) {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Loaded:");
            /* Show just the filename part */
            const char *slash = strrchr(g_vol.filepath, '/');
#ifdef _WIN32
            const char *bslash = strrchr(g_vol.filepath, '\\');
            if (bslash && (!slash || bslash > slash)) slash = bslash;
#endif
            ImGui::TextWrapped("%s", slash ? slash + 1 : g_vol.filepath);
            ImGui::Spacing();
            if (ImGui::Button("Close Volume")) {
                gui_volume_state_free(&g_vol);
                gui_log(GUI_LOG_INFO, "Volume closed.");
            }
        } else {
            ImGui::TextDisabled("(No volume loaded)");
            ImGui::Spacing();
            if (ImGui::Button("Open Volume...")) {
                g_open_dlg_visible  = true;
                g_open_dlg_error[0] = '\0';
            }
        }
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
