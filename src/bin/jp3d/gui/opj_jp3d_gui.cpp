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
 *   - File browser panel
 *   - Volume info panel
 *   - Slice / volume viewport
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

/* ================================================================== */
/*  Log panel ring buffer                                             */
/* ================================================================== */

enum log_severity {
    LOG_INFO    = 0,
    LOG_WARNING = 1,
    LOG_ERROR   = 2
};

struct log_entry {
    enum log_severity severity;
    std::string       message;
};

static std::vector<log_entry> g_log_entries;
static bool g_log_scroll_to_bottom = true;
static int  g_log_filter_mask = (1 << LOG_INFO) | (1 << LOG_WARNING) | (1 << LOG_ERROR);

static void gui_log(enum log_severity sev, const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    g_log_entries.push_back({sev, std::string(buf)});
    g_log_scroll_to_bottom = true;
}

/* ================================================================== */
/*  Panel state                                                       */
/* ================================================================== */

static bool g_show_file_browser   = true;
static bool g_show_volume_info    = true;
static bool g_show_viewport       = true;
static bool g_show_log_console    = true;
static bool g_show_about          = false;
static bool g_running             = true;

/* ================================================================== */
/*  Panel drawing helpers                                             */
/* ================================================================== */

static void draw_menu_bar(void)
{
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open Volume...", "Ctrl+O")) {
                gui_log(LOG_INFO, "File > Open Volume (not yet implemented)");
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
            ImGui::MenuItem("Log Console",   NULL, &g_show_log_console);
            ImGui::Separator();
            if (ImGui::MenuItem("Toggle Theme", "Ctrl+T")) {
                enum opj_gui_theme t = opj_gui_toggle_theme();
                gui_log(LOG_INFO, "Theme switched to %s",
                        t == OPJ_GUI_THEME_DARK ? "Dark" : "Light");
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Tools")) {
            if (ImGui::MenuItem("Encode...", NULL, false, false)) {}
            if (ImGui::MenuItem("Decode...", NULL, false, false)) {}
            if (ImGui::MenuItem("Transcode...", NULL, false, false)) {}
            ImGui::Separator();
            if (ImGui::MenuItem("Round-Trip Test...", NULL, false, false)) {}
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
            gui_log(LOG_INFO, "Toolbar > Open (not yet implemented)");
        }
        ImGui::SameLine();
        if (ImGui::Button("Encode")) {
            gui_log(LOG_INFO, "Toolbar > Encode (not yet implemented)");
        }
        ImGui::SameLine();
        if (ImGui::Button("Decode")) {
            gui_log(LOG_INFO, "Toolbar > Decode (not yet implemented)");
        }
        ImGui::SameLine();
        if (ImGui::Button("Transcode")) {
            gui_log(LOG_INFO, "Toolbar > Transcode (not yet implemented)");
        }
        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();
        if (ImGui::Button("Round-Trip")) {
            gui_log(LOG_INFO, "Toolbar > Round-Trip (not yet implemented)");
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
            gui_log(LOG_INFO, "Theme switched to %s",
                    t == OPJ_GUI_THEME_DARK ? "Dark" : "Light");
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

static void draw_file_browser(void)
{
    if (!g_show_file_browser) return;
    if (ImGui::Begin("File Browser", &g_show_file_browser)) {
        ImGui::TextWrapped("File browser panel — drag and drop files or "
                           "use File > Open Volume to load data.");
        ImGui::Separator();
        ImGui::TextDisabled("(No files loaded)");
    }
    ImGui::End();
}

static void draw_volume_info(void)
{
    if (!g_show_volume_info) return;
    if (ImGui::Begin("Volume Info", &g_show_volume_info)) {
        ImGui::TextWrapped("Volume information will be displayed here "
                           "after loading a dataset.");
        ImGui::Separator();
        ImGui::Text("Dimensions:   —");
        ImGui::Text("Bit depth:    —");
        ImGui::Text("Components:   —");
        ImGui::Text("Tile grid:    —");
        ImGui::Text("DWT levels:   —");
        ImGui::Text("Compression:  —");
    }
    ImGui::End();
}

static void draw_viewport(void)
{
    if (!g_show_viewport) return;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if (ImGui::Begin("Viewport", &g_show_viewport)) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGui::TextWrapped(" Slice / volume viewport  (%d x %d)",
                           (int)avail.x, (int)avail.y);
        /* Future: render slice texture here */
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

static void draw_log_console(void)
{
    if (!g_show_log_console) return;
    if (ImGui::Begin("Log Console", &g_show_log_console)) {
        /* Filter buttons */
        bool f_info = (g_log_filter_mask & (1 << LOG_INFO)) != 0;
        bool f_warn = (g_log_filter_mask & (1 << LOG_WARNING)) != 0;
        bool f_err  = (g_log_filter_mask & (1 << LOG_ERROR)) != 0;
        if (ImGui::Checkbox("Info", &f_info))
            g_log_filter_mask ^= (1 << LOG_INFO);
        ImGui::SameLine();
        if (ImGui::Checkbox("Warning", &f_warn))
            g_log_filter_mask ^= (1 << LOG_WARNING);
        ImGui::SameLine();
        if (ImGui::Checkbox("Error", &f_err))
            g_log_filter_mask ^= (1 << LOG_ERROR);
        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            g_log_entries.clear();
        }
        ImGui::Separator();

        /* Scrollable log area */
        ImGui::BeginChild("LogScroll", ImVec2(0, 0), ImGuiChildFlags_None,
                          ImGuiWindowFlags_HorizontalScrollbar);
        for (const auto &entry : g_log_entries) {
            if (!(g_log_filter_mask & (1 << entry.severity)))
                continue;
            ImVec4 col;
            const char *prefix;
            switch (entry.severity) {
            case LOG_WARNING:
                col = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
                prefix = "[WARN] ";
                break;
            case LOG_ERROR:
                col = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
                prefix = "[ERR]  ";
                break;
            default:
                col = ImGui::GetStyleColorVec4(ImGuiCol_Text);
                prefix = "[INFO] ";
                break;
            }
            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::TextUnformatted((std::string(prefix) +
                                    entry.message).c_str());
            ImGui::PopStyleColor();
        }
        if (g_log_scroll_to_bottom) {
            ImGui::SetScrollHereY(1.0f);
            g_log_scroll_to_bottom = false;
        }
        ImGui::EndChild();
    }
    ImGui::End();
}

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

    gui_log(LOG_INFO, "OpenJP3D GUI started (v%s)", OPJ_JP3D_VERSION);
    gui_log(LOG_INFO, "Dear ImGui %s, SDL %d.%d.%d",
            IMGUI_VERSION, SDL_MAJOR_VERSION, SDL_MINOR_VERSION,
            SDL_PATCHLEVEL);

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

        /* Keyboard shortcuts */
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Q))
            g_running = false;
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_T)) {
            enum opj_gui_theme t = opj_gui_toggle_theme();
            gui_log(LOG_INFO, "Theme switched to %s",
                    t == OPJ_GUI_THEME_DARK ? "Dark" : "Light");
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
                                        0.50f, &dock_left_top,
                                        &dock_left_bottom);

            ImGuiID dock_center, dock_bottom;
            ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Down,
                                        0.28f, &dock_bottom, &dock_center);

            ImGui::DockBuilderDockWindow("File Browser",  dock_left_top);
            ImGui::DockBuilderDockWindow("Volume Info",   dock_left_bottom);
            ImGui::DockBuilderDockWindow("Viewport",      dock_center);
            ImGui::DockBuilderDockWindow("Log Console",   dock_bottom);
            ImGui::DockBuilderFinish(dockspace_id);
        }

        ImGui::End(); /* DockSpace */

        /* ---- Draw panels ---- */
        draw_menu_bar();
        draw_toolbar();
        draw_file_browser();
        draw_volume_info();
        draw_viewport();
        draw_log_console();
        draw_about_dialog();

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
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
