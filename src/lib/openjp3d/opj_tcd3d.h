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
 * @file opj_tcd3d.h
 * @brief Tile compression/decompression driver for JP3D.
 */

#ifndef OPJ_TCD3D_H
#define OPJ_TCD3D_H

#include "opj_cs3d.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Encode one tile: apply DWT, split into code-blocks, encode each,
 *        and write a tier-2 packet into @p out.
 *
 * @param tile_data  Input samples (tw*th*td, z-major).
 * @param tw         Tile width.
 * @param th         Tile height.
 * @param td         Tile depth.
 * @param cblk_w     Code-block width.
 * @param cblk_h     Code-block height.
 * @param cblk_d     Code-block depth.
 * @param nx         DWT levels along X.
 * @param ny         DWT levels along Y.
 * @param nz         DWT levels along Z.
 * @param filter     OPJ_JP3D_FILTER_53 or OPJ_JP3D_FILTER_97.
 * @param use_htj2k  Non-zero to use the HT block coder instead of EBCOT.
 * @param out        Destination buffer.
 * @return 1 on success, 0 on failure.
 */
int opj_tcd3d_encode_tile(
    const int32_t *tile_data,
    uint32_t tw, uint32_t th, uint32_t td,
    uint32_t cblk_w, uint32_t cblk_h, uint32_t cblk_d,
    uint32_t nx, uint32_t ny, uint32_t nz,
    int32_t filter,
    int32_t use_htj2k,
    opj_buf_t *out);

/**
 * @brief Decode one tile: parse tier-2 packet, decode code-blocks, apply
 *        inverse DWT, write samples to @p out_data.
 *
 * @param tile_data  Encoded tile byte stream.
 * @param tile_size  Size of @p tile_data.
 * @param out_data   Output samples (tw*th*td, z-major).
 * @param tw         Tile width.
 * @param th         Tile height.
 * @param td         Tile depth.
 * @param cblk_w     Code-block width.
 * @param cblk_h     Code-block height.
 * @param cblk_d     Code-block depth.
 * @param nx         DWT levels along X.
 * @param ny         DWT levels along Y.
 * @param nz         DWT levels along Z.
 * @param filter     OPJ_JP3D_FILTER_53 or OPJ_JP3D_FILTER_97.
 * @param use_htj2k  Non-zero to use the HT block coder instead of EBCOT.
 * @return 1 on success, 0 on failure.
 */
int opj_tcd3d_decode_tile(
    const uint8_t *tile_data, size_t tile_size,
    int32_t *out_data,
    uint32_t tw, uint32_t th, uint32_t td,
    uint32_t cblk_w, uint32_t cblk_h, uint32_t cblk_d,
    uint32_t nx, uint32_t ny, uint32_t nz,
    int32_t filter,
    int32_t use_htj2k);

#ifdef __cplusplus
}
#endif

#endif /* OPJ_TCD3D_H */
