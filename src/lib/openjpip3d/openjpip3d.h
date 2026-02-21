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
 * @file openjpip3d.h
 * @brief OpenJPIP3D — JPIP volumetric streaming API (JPEG 2000 Part 9).
 *
 * This header defines the public API for the OpenJPIP3D library, extending
 * the JPIP interactive protocol to support streaming of JP3D volumetric
 * datasets.
 */

#ifndef OPENJPIP3D_H
#define OPENJPIP3D_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ==========================================================================
 *   Shared library export macros
 * ==========================================================================
 */

#if defined(_WIN32) || defined(__CYGWIN__)
    #ifdef OPJ_JPIP3D_BUILDING_DLL
        #define OPJ_JPIP3D_API __declspec(dllexport)
    #elif defined(OPJ_JPIP3D_USING_DLL)
        #define OPJ_JPIP3D_API __declspec(dllimport)
    #else
        #define OPJ_JPIP3D_API
    #endif
#elif defined(__GNUC__) && __GNUC__ >= 4
    #define OPJ_JPIP3D_API __attribute__((visibility("default")))
#else
    #define OPJ_JPIP3D_API
#endif

/**
 * @brief Return the JPIP3D library version string.
 *
 * @return Null-terminated version string.
 */
OPJ_JPIP3D_API const char *opj_jpip3d_get_version(void);

#ifdef __cplusplus
}
#endif

#endif /* OPENJPIP3D_H */
