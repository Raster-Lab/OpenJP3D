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
 * @file gui_log_prefs.cpp
 * @brief Log console (8F.1), preferences dialog (8F.2), and keyboard
 *        shortcuts editor (8F.4) for Phase 8F of the OpenJP3D GUI.
 *
 * Also provides gui_log_codec_callback() — the opj_jp3d_msg_callback_t
 * implementation that routes codec messages to the GUI log panel.
 */

#include "gui_log_prefs.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <ctime>
#include <cctype>
#include <algorithm>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

#include "imgui.h"
#include "gui_theme.h"

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>   /* GetEnvironmentVariable, CreateDirectory */
#  include <direct.h>    /* _mkdir */
#else
#  include <sys/stat.h>
#  include <errno.h>
#endif

/* ================================================================== */
/*  Internal helpers                                                   */
/* ================================================================== */

/** Format current wall-clock time as "HH:MM:SS". */
static std::string current_timestamp(void)
{
    time_t t = time(NULL);
    struct tm *tm_info;
#if defined(_WIN32)
    struct tm tm_buf;
    localtime_s(&tm_buf, &t);
    tm_info = &tm_buf;
#else
    struct tm tm_buf;
    localtime_r(&t, &tm_buf);
    tm_info = &tm_buf;
#endif
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    return std::string(buf);
}

/** Severity prefix string. */
static const char *severity_prefix(GuiLogSeverity sev)
{
    switch (sev) {
    case GUI_LOG_WARNING: return "[WARN] ";
    case GUI_LOG_ERROR:   return "[ERR]  ";
    default:              return "[INFO] ";
    }
}

/** Severity colour for ImGui text. */
static ImVec4 severity_colour(GuiLogSeverity sev)
{
    switch (sev) {
    case GUI_LOG_WARNING: return ImVec4(1.0f, 0.80f, 0.20f, 1.0f);
    case GUI_LOG_ERROR:   return ImVec4(1.0f, 0.30f, 0.30f, 1.0f);
    default:              return ImGui::GetStyleColorVec4(ImGuiCol_Text);
    }
}

/* ================================================================== */
/*  8F.1 — Log console                                                */
/* ================================================================== */

void gui_log_state_init(GuiLogState *ls)
{
    ls->show_log            = true;
    ls->scroll_to_bottom    = true;
    ls->filter_info         = true;
    ls->filter_warning      = true;
    ls->filter_error        = true;
    ls->show_export_dialog  = false;
    ls->export_path[0]      = '\0';
    ls->export_status.clear();
}

void gui_log_state_free(GuiLogState *ls)
{
    std::lock_guard<std::mutex> lock(ls->mutex);
    ls->entries.clear();
}

void gui_log_add(GuiLogState *ls, GuiLogSeverity sev, const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    GuiLogEntry entry;
    entry.severity  = sev;
    entry.timestamp = current_timestamp();
    entry.message   = std::string(buf);

    {
        std::lock_guard<std::mutex> lock(ls->mutex);
        ls->entries.push_back(entry);
    }
    ls->scroll_to_bottom = true;
}

