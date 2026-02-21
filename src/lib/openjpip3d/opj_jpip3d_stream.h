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
 * @file opj_jpip3d_stream.h
 * @brief Internal header: JPT/JPP byte-stream builder for JPIP 3-D.
 */

#ifndef OPJ_JPIP3D_STREAM_H
#define OPJ_JPIP3D_STREAM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Magic bytes identifying stream-part headers
 * ------------------------------------------------------------------------- */

/** @brief Magic prefix for JPT-stream tile-part headers. */
#define OPJ_JPIP3D_JPT_MAGIC "JPT3"
/** @brief Magic prefix for JPP-stream precinct-part headers. */
#define OPJ_JPIP3D_JPP_MAGIC "JPP3"

/* -------------------------------------------------------------------------
 * Opaque stream type (full definition only in the .c file)
 * ------------------------------------------------------------------------- */

/** @brief Opaque growing-buffer stream handle. */
typedef struct opj_jpip3d_stream opj_jpip3d_stream_t;

/* -------------------------------------------------------------------------
 * Stream lifecycle
 * ------------------------------------------------------------------------- */

/** @brief Allocate an empty stream. Returns NULL on allocation failure. */
opj_jpip3d_stream_t *opj_jpip3d_stream_create(void);
/** @brief Destroy @p s and free all memory. */
void                 opj_jpip3d_stream_destroy(opj_jpip3d_stream_t *s);
/** @brief Return a read-only pointer to the accumulated bytes. */
const uint8_t       *opj_jpip3d_stream_get_data(const opj_jpip3d_stream_t *s);
/** @brief Return the number of bytes currently in the stream. */
size_t               opj_jpip3d_stream_get_size(const opj_jpip3d_stream_t *s);

/* -------------------------------------------------------------------------
 * Write helpers
 * ------------------------------------------------------------------------- */

/**
 * @brief Append a JPT-stream tile-part header and data.
 *
 * Wire format (all multi-byte fields big-endian):
 *   "JPT3" (4) | tile_x (4) | tile_y (4) | tile_z (4) |
 *   layer (4)  | res (4)    | comp (4)   | data_len (4) | data (data_len)
 *
 * @return 1 on success, 0 on failure.
 */
int opj_jpip3d_jpt_write_tile_part(opj_jpip3d_stream_t *s,
    uint32_t tile_x,  uint32_t tile_y,  uint32_t tile_z,
    uint32_t layer,   uint32_t res_level, uint32_t comp,
    const uint8_t *data, uint32_t data_len);

/**
 * @brief Append a JPP-stream precinct-part header and data.
 *
 * Wire format (all multi-byte fields big-endian):
 *   "JPP3" (4) | tile_x (4) | tile_y (4) | tile_z (4) |
 *   prec_x (4) | prec_y (4) | prec_z (4) |
 *   layer (4)  | res (4)    | comp (4)   | data_len (4) | data (data_len)
 *
 * @return 1 on success, 0 on failure.
 */
int opj_jpip3d_jpp_write_precinct(opj_jpip3d_stream_t *s,
    uint32_t tile_x,  uint32_t tile_y,  uint32_t tile_z,
    uint32_t prec_x,  uint32_t prec_y,  uint32_t prec_z,
    uint32_t layer,   uint32_t res_level, uint32_t comp,
    const uint8_t *data, uint32_t data_len);

#ifdef __cplusplus
}
#endif

#endif /* OPJ_JPIP3D_STREAM_H */
