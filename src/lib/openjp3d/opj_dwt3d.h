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
 * @file opj_dwt3d.h
 * @brief 3-D separable DWT (5/3 lossless and 9/7 lossy) for OpenJP3D.
 */

#ifndef OPJ_DWT3D_H
#define OPJ_DWT3D_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Filter type: lossless 5/3 integer lifting. */
#define OPJ_JP3D_FILTER_53  0
/** @brief Filter type: lossy 9/7 floating-point lifting. */
#define OPJ_JP3D_FILTER_97  1

/**
 * @brief Forward 3-D separable DWT, in-place.
 *
 * @param data   Array of @p w * @p h * @p d samples (z-major order).
 * @param w      Width  (X dimension).
 * @param h      Height (Y dimension).
 * @param d      Depth  (Z dimension).
 * @param nx     Number of decomposition levels along X (0 = identity).
 * @param ny     Number of decomposition levels along Y.
 * @param nz     Number of decomposition levels along Z.
 * @param filter OPJ_JP3D_FILTER_53 or OPJ_JP3D_FILTER_97.
 * @return 1 on success, 0 on memory error.
 */
int opj_dwt3d_fwd(int32_t *data,
                  uint32_t w, uint32_t h, uint32_t d,
                  uint32_t nx, uint32_t ny, uint32_t nz,
                  int32_t filter);

/**
 * @brief Inverse 3-D separable DWT, in-place.
 *
 * @param data   Array of @p w * @p h * @p d samples (z-major order).
 * @param w      Width  (X dimension).
 * @param h      Height (Y dimension).
 * @param d      Depth  (Z dimension).
 * @param nx     Number of decomposition levels along X.
 * @param ny     Number of decomposition levels along Y.
 * @param nz     Number of decomposition levels along Z.
 * @param filter OPJ_JP3D_FILTER_53 or OPJ_JP3D_FILTER_97.
 * @return 1 on success, 0 on memory error.
 */
int opj_dwt3d_inv(int32_t *data,
                  uint32_t w, uint32_t h, uint32_t d,
                  uint32_t nx, uint32_t ny, uint32_t nz,
                  int32_t filter);

#ifdef __cplusplus
}
#endif

#endif /* OPJ_DWT3D_H */