void gui_log_codec_callback(opj_jp3d_msg_level_t level,
                            const char *message,
                            void *data)
{
    if (!data || !message) return;
    GuiLogState *ls = (GuiLogState *)data;

    GuiLogSeverity sev;
    switch (level) {
    case OPJ_JP3D_MSG_ERROR:   sev = GUI_LOG_ERROR;   break;
    case OPJ_JP3D_MSG_WARNING: sev = GUI_LOG_WARNING; break;
    default:                   sev = GUI_LOG_INFO;    break;
    }

    /* Strip trailing newline from codec messages */
    std::string msg(message);
    while (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r'))
        msg.pop_back();

    if (!msg.empty())
        gui_log_add(ls, sev, "%s", msg.c_str());
}

/* ---- Internal: build a single string from all visible log entries ---- */
static std::string log_build_text(const GuiLogState *ls)
{
    std::string out;
    out.reserve(ls->entries.size() * 80);
    for (const auto &e : ls->entries) {
        bool visible = false;
        switch (e.severity) {
        case GUI_LOG_INFO:    visible = ls->filter_info;    break;
        case GUI_LOG_WARNING: visible = ls->filter_warning; break;
        case GUI_LOG_ERROR:   visible = ls->filter_error;   break;
        }
        if (!visible) continue;
        out += e.timestamp + "  ";
        out += severity_prefix(e.severity);
        out += e.message + "\n";
    }
    return out;
}

void gui_log_draw_panel(GuiLogState *ls)
{
    if (!ls->show_log) return;
    if (!ImGui::Begin("Log Console", &ls->show_log)) {
        ImGui::End();
        return;
    }

    /* ---- Toolbar ---- */
    if (ImGui::Checkbox("Info", &ls->filter_info))    { /* refilter */ }
    ImGui::SameLine();
    if (ImGui::Checkbox("Warning", &ls->filter_warning)) { /* refilter */ }
    ImGui::SameLine();
    if (ImGui::Checkbox("Error",   &ls->filter_error))   { /* refilter */ }
    ImGui::SameLine();

    if (ImGui::Button("Clear")) {
        std::lock_guard<std::mutex> lock(ls->mutex);
        ls->entries.clear();
    }
    ImGui::SameLine();

    /* ---- Copy to clipboard ---- */
    if (ImGui::Button("Copy")) {
        std::lock_guard<std::mutex> lock(ls->mutex);
        std::string text = log_build_text(ls);
        ImGui::SetClipboardText(text.c_str());
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Copy visible log entries to clipboard");
    ImGui::SameLine();

    /* ---- Export to file ---- */
    if (ImGui::Button("Export...")) {
        ls->show_export_dialog = true;
        ls->export_status.clear();
        if (ls->export_path[0] == '\0')
            strncpy(ls->export_path, "openjp3d_log.txt",
                    sizeof(ls->export_path) - 1);
    }

    ImGui::Separator();

    /* ---- Scrollable log area ---- */
    ImGui::BeginChild("LogScroll", ImVec2(0, 0), ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);
    {
        std::lock_guard<std::mutex> lock(ls->mutex);
        for (const auto &entry : ls->entries) {
            bool visible = false;
            switch (entry.severity) {
            case GUI_LOG_INFO:    visible = ls->filter_info;    break;
            case GUI_LOG_WARNING: visible = ls->filter_warning; break;
            case GUI_LOG_ERROR:   visible = ls->filter_error;   break;
            }
            if (!visible) continue;

            ImGui::PushStyleColor(ImGuiCol_Text,
                                  severity_colour(entry.severity));
            ImGui::TextUnformatted(
                (entry.timestamp + "  " +
                 severity_prefix(entry.severity) +
                 entry.message).c_str());
            ImGui::PopStyleColor();
        }
    }
    if (ls->scroll_to_bottom) {
        ImGui::SetScrollHereY(1.0f);
        ls->scroll_to_bottom = false;
    }
    ImGui::EndChild();

    ImGui::End();

    /* ---- Export dialog ---- */
    if (ls->show_export_dialog) {
        ImGui::SetNextWindowSize(ImVec2(480, 140), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Export Log", &ls->show_export_dialog,
                         ImGuiWindowFlags_NoCollapse)) {
            ImGui::Text("Save log to file:");
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##export_path", ls->export_path,
                             sizeof(ls->export_path));
            ImGui::Spacing();
            if (ImGui::Button("Save", ImVec2(80, 0))) {
                std::lock_guard<std::mutex> lock(ls->mutex);
                std::string text = log_build_text(ls);
                FILE *f = fopen(ls->export_path, "w");
                if (f) {
                    fwrite(text.c_str(), 1, text.size(), f);
                    fclose(f);
                    ls->export_status = std::string("Saved to: ") +
                                        ls->export_path;
                    ls->show_export_dialog = false;
                } else {
                    ls->export_status = "Error: could not write file.";
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(80, 0)))
                ls->show_export_dialog = false;

            if (!ls->export_status.empty()) {
                ImGui::Spacing();
                ImGui::TextWrapped("%s", ls->export_status.c_str());
            }
        }
        ImGui::End();
    }
}

