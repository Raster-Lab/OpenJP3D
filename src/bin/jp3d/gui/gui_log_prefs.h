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
 * @file gui_log_prefs.h
 * @brief Log console, preferences dialog, and keyboard shortcuts state for
 *        the OpenJP3D GUI application (Phase 8F).
 *
 * Provides:
 *  - GuiLogState:    enhanced log panel with timestamps, severity
 *                    filtering, clipboard copy, and file export (8F.1).
 *  - GuiPrefsState:  persistent preferences stored in a platform-appropriate
 *                    INI file (8F.2).
 *  - GuiShortcutsState: configurable keyboard shortcuts for common
 *                    actions (8F.4).
 *  - gui_log_codec_callback(): opj_jp3d_msg_callback_t implementation that
 *                    forwards codec messages to GuiLogState (8F.1).
 */

#ifndef OPJ_JP3D_GUI_LOG_PREFS_H
#define OPJ_JP3D_GUI_LOG_PREFS_H

#include <stdint.h>
#include <stdbool.h>
#include <mutex>
#include <string>
#include <vector>

extern "C" {
#include "openjp3d.h"
}

#include "imgui.h"

/* ------------------------------------------------------------------ */
/*  8F.1 — Log console                                                */
/* ------------------------------------------------------------------ */

/** Severity levels for log entries (mirrors opj_jp3d_msg_level_t). */
enum GuiLogSeverity {
    GUI_LOG_INFO    = 0, /**< Informational message. */
    GUI_LOG_WARNING = 1, /**< Warning message. */
    GUI_LOG_ERROR   = 2  /**< Error message. */
};

/** A single entry in the GUI log. */
struct GuiLogEntry {
    GuiLogSeverity severity;  /**< Message severity. */
    std::string    timestamp; /**< Wall-clock timestamp (HH:MM:SS). */
    std::string    message;   /**< Message text. */
};

/**
 * @brief State for the enhanced log console panel (8F.1).
 *
 * Thread-safe: gui_log_add() may be called from background threads
 * (e.g., codec callbacks).
 */
struct GuiLogState {
    /* ---- panel ---- */
    bool show_log;           /**< Log panel visibility. */

    /* ---- entries ---- */
    std::vector<GuiLogEntry> entries;   /**< All log entries. */
    std::mutex               mutex;     /**< Guards entries vector. */
    bool                     scroll_to_bottom; /**< Auto-scroll flag. */

    /* ---- filter ---- */
    bool filter_info;    /**< Show INFO messages. */
    bool filter_warning; /**< Show WARNING messages. */
    bool filter_error;   /**< Show ERROR messages. */

    /* ---- export ---- */
    bool show_export_dialog;     /**< Export-to-file dialog visible. */
    char export_path[512];       /**< Destination file path for export. */
    std::string export_status;   /**< Last export status message. */
};

/* ------------------------------------------------------------------ */
/*  8F.2 — Preferences                                                */
/* ------------------------------------------------------------------ */

/** Default encoder parameter preset (persisted in preferences). */
struct GuiEncoderPreset {
    int   tile_w;       /**< Tile width (0 = whole volume). */
    int   tile_h;       /**< Tile height. */
    int   tile_d;       /**< Tile depth. */
    int   dwt_levels;   /**< DWT decomposition levels. */
    float target_rate;  /**< 0 = lossless. */
    int   filter;       /**< OPJ_JP3D_FILTER_53 or _97. */
    bool  htj2k;        /**< HTJ2K mode. */
    int   threads;      /**< Number of threads. */
};

/**
 * @brief Persistent application preferences (8F.2).
 *
 * Loaded from / saved to a platform-appropriate INI file:
 *   Linux:   ~/.config/openjp3d/gui.ini
 *   macOS:   ~/Library/Preferences/openjp3d-gui.ini
 *   Windows: %APPDATA%\openjp3d\gui.ini
 */
struct GuiPrefsState {
    /* ---- panel ---- */
    bool show_prefs;             /**< Preferences dialog visibility. */

    /* ---- file paths ---- */
    char default_open_dir[512];  /**< Default directory for Open dialogs. */
    char default_output_dir[512];/**< Default directory for output files. */

    /* ---- appearance ---- */
    int  theme;                  /**< 0 = Dark, 1 = Light. */
    float bg_r, bg_g, bg_b;     /**< Viewport background colour (0–1). */

    /* ---- performance ---- */
    int  threads;                /**< Default thread count for codec ops. */

    /* ---- encoder defaults ---- */
    GuiEncoderPreset enc_preset; /**< Default encoder parameters. */

    /* ---- internal ---- */
    char config_path[768];       /**< Resolved INI file path. */
    bool dirty;                  /**< True when unsaved changes exist. */
};

/* ------------------------------------------------------------------ */
/*  8F.4 — Keyboard shortcuts                                         */
/* ------------------------------------------------------------------ */

