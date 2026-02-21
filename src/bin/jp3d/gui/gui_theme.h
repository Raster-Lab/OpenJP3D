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
 * @file gui_theme.h
 * @brief Theme management for the OpenJP3D GUI application.
 *
 * Provides light and dark theme presets with accessible colour contrast.
 */

#ifndef OPJ_JP3D_GUI_THEME_H
#define OPJ_JP3D_GUI_THEME_H

/** @brief Available GUI themes. */
enum opj_gui_theme {
    OPJ_GUI_THEME_DARK = 0,  /**< Dark theme (default). */
    OPJ_GUI_THEME_LIGHT = 1  /**< Light theme. */
};

/**
 * @brief Apply the specified theme to the Dear ImGui style.
 *
 * Configures colours, rounding, and spacing for the selected theme.
 * Both themes are designed to meet WCAG 2.1 AA contrast requirements.
 *
 * @param theme  The theme to apply.
 */
void opj_gui_apply_theme(enum opj_gui_theme theme);

/**
 * @brief Get the currently active theme.
 *
 * @return The active theme enum value.
 */
enum opj_gui_theme opj_gui_get_current_theme(void);

/**
 * @brief Toggle between dark and light themes.
 *
 * @return The newly active theme.
 */
enum opj_gui_theme opj_gui_toggle_theme(void);

#endif /* OPJ_JP3D_GUI_THEME_H */