/* ================================================================== */
/*  INI file utilities                                                */
/* ================================================================== */

/** Create a directory (single level, no parents). */
static void make_dir(const char *path)
{
#if defined(_WIN32)
    CreateDirectoryA(path, NULL);
#else
    mkdir(path, 0755);
#endif
}

/** Create path's parent directory if needed. */
static void ensure_parent_dir(const char *path)
{
    /* Find last directory separator */
    const char *last = strrchr(path, '/');
#if defined(_WIN32)
    const char *last2 = strrchr(path, '\\');
    if (last2 && (!last || last2 > last)) last = last2;
#endif
    if (!last) return;

    char dir[768];
    size_t len = (size_t)(last - path);
    if (len == 0 || len >= sizeof(dir)) return;
    memcpy(dir, path, len);
    dir[len] = '\0';
    make_dir(dir);
}

/* ---- Minimal INI writer ---- */
static void ini_write_str(FILE *f, const char *key, const char *val)
{
    fprintf(f, "%s=%s\n", key, val);
}

static void ini_write_int(FILE *f, const char *key, int val)
{
    fprintf(f, "%s=%d\n", key, val);
}

static void ini_write_float(FILE *f, const char *key, float val)
{
    fprintf(f, "%s=%.6f\n", key, (double)val);
}

static void ini_write_bool(FILE *f, const char *key, bool val)
{
    fprintf(f, "%s=%d\n", key, val ? 1 : 0);
}

/* ---- Minimal INI reader ---- */
struct IniMap {
    std::string section;
    std::string key;
    std::string value;
};

static std::vector<IniMap> ini_parse(const char *path)
{
    std::vector<IniMap> result;
    FILE *f = fopen(path, "r");
    if (!f) return result;

    char line[512];
    std::string cur_section;

    while (fgets(line, sizeof(line), f)) {
        /* Strip trailing newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';

        /* Skip blank lines and comments */
        const char *p = line;
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == '\0' || *p == ';' || *p == '#') continue;

        /* Section */
        if (*p == '[') {
            const char *end = strchr(p+1, ']');
            if (end) {
                cur_section = std::string(p+1, end);
            }
            continue;
        }

        /* Key=value */
        const char *eq = strchr(p, '=');
        if (!eq) continue;

        std::string key(p, eq);
        std::string value(eq + 1);

        /* Trim whitespace from key */
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t'))
            key.pop_back();

        IniMap entry;
        entry.section = cur_section;
        entry.key     = key;
        entry.value   = value;
        result.push_back(entry);
    }
    fclose(f);
    return result;
}

static const std::string *ini_get(const std::vector<IniMap> &map,
                                  const char *section, const char *key)
{
    for (const auto &e : map) {
        if (e.section == section && e.key == key)
            return &e.value;
    }
    return NULL;
}

static int ini_get_int(const std::vector<IniMap> &map,
                       const char *section, const char *key, int def)
{
    const std::string *v = ini_get(map, section, key);
    if (!v) return def;
    return atoi(v->c_str());
}

static float ini_get_float(const std::vector<IniMap> &map,
                            const char *section, const char *key, float def)
{
    const std::string *v = ini_get(map, section, key);
    if (!v) return def;
    return (float)atof(v->c_str());
}

static bool ini_get_bool(const std::vector<IniMap> &map,
                         const char *section, const char *key, bool def)
{
    const std::string *v = ini_get(map, section, key);
    if (!v) return def;
    return atoi(v->c_str()) != 0;
}

static std::string ini_get_str(const std::vector<IniMap> &map,
                                const char *section, const char *key,
                                const char *def)
{
    const std::string *v = ini_get(map, section, key);
    if (!v) return std::string(def);
    return *v;
}

/* ================================================================== */
/*  8F.2 — Preferences                                                */
/* ================================================================== */

