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
 * @file openjp3d.h
 * @brief OpenJP3D — JPEG 2000 Part 10 (JP3D) volumetric codec API.
 *
 * This header defines the public API for the OpenJP3D library, providing
 * lossless and lossy compression of 3-D image volumes conforming to
 * ISO/IEC 15444-10.
 */

#ifndef OPENJP3D_H
#define OPENJP3D_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ==========================================================================
 *   Version information
 * ==========================================================================
 */

/** @brief Major version number. */
#define OPJ_JP3D_VERSION_MAJOR 0

/** @brief Minor version number. */
#define OPJ_JP3D_VERSION_MINOR 1

/** @brief Patch version number. */
#define OPJ_JP3D_VERSION_PATCH 0

/** @brief Full version string. */
#define OPJ_JP3D_VERSION "0.1.0"

/*
 * ==========================================================================
 *   Shared library export macros
 * ==========================================================================
 */

#if defined(_WIN32) || defined(__CYGWIN__)
    #ifdef OPJ_JP3D_BUILDING_DLL
        #define OPJ_JP3D_API __declspec(dllexport)
    #elif defined(OPJ_JP3D_USING_DLL)
        #define OPJ_JP3D_API __declspec(dllimport)
    #else
        #define OPJ_JP3D_API
    #endif
#elif defined(__GNUC__) && __GNUC__ >= 4
    #define OPJ_JP3D_API __attribute__((visibility("default")))
#else
    #define OPJ_JP3D_API
#endif

/*
 * ==========================================================================
 *   Basic types
 * ==========================================================================
 */

/** @brief Boolean type for the JP3D API. */
typedef int32_t opj_jp3d_bool_t;

/** @brief True value. */
#define OPJ_JP3D_TRUE  1

/** @brief False value. */
#define OPJ_JP3D_FALSE 0

/*
 * ==========================================================================
 *   Error handling
 * ==========================================================================
 */

/**
 * @brief Severity levels for event messages.
 */
typedef enum {
    OPJ_JP3D_MSG_INFO    = 0, /**< Informational message. */
    OPJ_JP3D_MSG_WARNING = 1, /**< Warning message. */
    OPJ_JP3D_MSG_ERROR   = 2  /**< Error message. */
} opj_jp3d_msg_level_t;

/**
 * @brief Callback function type for event messages.
 *
 * @param level   Severity level of the message.
 * @param message Null-terminated message string.
 * @param data    User-provided context pointer.
 */
typedef void (*opj_jp3d_msg_callback_t)(opj_jp3d_msg_level_t level,
                                        const char *message,
                                        void *data);

/*
 * ==========================================================================
 *   Forward declarations (Phase 1 will define these fully)
 * ==========================================================================
 */

/** @brief Opaque codec handle for JP3D encoding/decoding. */
typedef struct opj_jp3d_codec opj_jp3d_codec_t;

/*
 * ==========================================================================
 *   Library lifecycle
 * ==========================================================================
 */

/**
 * @brief Return the library version string.
 *
 * @return Null-terminated version string (e.g. "0.1.0").
 */
OPJ_JP3D_API const char *opj_jp3d_get_version(void);

#ifdef __cplusplus
}
#endif

#endif /* OPENJP3D_H */