/** Identifiers for configurable GUI actions. */
enum GuiActionId {
    GUI_ACTION_OPEN         = 0, /**< Open Volume. */
    GUI_ACTION_ENCODE       = 1, /**< Open Encode panel. */
    GUI_ACTION_DECODE       = 2, /**< Open Decode panel. */
    GUI_ACTION_NEXT_SLICE   = 3, /**< Advance slice index by 1. */
    GUI_ACTION_PREV_SLICE   = 4, /**< Go back one slice. */
    GUI_ACTION_ZOOM_IN      = 5, /**< Increase viewport zoom. */
    GUI_ACTION_ZOOM_OUT     = 6, /**< Decrease viewport zoom. */
    GUI_ACTION_TOGGLE_THEME = 7, /**< Toggle dark/light theme. */
    GUI_ACTION_QUIT         = 8, /**< Quit the application. */
    GUI_ACTION_COUNT        = 9  /**< Number of configurable actions. */
};

/** A configurable keyboard shortcut binding. */
struct GuiShortcut {
    const char *action_name; /**< Human-readable action label. */
    ImGuiKey    key;         /**< ImGui key code. */
    bool        ctrl;        /**< Require Ctrl modifier. */
    bool        shift;       /**< Require Shift modifier. */
    bool        alt;         /**< Require Alt modifier. */
    bool        editing;     /**< True while waiting for new key input. */
};

/**
 * @brief State for the configurable keyboard shortcuts editor (8F.4).
 */
struct GuiShortcutsState {
    bool         show_shortcuts;                   /**< Dialog visibility. */
    GuiShortcut  shortcuts[GUI_ACTION_COUNT];      /**< All shortcut bindings. */
};

/* ------------------------------------------------------------------ */
/*  Public API — Log console (8F.1)                                   */
/* ------------------------------------------------------------------ */

/** Initialise log state to safe defaults. */
void gui_log_state_init(GuiLogState *ls);

/** Release log state resources. */
void gui_log_state_free(GuiLogState *ls);

/**
 * @brief Append a message to the log.  Thread-safe.
 *
 * @param ls   Log state.
 * @param sev  Severity level.
 * @param fmt  printf-style format string.
 * @param ...  Format arguments.
 */
void gui_log_add(GuiLogState *ls, GuiLogSeverity sev, const char *fmt, ...)
#ifdef __GNUC__
    __attribute__((format(printf, 3, 4)))
#endif
    ;

/** Draw the enhanced log console panel. */
void gui_log_draw_panel(GuiLogState *ls);

/**
 * @brief opj_jp3d_msg_callback_t implementation that routes codec messages
 *        to a GuiLogState.
 *
 * Usage:  pass this function as the @p callback argument to
 *         opj_jp3d_encode() / opj_jp3d_decode(), and pass the
 *         GuiLogState pointer as @p callback_data.
 */
void gui_log_codec_callback(opj_jp3d_msg_level_t level,
                            const char *message,
                            void *data);

/* ------------------------------------------------------------------ */
/*  Public API — Preferences (8F.2)                                   */
/* ------------------------------------------------------------------ */

/** Initialise preferences to built-in defaults (does not load the file). */
void gui_prefs_state_init(GuiPrefsState *ps);

/** Determine the platform-appropriate config file path and store it. */
void gui_prefs_resolve_config_path(GuiPrefsState *ps);

/** Load preferences from the INI file.  No-op if the file does not exist. */
void gui_prefs_load(GuiPrefsState *ps);

/** Save preferences to the INI file, creating directories as needed. */
void gui_prefs_save(GuiPrefsState *ps);

/** Draw the preferences dialog. */
void gui_prefs_draw(GuiPrefsState *ps);

/* ------------------------------------------------------------------ */
/*  Public API — Keyboard shortcuts (8F.4)                            */
/* ------------------------------------------------------------------ */

/** Initialise shortcut bindings to built-in defaults. */
void gui_shortcuts_state_init(GuiShortcutsState *ss);

/** Load shortcut overrides from a GuiPrefsState's config file. */
void gui_shortcuts_load(GuiShortcutsState *ss, const char *config_path);

/** Save shortcut bindings to a GuiPrefsState's config file. */
void gui_shortcuts_save(GuiShortcutsState *ss, const char *config_path);

/**
 * @brief Check whether an action's shortcut is currently pressed.
 *
 * Must be called inside the Dear ImGui frame after NewFrame().
 *
 * @param ss  Shortcuts state.
 * @param id  Action identifier.
 * @return true if the action's key + modifiers are pressed this frame.
 */
bool gui_shortcuts_check(const GuiShortcutsState *ss, GuiActionId id);

/** Draw the keyboard shortcuts editor dialog. */
void gui_shortcuts_draw(GuiShortcutsState *ss);

#endif /* OPJ_JP3D_GUI_LOG_PREFS_H */