void gui_prefs_state_init(GuiPrefsState *ps)
{
    ps->show_prefs          = false;
    ps->default_open_dir[0] = '\0';
    ps->default_output_dir[0] = '\0';
    ps->theme               = 0;  /* Dark */
    ps->bg_r                = 0.12f;
    ps->bg_g                = 0.12f;
    ps->bg_b                = 0.12f;
    ps->threads             = 1;
    ps->dirty               = false;
    ps->config_path[0]      = '\0';

    /* Encoder defaults */
    GuiEncoderPreset &ep = ps->enc_preset;
    ep.tile_w      = 0;
    ep.tile_h      = 0;
    ep.tile_d      = 0;
    ep.dwt_levels  = 3;
    ep.target_rate = 0.0f;
    ep.filter      = OPJ_JP3D_FILTER_53;
    ep.htj2k       = false;
    ep.threads     = 1;
}

void gui_prefs_resolve_config_path(GuiPrefsState *ps)
{
#if defined(_WIN32)
    char appdata[512] = "";
    DWORD ret = GetEnvironmentVariableA("APPDATA", appdata, sizeof(appdata));
    if (ret == 0 || ret >= sizeof(appdata)) {
        strncpy(appdata, ".", sizeof(appdata) - 1);
    }
    snprintf(ps->config_path, sizeof(ps->config_path),
             "%s\\openjp3d\\gui.ini", appdata);
#elif defined(__APPLE__)
    const char *home = getenv("HOME");
    if (!home) home = ".";
    snprintf(ps->config_path, sizeof(ps->config_path),
             "%s/Library/Preferences/openjp3d-gui.ini", home);
#else
    /* XDG Base Directory: $XDG_CONFIG_HOME or ~/.config */
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg) {
        snprintf(ps->config_path, sizeof(ps->config_path),
                 "%s/openjp3d/gui.ini", xdg);
    } else {
        const char *home = getenv("HOME");
        if (!home) home = ".";
        snprintf(ps->config_path, sizeof(ps->config_path),
                 "%s/.config/openjp3d/gui.ini", home);
    }
#endif
}

void gui_prefs_load(GuiPrefsState *ps)
{
    if (ps->config_path[0] == '\0')
        gui_prefs_resolve_config_path(ps);

    std::vector<IniMap> map = ini_parse(ps->config_path);
    if (map.empty()) return;  /* File not found or empty — use defaults */

    /* [preferences] */
    std::string od = ini_get_str(map, "preferences", "default_open_dir", "");
    strncpy(ps->default_open_dir, od.c_str(),
            sizeof(ps->default_open_dir) - 1);

    std::string odir = ini_get_str(map, "preferences", "default_output_dir", "");
    strncpy(ps->default_output_dir, odir.c_str(),
            sizeof(ps->default_output_dir) - 1);

    ps->theme   = ini_get_int(map, "preferences", "theme",   0);
    ps->threads = ini_get_int(map, "preferences", "threads", 1);

    /* [viewer] */
    ps->bg_r = ini_get_float(map, "viewer", "bg_r", 0.12f);
    ps->bg_g = ini_get_float(map, "viewer", "bg_g", 0.12f);
    ps->bg_b = ini_get_float(map, "viewer", "bg_b", 0.12f);

    /* [encoder] */
    GuiEncoderPreset &ep = ps->enc_preset;
    ep.tile_w      = ini_get_int  (map, "encoder", "tile_w",      0);
    ep.tile_h      = ini_get_int  (map, "encoder", "tile_h",      0);
    ep.tile_d      = ini_get_int  (map, "encoder", "tile_d",      0);
    ep.dwt_levels  = ini_get_int  (map, "encoder", "dwt_levels",  3);
    ep.target_rate = ini_get_float(map, "encoder", "target_rate", 0.0f);
    ep.filter      = ini_get_int  (map, "encoder", "filter",      OPJ_JP3D_FILTER_53);
    ep.htj2k       = ini_get_bool (map, "encoder", "htj2k",       false);
    ep.threads     = ini_get_int  (map, "encoder", "threads",     1);

    ps->dirty = false;
}

