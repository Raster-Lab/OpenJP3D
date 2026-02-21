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
 * @file opj_t2_3d.c
 * @brief Tier-2 packet formation (simple concatenation).
 */

#include "opj_t2_3d.h"
#include "opj_mem.h"

#include <string.h>

int opj_t2_3d_write_packet(opj_buf_t *out,
                            const uint8_t * const *cblk_bufs,
                            const size_t          *cblk_lens,
                            uint32_t               num_cblks)
{
    /* Write count */
    uint8_t hdr[4];
    hdr[0] = (uint8_t)(num_cblks >> 24);
    hdr[1] = (uint8_t)(num_cblks >> 16);
    hdr[2] = (uint8_t)(num_cblks >>  8);
    hdr[3] = (uint8_t)(num_cblks      );
    if (!opj_buf_write(out, hdr, 4))
        return 0;

    for (uint32_t i = 0; i < num_cblks; i++) {
        uint32_t sz = (uint32_t)cblk_lens[i];
        uint8_t szb[4] = {
            (uint8_t)(sz >> 24), (uint8_t)(sz >> 16),
            (uint8_t)(sz >>  8), (uint8_t)(sz      )
        };
        if (!opj_buf_write(out, szb, 4))
            return 0;
        if (sz > 0 && !opj_buf_write(out, cblk_bufs[i], sz))
            return 0;
    }
    return 1;
}

int opj_t2_3d_read_packet(const uint8_t  *in, size_t in_size,
                           uint8_t       ***cblk_bufs_out,
                           size_t         **cblk_lens_out,
                           uint32_t        *num_cblks_out)
{
    if (in_size < 4)
        return 0;

    size_t pos = 0;
    uint32_t num = ((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16) |
                   ((uint32_t)in[2] <<  8) |  (uint32_t)in[3];
    pos += 4;

    uint8_t **bufs = NULL;
    size_t   *lens = NULL;

    if (num > 0) {
        bufs = (uint8_t **)opj_jp3d_calloc(num, sizeof(uint8_t *));
        lens = (size_t   *)opj_jp3d_calloc(num, sizeof(size_t));
        if (!bufs || !lens) {
            opj_jp3d_free(bufs);
            opj_jp3d_free(lens);
            return 0;
        }
    }

    for (uint32_t i = 0; i < num; i++) {
        if (pos + 4 > in_size) goto fail;
        uint32_t sz = ((uint32_t)in[pos] << 24) | ((uint32_t)in[pos+1] << 16) |
                      ((uint32_t)in[pos+2] <<  8) |  (uint32_t)in[pos+3];
        pos += 4;
        if (pos + sz > in_size) goto fail;

        lens[i] = sz;
        if (sz > 0) {
            bufs[i] = (uint8_t *)opj_jp3d_malloc(sz);
            if (!bufs[i]) goto fail;
            memcpy(bufs[i], in + pos, sz);
        }
        pos += sz;
    }

    *cblk_bufs_out = bufs;
    *cblk_lens_out = lens;
    *num_cblks_out = num;
    return 1;

fail:
    if (bufs) {
        for (uint32_t i = 0; i < num; i++)
            opj_jp3d_free(bufs[i]);
        opj_jp3d_free(bufs);
    }
    opj_jp3d_free(lens);
    return 0;
}
