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
 * @file opj_ht3d.h
 * @brief High-Throughput (HTJ2K-style) block coder for JP3D 3-D code-blocks.
 *
 * Implements a Fast Block Coder with Optimised Truncation (FBCOT) algorithm
 * adapted for 3-D code-blocks, following the structure of ISO/IEC 15444-15
 * (HTJ2K) extended to three dimensions.
 *
 * The coder uses:
 *  - MEL (Minimum Entropy Length) entropy coding for the significance stream.
 *  - MagSgn (Magnitude-Sign) Exp-Golomb variable-length codes for magnitudes
 *    and signs.
 *
 * This replaces the EBCOT Tier-1 coder when @c OPJ_JP3D_USE_HTJ2K is set in
 * the encoder parameters.
 *
 * **Reference:** The MEL coder state machine is modelled on the description
 * in ISO/IEC 15444-15 §7 (HT cleanup pass).  The MagSgn stream uses
 * Exp-Golomb order-0 coding, which is a compatible simplification of the
 * MagSgn byte-aligned stream described in the standard.
 */

#ifndef OPJ_HT3D_H
#define OPJ_HT3D_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Encode a 3-D code-block using the HT block coder.
 *
 * Output layout (all fields big-endian):
 * @verbatim
 *   Offset  Size  Field
 *   0       1     Magic byte: 0x48 ('H') — identifies HT block
 *   1       1     mbps — number of active magnitude bit planes
 *   2       4     w — code-block width
 *   6       4     h — code-block height
 *   10      4     d — code-block depth
 *   14      4     mel_len — byte length of MEL stream
 *   18      mel_len  MEL stream (significance bits)
 *   18+mel_len  …  MagSgn stream (Exp-Golomb magnitudes + sign bits)
 * @endverbatim
 *
 * @param data    Input samples (w*h*d, z-major order).
 * @param w       Code-block width.
 * @param h       Code-block height.
 * @param d       Code-block depth.
 * @param out_buf Output byte buffer.
 * @param out_cap Capacity of @p out_buf.
 * @return Number of bytes written, or 0 on failure.
 */
size_t opj_ht3d_encode_cblk(const int32_t *data,
                              uint32_t w, uint32_t h, uint32_t d,
                              uint8_t *out_buf, size_t out_cap);

/**
 * @brief Decode a 3-D code-block encoded with the HT block coder.
 *
 * @param in_buf  Input byte buffer (HT-encoded code-block).
 * @param in_size Size of @p in_buf in bytes.
 * @param data    Output samples (w*h*d, z-major order).
 * @param w       Expected code-block width.
 * @param h       Expected code-block height.
 * @param d       Expected code-block depth.
 * @return 1 on success, 0 on failure.
 */
int opj_ht3d_decode_cblk(const uint8_t *in_buf, size_t in_size,
                           int32_t *data,
                           uint32_t w, uint32_t h, uint32_t d);

#ifdef __cplusplus
}
#endif

#endif /* OPJ_HT3D_H */