void gui_prefs_save(GuiPrefsState *ps)
{
    if (ps->config_path[0] == '\0')
        gui_prefs_resolve_config_path(ps);

    ensure_parent_dir(ps->config_path);

    FILE *f = fopen(ps->config_path, "w");
    if (!f) return;

    fprintf(f, "# OpenJP3D GUI preferences\n");
    fprintf(f, "# Auto-generated — edit with care.\n\n");

    fprintf(f, "[preferences]\n");
    ini_write_str(f, "default_open_dir",   ps->default_open_dir);
    ini_write_str(f, "default_output_dir", ps->default_output_dir);
    ini_write_int(f, "theme",              ps->theme);
    ini_write_int(f, "threads",            ps->threads);
    fprintf(f, "\n");

    fprintf(f, "[viewer]\n");
    ini_write_float(f, "bg_r", ps->bg_r);
    ini_write_float(f, "bg_g", ps->bg_g);
    ini_write_float(f, "bg_b", ps->bg_b);
    fprintf(f, "\n");

    fprintf(f, "[encoder]\n");
    GuiEncoderPreset &ep = ps->enc_preset;
    ini_write_int  (f, "tile_w",      ep.tile_w);
    ini_write_int  (f, "tile_h",      ep.tile_h);
    ini_write_int  (f, "tile_d",      ep.tile_d);
    ini_write_int  (f, "dwt_levels",  ep.dwt_levels);
    ini_write_float(f, "target_rate", ep.target_rate);
    ini_write_int  (f, "filter",      ep.filter);
    ini_write_bool (f, "htj2k",       ep.htj2k);
    ini_write_int  (f, "threads",     ep.threads);
    fprintf(f, "\n");

    fclose(f);
    ps->dirty = false;
}

void gui_prefs_draw(GuiPrefsState *ps)
{
    if (!ps->show_prefs) return;

    ImGui::SetNextWindowSize(ImVec2(560, 480), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Preferences", &ps->show_prefs,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    bool changed = false;

    /* ---- Appearance ---- */
    if (ImGui::CollapsingHeader("Appearance", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char *themes[] = { "Dark", "Light" };
        int old_theme = ps->theme;
        if (ImGui::Combo("Theme", &ps->theme, themes, 2)) {
            if (ps->theme != old_theme) {
                opj_gui_apply_theme(ps->theme == 0 ? OPJ_GUI_THEME_DARK
                                                   : OPJ_GUI_THEME_LIGHT);
                changed = true;
            }
        }
        ImGui::Spacing();
        ImGui::Text("Viewport background:");
        if (ImGui::ColorEdit3("##bg_color", &ps->bg_r))
            changed = true;
        ImGui::Spacing();
    }

    /* ---- File Paths ---- */
    if (ImGui::CollapsingHeader("File Paths", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Default open directory:");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##open_dir", ps->default_open_dir,
                             sizeof(ps->default_open_dir)))
            changed = true;

        ImGui::Spacing();
        ImGui::Text("Default output directory:");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##out_dir", ps->default_output_dir,
                             sizeof(ps->default_output_dir)))
            changed = true;
        ImGui::Spacing();
    }

    /* ---- Performance ---- */
    if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::SliderInt("Thread count", &ps->threads, 1, 16))
            changed = true;
        ImGui::Spacing();
    }

    /* ---- Default Encoder Preset ---- */
    if (ImGui::CollapsingHeader("Default Encoder Preset")) {
        GuiEncoderPreset &ep = ps->enc_preset;
        ImGui::PushItemWidth(120);

        ImGui::Text("Tile size  (0 = whole volume):");
        if (ImGui::InputInt("W##tw", &ep.tile_w)) changed = true;
        ImGui::SameLine();
        if (ImGui::InputInt("H##th", &ep.tile_h)) changed = true;
        ImGui::SameLine();
        if (ImGui::InputInt("D##td", &ep.tile_d)) changed = true;
        if (ep.tile_w < 0) ep.tile_w = 0;
        if (ep.tile_h < 0) ep.tile_h = 0;
        if (ep.tile_d < 0) ep.tile_d = 0;

        if (ImGui::SliderInt("DWT levels", &ep.dwt_levels, 0, 8))
            changed = true;

        if (ImGui::SliderFloat("Target rate (0=lossless)",
                               &ep.target_rate, 0.0f, 10.0f))
            changed = true;

        const char *filters[] = { "5/3 (lossless)", "9/7 (lossy)" };
        if (ImGui::Combo("Filter", &ep.filter, filters, 2))
            changed = true;

        if (ImGui::Checkbox("HTJ2K mode", &ep.htj2k))
            changed = true;

        if (ImGui::SliderInt("Threads##enc", &ep.threads, 1, 16))
            changed = true;

        ImGui::PopItemWidth();
        ImGui::Spacing();
    }

    /* ---- Config file path (read-only info) ---- */
    if (ImGui::CollapsingHeader("Configuration File")) {
        ImGui::TextWrapped("Settings are stored in:");
        ImGui::PushStyleColor(ImGuiCol_Text,
                              ImVec4(0.7f, 0.9f, 1.0f, 1.0f));
        ImGui::TextWrapped("%s", ps->config_path[0] ? ps->config_path
                                                    : "(not yet resolved)");
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    ImGui::Separator();
    ImGui::Spacing();

    /* ---- Save / Cancel buttons ---- */
    bool save_btn = ImGui::Button("Save", ImVec2(80, 0));
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(80, 0))) {
        /* Reload from disk to discard in-memory changes */
        gui_prefs_load(ps);
        ps->show_prefs = false;
        changed = false;
    }

    if (save_btn) {
        gui_prefs_save(ps);
        ps->show_prefs = false;
    }

    if (changed && !save_btn)
        ps->dirty = true;

    ImGui::End();
}

