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
 * @file opj_raw_io.h
 * @brief Raw binary volume reader/writer.
 */

#ifndef OPJ_RAW_IO_H
#define OPJ_RAW_IO_H

#include "openjp3d.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Write a raw binary volume to @p path.
 *
 * For each component, samples are written in z-major order as
 * little-endian integers: uint8 for prec<=8, uint16 for prec<=16,
 * uint32 for prec<=32 (unsigned), or the signed equivalents.
 *
 * @return 1 on success, 0 on failure.
 */
int opj_raw_io_write(const opj_volume_t *vol, const char *path);

/**
 * @brief Read raw binary data into an existing, pre-allocated volume.
 *
 * @param vol  Volume with allocated comps and data arrays.
 * @param path Path to the raw binary file.
 * @return 1 on success, 0 on failure.
 */
int opj_raw_io_read(opj_volume_t *vol, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* OPJ_RAW_IO_H */
