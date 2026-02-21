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
 * @file opj_t1_3d.h
 * @brief Tier-1 bit-plane encoder/decoder for JP3D code-blocks.
 */

#ifndef OPJ_T1_3D_H
#define OPJ_T1_3D_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Encode a code-block into a byte buffer.
 *
 * @param data    Input samples (w*h*d, z-major order).
 * @param w       Code-block width.
 * @param h       Code-block height.
 * @param d       Code-block depth.
 * @param out_buf Output byte buffer.
 * @param out_cap Capacity of @p out_buf.
 * @return Number of bytes written, or 0 on failure.
 */
size_t opj_t1_3d_encode_cblk(const int32_t *data,
                               uint32_t w, uint32_t h, uint32_t d,
                               uint8_t *out_buf, size_t out_cap);

/**
 * @brief Decode a code-block from a byte buffer.
 *
 * @param in_buf  Input byte buffer.
 * @param in_size Size of @p in_buf.
 * @param data    Output samples (w*h*d, z-major order).
 * @param w       Code-block width.
 * @param h       Code-block height.
 * @param d       Code-block depth.
 * @return 1 on success, 0 on failure.
 */
int opj_t1_3d_decode_cblk(const uint8_t *in_buf, size_t in_size,
                            int32_t *data,
                            uint32_t w, uint32_t h, uint32_t d);

#ifdef __cplusplus
}
#endif

#endif /* OPJ_T1_3D_H */