/* ================================================================== */
/*  8F.4 — Keyboard shortcuts                                         */
/* ================================================================== */

/** Human-readable names for each action. */
static const char *s_action_names[GUI_ACTION_COUNT] = {
    "Open Volume",       /* GUI_ACTION_OPEN         */
    "Encode",            /* GUI_ACTION_ENCODE        */
    "Decode",            /* GUI_ACTION_DECODE        */
    "Next Slice",        /* GUI_ACTION_NEXT_SLICE    */
    "Previous Slice",    /* GUI_ACTION_PREV_SLICE    */
    "Zoom In",           /* GUI_ACTION_ZOOM_IN       */
    "Zoom Out",          /* GUI_ACTION_ZOOM_OUT      */
    "Toggle Theme",      /* GUI_ACTION_TOGGLE_THEME  */
    "Quit"               /* GUI_ACTION_QUIT          */
};

/** Default key bindings. */
static const ImGuiKey s_default_keys[GUI_ACTION_COUNT] = {
    ImGuiKey_O,           /* Open         */
    ImGuiKey_E,           /* Encode       */
    ImGuiKey_D,           /* Decode       */
    ImGuiKey_RightArrow,  /* Next Slice   */
    ImGuiKey_LeftArrow,   /* Prev Slice   */
    ImGuiKey_Equal,       /* Zoom In      */
    ImGuiKey_Minus,       /* Zoom Out     */
    ImGuiKey_T,           /* Toggle Theme */
    ImGuiKey_Q            /* Quit         */
};

static const bool s_default_ctrl[GUI_ACTION_COUNT] = {
    true,  /* Open         Ctrl+O */
    false, /* Encode       E      */
    false, /* Decode       D      */
    false, /* Next Slice   Right  */
    false, /* Prev Slice   Left   */
    false, /* Zoom In      =      */
    false, /* Zoom Out     -      */
    true,  /* Toggle Theme Ctrl+T */
    true   /* Quit         Ctrl+Q */
};

