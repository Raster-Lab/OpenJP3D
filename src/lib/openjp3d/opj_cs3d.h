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
 * @file opj_cs3d.h
 * @brief Codestream markers, I/O helpers, and opj_buf_t for OpenJP3D.
 */

#ifndef OPJ_CS3D_H
#define OPJ_CS3D_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Growable byte buffer
 * ---------------------------------------------------------------------- */

/** @brief Growable byte buffer used for building codestreams. */
typedef struct {
    uint8_t *data; /**< Buffer data. */
    size_t   len;  /**< Bytes currently written. */
    size_t   cap;  /**< Allocated capacity. */
} opj_buf_t;

/**
 * @brief Write @p n bytes from @p src into @p b, growing as needed.
 * @return 1 on success, 0 on out-of-memory.
 */
int opj_buf_write(opj_buf_t *b, const uint8_t *src, size_t n);

/**
 * @brief Free the buffer data and zero the struct.
 */
void opj_buf_free(opj_buf_t *b);

/* -------------------------------------------------------------------------
 * Marker definitions
 * ---------------------------------------------------------------------- */

#define OPJ_CS3D_SOC    0xFF4Fu  /**< Start of codestream. */
#define OPJ_CS3D_SIZ3D  0xFF51u  /**< Image/volume size segment. */
#define OPJ_CS3D_COD3D  0xFF52u  /**< Coding style default segment. */
#define OPJ_CS3D_QCD3D  0xFF5Cu  /**< Quantisation default segment. */
#define OPJ_CS3D_SOT    0xFF90u  /**< Start of tile-part. */
#define OPJ_CS3D_SOD    0xFF93u  /**< Start of data. */
#define OPJ_CS3D_EOC    0xFFD9u  /**< End of codestream. */

/* -------------------------------------------------------------------------
 * Write helpers (big-endian)
 * ---------------------------------------------------------------------- */

void opj_cs3d_write_u8 (opj_buf_t *b, uint8_t  v);
void opj_cs3d_write_u16(opj_buf_t *b, uint16_t v);
void opj_cs3d_write_u32(opj_buf_t *b, uint32_t v);
/** Write a float as IEEE 754 big-endian uint32. */
void opj_cs3d_write_f32(opj_buf_t *b, float v);

/* -------------------------------------------------------------------------
 * Read helpers
 * ---------------------------------------------------------------------- */

uint8_t  opj_cs3d_read_u8 (const uint8_t *d, size_t *pos, size_t maxlen, int *err);
uint16_t opj_cs3d_read_u16(const uint8_t *d, size_t *pos, size_t maxlen, int *err);
uint32_t opj_cs3d_read_u32(const uint8_t *d, size_t *pos, size_t maxlen, int *err);
float    opj_cs3d_read_f32 (const uint8_t *d, size_t *pos, size_t maxlen, int *err);

#ifdef __cplusplus
}
#endif

#endif /* OPJ_CS3D_H */
