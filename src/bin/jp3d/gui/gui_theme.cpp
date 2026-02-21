/*
 * Copyright (c) 2024-2026, OpenJP3D Contributors
 * All rights reserved.
 * SPDX-License-Identifier: BSD-2-Clause
 */

/**
 * @file gui_theme.cpp
 * @brief Theme management — light and dark presets for Dear ImGui.
 */

#include "gui_theme.h"
#include "imgui.h"

static enum opj_gui_theme g_current_theme = OPJ_GUI_THEME_DARK;

/* ------------------------------------------------------------------ */
/*  Dark theme                                                        */
/* ------------------------------------------------------------------ */
static void apply_dark_theme(void)
{
    ImGuiStyle &style = ImGui::GetStyle();
    ImGui::StyleColorsDark(&style);

    /* Rounding & spacing */
    style.WindowRounding    = 4.0f;
    style.ChildRounding     = 4.0f;
    style.FrameRounding     = 3.0f;
    style.PopupRounding     = 3.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding      = 3.0f;
    style.TabRounding       = 4.0f;
    style.FramePadding      = ImVec2(6.0f, 4.0f);
    style.ItemSpacing       = ImVec2(8.0f, 6.0f);

    ImVec4 *c = style.Colors;

    /* Window / panel backgrounds — dark grey */
    c[ImGuiCol_WindowBg]   = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    c[ImGuiCol_ChildBg]    = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
    c[ImGuiCol_PopupBg]    = ImVec4(0.10f, 0.10f, 0.12f, 0.96f);

    /* Title bar */
    c[ImGuiCol_TitleBg]          = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    c[ImGuiCol_TitleBgActive]    = ImVec4(0.14f, 0.28f, 0.50f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.08f, 0.10f, 0.60f);

    /* Menu bar */
    c[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);

    /* Frames / inputs */
    c[ImGuiCol_FrameBg]        = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.24f, 0.28f, 1.00f);
    c[ImGuiCol_FrameBgActive]  = ImVec4(0.28f, 0.28f, 0.34f, 1.00f);

    /* Buttons */
    c[ImGuiCol_Button]        = ImVec4(0.20f, 0.36f, 0.56f, 1.00f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.44f, 0.66f, 1.00f);
    c[ImGuiCol_ButtonActive]  = ImVec4(0.18f, 0.30f, 0.48f, 1.00f);

    /* Headers (collapsing headers, menu items) */
    c[ImGuiCol_Header]        = ImVec4(0.20f, 0.36f, 0.56f, 0.60f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.44f, 0.66f, 0.80f);
    c[ImGuiCol_HeaderActive]  = ImVec4(0.18f, 0.30f, 0.48f, 1.00f);

    /* Tabs */
    c[ImGuiCol_Tab]                = ImVec4(0.14f, 0.14f, 0.18f, 1.00f);
    c[ImGuiCol_TabHovered]         = ImVec4(0.26f, 0.44f, 0.66f, 0.80f);
    c[ImGuiCol_TabSelected]        = ImVec4(0.20f, 0.36f, 0.56f, 1.00f);
    c[ImGuiCol_TabDimmed]          = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    c[ImGuiCol_TabDimmedSelected]  = ImVec4(0.14f, 0.26f, 0.42f, 1.00f);

    /* Scrollbar */
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.10f, 0.10f, 0.12f, 0.60f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.30f, 0.30f, 0.34f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.44f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.50f, 0.50f, 0.54f, 1.00f);

    /* Separator */
    c[ImGuiCol_Separator] = ImVec4(0.28f, 0.28f, 0.32f, 1.00f);

    /* Text — high contrast */
    c[ImGuiCol_Text]         = ImVec4(0.92f, 0.92f, 0.94f, 1.00f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.54f, 1.00f);

    /* Docking */
    c[ImGuiCol_DockingPreview] = ImVec4(0.20f, 0.36f, 0.56f, 0.70f);
    c[ImGuiCol_DockingEmptyBg] = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
}