void gui_shortcuts_state_init(GuiShortcutsState *ss)
{
    ss->show_shortcuts = false;
    for (int i = 0; i < GUI_ACTION_COUNT; ++i) {
        ss->shortcuts[i].action_name = s_action_names[i];
        ss->shortcuts[i].key         = s_default_keys[i];
        ss->shortcuts[i].ctrl        = s_default_ctrl[i];
        ss->shortcuts[i].shift       = false;
        ss->shortcuts[i].alt         = false;
        ss->shortcuts[i].editing     = false;
    }
}

/* ---- INI section for shortcuts ---- */
static const char *s_shortcut_section = "shortcuts";

/** Format a shortcut as "Ctrl+Shift+Alt+KeyName". */
static std::string shortcut_to_string(const GuiShortcut &sc)
{
    std::string s;
    if (sc.ctrl)  s += "Ctrl+";
    if (sc.shift) s += "Shift+";
    if (sc.alt)   s += "Alt+";
    s += ImGui::GetKeyName(sc.key);
    return s;
}

void gui_shortcuts_load(GuiShortcutsState *ss, const char *config_path)
{
    if (!config_path || !config_path[0]) return;
    std::vector<IniMap> map = ini_parse(config_path);
    if (map.empty()) return;

    for (int i = 0; i < GUI_ACTION_COUNT; ++i) {
        /* Key: action index as string, e.g. "action_0" */
        char key[32];
        snprintf(key, sizeof(key), "action_%d_key", i);
        char ctrl_key[32], shift_key[32], alt_key[32];
        snprintf(ctrl_key,  sizeof(ctrl_key),  "action_%d_ctrl",  i);
        snprintf(shift_key, sizeof(shift_key), "action_%d_shift", i);
        snprintf(alt_key,   sizeof(alt_key),   "action_%d_alt",   i);

        int k = ini_get_int(map, s_shortcut_section, key, (int)s_default_keys[i]);
        ss->shortcuts[i].key   = (ImGuiKey)k;
        ss->shortcuts[i].ctrl  = ini_get_bool(map, s_shortcut_section, ctrl_key,
                                              s_default_ctrl[i]);
        ss->shortcuts[i].shift = ini_get_bool(map, s_shortcut_section, shift_key,
                                              false);
        ss->shortcuts[i].alt   = ini_get_bool(map, s_shortcut_section, alt_key,
                                              false);
    }
}

void gui_shortcuts_save(GuiShortcutsState *ss, const char *config_path)
{
    if (!config_path || !config_path[0]) return;

    /* Read existing file, merge shortcuts section, then rewrite.
     * Simpler: just append/overwrite the shortcuts section at the end. */
    /* Read existing content (minus any [shortcuts] section). */
    std::ifstream in(config_path);
    std::ostringstream buf;
    if (in.is_open()) {
        std::string line;
        bool in_shortcuts = false;
        while (std::getline(in, line)) {
            /* Skip existing shortcuts section */
            if (line.rfind("[shortcuts]", 0) == 0) {
                in_shortcuts = true;
                continue;
            }
            if (in_shortcuts && !line.empty() && line[0] == '[') {
                in_shortcuts = false;
            }
            if (!in_shortcuts) buf << line << "\n";
        }
        in.close();
    }

    /* Append updated shortcuts section */
    buf << "\n[shortcuts]\n";
    for (int i = 0; i < GUI_ACTION_COUNT; ++i) {
        char key[32], ctrl_k[32], shift_k[32], alt_k[32];
        snprintf(key,     sizeof(key),     "action_%d_key",   i);
        snprintf(ctrl_k,  sizeof(ctrl_k),  "action_%d_ctrl",  i);
        snprintf(shift_k, sizeof(shift_k), "action_%d_shift", i);
        snprintf(alt_k,   sizeof(alt_k),   "action_%d_alt",   i);
        buf << key     << "=" << (int)ss->shortcuts[i].key   << "\n";
        buf << ctrl_k  << "=" << (ss->shortcuts[i].ctrl  ? 1 : 0) << "\n";
        buf << shift_k << "=" << (ss->shortcuts[i].shift ? 1 : 0) << "\n";
        buf << alt_k   << "=" << (ss->shortcuts[i].alt   ? 1 : 0) << "\n";
    }

    ensure_parent_dir(config_path);
    std::ofstream out(config_path);
    if (out.is_open()) {
        out << buf.str();
    }
}

