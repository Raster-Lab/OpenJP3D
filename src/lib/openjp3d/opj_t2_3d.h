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
 * @file opj_t2_3d.h
 * @brief Tier-2 packet formation for JP3D (Phase 1: simple concatenation).
 */

#ifndef OPJ_T2_3D_H
#define OPJ_T2_3D_H

#include "opj_cs3d.h"  /* opj_buf_t */
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Write a packet containing @p num_cblks code-block byte-streams.
 *
 * Format:
 *   - 4 bytes: num_cblks (big-endian uint32)
 *   - for each code-block: 4 bytes size + [bytes]
 *
 * @param out       Destination buffer.
 * @param cblk_bufs Array of pointers to code-block byte-streams.
 * @param cblk_lens Array of code-block byte-stream lengths.
 * @param num_cblks Number of code-blocks.
 * @return 1 on success, 0 on failure.
 */
int opj_t2_3d_write_packet(opj_buf_t *out,
                            const uint8_t * const *cblk_bufs,
                            const size_t          *cblk_lens,
                            uint32_t               num_cblks);

/**
 * @brief Read a packet previously written by opj_t2_3d_write_packet.
 *
 * Allocates *cblk_bufs and *cblk_lens; caller must free each cblk_bufs[i]
 * and the two arrays themselves.
 *
 * @param in          Input byte stream.
 * @param in_size     Total size of input.
 * @param cblk_bufs   Output: array of allocated byte buffers.
 * @param cblk_lens   Output: array of lengths.
 * @param num_cblks   Output: number of code-blocks.
 * @return 1 on success, 0 on failure.
 */
int opj_t2_3d_read_packet(const uint8_t  *in, size_t in_size,
                           uint8_t       ***cblk_bufs,
                           size_t         **cblk_lens,
                           uint32_t        *num_cblks);

#ifdef __cplusplus
}
#endif

#endif /* OPJ_T2_3D_H */