/* ------------------------------------------------------------------ */
/*  Light theme                                                       */
/* ------------------------------------------------------------------ */
static void apply_light_theme(void)
{
    ImGuiStyle &style = ImGui::GetStyle();
    ImGui::StyleColorsLight(&style);

    /* Rounding & spacing — same as dark theme for consistency */
    style.WindowRounding    = 4.0f;
    style.ChildRounding     = 4.0f;
    style.FrameRounding     = 3.0f;
    style.PopupRounding     = 3.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding      = 3.0f;
    style.TabRounding       = 4.0f;
    style.FramePadding      = ImVec2(6.0f, 4.0f);
    style.ItemSpacing       = ImVec2(8.0f, 6.0f);

    ImVec4 *c = style.Colors;

    /* Window / panel backgrounds — light grey */
    c[ImGuiCol_WindowBg]   = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
    c[ImGuiCol_ChildBg]    = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
    c[ImGuiCol_PopupBg]    = ImVec4(1.00f, 1.00f, 1.00f, 0.98f);

    /* Title bar */
    c[ImGuiCol_TitleBg]          = ImVec4(0.88f, 0.88f, 0.90f, 1.00f);
    c[ImGuiCol_TitleBgActive]    = ImVec4(0.24f, 0.50f, 0.76f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.88f, 0.88f, 0.90f, 0.60f);

    /* Menu bar */
    c[ImGuiCol_MenuBarBg] = ImVec4(0.90f, 0.90f, 0.92f, 1.00f);

    /* Frames / inputs */
    c[ImGuiCol_FrameBg]        = ImVec4(0.86f, 0.86f, 0.88f, 1.00f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.78f, 0.82f, 0.88f, 1.00f);
    c[ImGuiCol_FrameBgActive]  = ImVec4(0.70f, 0.76f, 0.84f, 1.00f);

    /* Buttons — blue accent */
    c[ImGuiCol_Button]        = ImVec4(0.24f, 0.50f, 0.76f, 1.00f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.56f, 0.82f, 1.00f);
    c[ImGuiCol_ButtonActive]  = ImVec4(0.18f, 0.42f, 0.68f, 1.00f);

    /* Headers */
    c[ImGuiCol_Header]        = ImVec4(0.24f, 0.50f, 0.76f, 0.40f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.56f, 0.82f, 0.60f);
    c[ImGuiCol_HeaderActive]  = ImVec4(0.18f, 0.42f, 0.68f, 0.80f);

    /* Tabs */
    c[ImGuiCol_Tab]                = ImVec4(0.88f, 0.88f, 0.90f, 1.00f);
    c[ImGuiCol_TabHovered]         = ImVec4(0.30f, 0.56f, 0.82f, 0.60f);
    c[ImGuiCol_TabSelected]        = ImVec4(0.24f, 0.50f, 0.76f, 1.00f);
    c[ImGuiCol_TabDimmed]          = ImVec4(0.92f, 0.92f, 0.94f, 1.00f);
    c[ImGuiCol_TabDimmedSelected]  = ImVec4(0.44f, 0.64f, 0.84f, 1.00f);

    /* Scrollbar */
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.92f, 0.92f, 0.94f, 0.60f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.64f, 0.64f, 0.68f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.50f, 0.50f, 0.54f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.40f, 0.40f, 0.44f, 1.00f);

    /* Separator */
    c[ImGuiCol_Separator] = ImVec4(0.72f, 0.72f, 0.76f, 1.00f);

    /* Text — dark for high contrast on light backgrounds */
    c[ImGuiCol_Text]         = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.46f, 0.46f, 0.50f, 1.00f);

    /* Docking */
    c[ImGuiCol_DockingPreview] = ImVec4(0.24f, 0.50f, 0.76f, 0.50f);
    c[ImGuiCol_DockingEmptyBg] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
}

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */

void opj_gui_apply_theme(enum opj_gui_theme theme)
{
    g_current_theme = theme;
    switch (theme) {
    case OPJ_GUI_THEME_LIGHT:
        apply_light_theme();
        break;
    case OPJ_GUI_THEME_DARK:  /* fall-through */
    default:
        apply_dark_theme();
        break;
    }
}

enum opj_gui_theme opj_gui_get_current_theme(void)
{
    return g_current_theme;
}

enum opj_gui_theme opj_gui_toggle_theme(void)
{
    enum opj_gui_theme next = (g_current_theme == OPJ_GUI_THEME_DARK)
                                  ? OPJ_GUI_THEME_LIGHT
                                  : OPJ_GUI_THEME_DARK;
    opj_gui_apply_theme(next);
    return next;
}