bool gui_shortcuts_check(const GuiShortcutsState *ss, GuiActionId id)
{
    if (id < 0 || id >= GUI_ACTION_COUNT) return false;
    const GuiShortcut &sc = ss->shortcuts[id];
    const ImGuiIO &io = ImGui::GetIO();

    /* Modifier check */
    bool ctrl_ok  = (sc.ctrl  == io.KeyCtrl);
    bool shift_ok = (sc.shift == io.KeyShift);
    bool alt_ok   = (sc.alt   == io.KeyAlt);

    if (!ctrl_ok || !shift_ok || !alt_ok) return false;
    return ImGui::IsKeyPressed(sc.key, false);
}

void gui_shortcuts_draw(GuiShortcutsState *ss)
{
    if (!ss->show_shortcuts) return;

    ImGui::SetNextWindowSize(ImVec2(500, 380), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Keyboard Shortcuts", &ss->show_shortcuts,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    ImGui::TextWrapped(
        "Click a shortcut's current binding to reassign it, "
        "then press the desired key combination.");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    /* Check if any shortcut is waiting for input */
    bool any_editing = false;
    for (int i = 0; i < GUI_ACTION_COUNT; ++i) {
        if (ss->shortcuts[i].editing) {
            any_editing = true;
            break;
        }
    }

    /* Capture keypress for the shortcut being edited */
    if (any_editing) {
        const ImGuiIO &io = ImGui::GetIO();
        for (ImGuiKey k = ImGuiKey_NamedKey_BEGIN;
             k < ImGuiKey_NamedKey_END;
             k = (ImGuiKey)(k + 1)) {
            if (k == ImGuiKey_Escape) {
                /* Cancel editing */
                for (int i = 0; i < GUI_ACTION_COUNT; ++i)
                    ss->shortcuts[i].editing = false;
                break;
            }
            if (ImGui::IsKeyPressed(k, false)) {
                for (int i = 0; i < GUI_ACTION_COUNT; ++i) {
                    if (ss->shortcuts[i].editing) {
                        ss->shortcuts[i].key   = k;
                        ss->shortcuts[i].ctrl  = io.KeyCtrl;
                        ss->shortcuts[i].shift = io.KeyShift;
                        ss->shortcuts[i].alt   = io.KeyAlt;
                        ss->shortcuts[i].editing = false;
                    }
                }
                break;
            }
        }
    }

    /* Table */
    if (ImGui::BeginTable("shortcuts_table", 2,
                          ImGuiTableFlags_BordersOuter |
                          ImGuiTableFlags_BordersInnerH |
                          ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Action",  ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Binding", ImGuiTableColumnFlags_WidthFixed, 160.0f);
        ImGui::TableHeadersRow();

        for (int i = 0; i < GUI_ACTION_COUNT; ++i) {
            GuiShortcut &sc = ss->shortcuts[i];
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(sc.action_name);

            ImGui::TableSetColumnIndex(1);
            std::string binding = sc.editing
                ? "[Press key...]"
                : shortcut_to_string(sc);

            char btn_id[64];
            snprintf(btn_id, sizeof(btn_id), "%s##sc%d", binding.c_str(), i);

            if (sc.editing) {
                ImGui::PushStyleColor(ImGuiCol_Button,
                    ImVec4(0.6f, 0.3f, 0.1f, 1.0f));
            }
            if (ImGui::Button(btn_id, ImVec2(-1, 0))) {
                /* Start editing this shortcut */
                for (int j = 0; j < GUI_ACTION_COUNT; ++j)
                    ss->shortcuts[j].editing = false;
                sc.editing = true;
            }
            if (sc.editing) ImGui::PopStyleColor();
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
    if (ImGui::Button("Reset to Defaults", ImVec2(140, 0))) {
        gui_shortcuts_state_init(ss);
    }

    ImGui::End();
}
