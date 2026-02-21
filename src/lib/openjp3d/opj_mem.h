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
 * @file opj_mem.h
 * @brief Memory allocation helpers for OpenJP3D.
 */

#ifndef OPJ_MEM_H
#define OPJ_MEM_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Allocate @p size bytes; returns NULL on failure. */
void *opj_jp3d_malloc(size_t size);

/** @brief Allocate @p n * @p size zeroed bytes; returns NULL on failure. */
void *opj_jp3d_calloc(size_t n, size_t size);

/** @brief Resize allocation at @p ptr to @p size bytes; returns NULL on failure. */
void *opj_jp3d_realloc(void *ptr, size_t size);

/** @brief Free memory allocated by opj_jp3d_malloc / calloc / realloc. */
void opj_jp3d_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* OPJ_MEM_H */
