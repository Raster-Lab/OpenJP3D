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
 * @file opj_dwt3d_simd.h
 * @brief SIMD-accelerated 5/3 lifting DWT function declarations.
 *
 * Each platform-specific translation unit (opj_dwt3d_sse41.c,
 * opj_dwt3d_avx2.c, opj_dwt3d_neon.c) provides forward and inverse
 * 1-D 5/3 integer lifting transforms that are drop-in replacements for
 * the scalar dwt53_fwd_1d / dwt53_inv_1d functions.  Output is
 * guaranteed bit-identical to the scalar reference implementation.
 *
 * Only opj_dwt3d.c and the platform translation units should include
 * this header.
 */

#ifndef OPJ_DWT3D_SIMD_H
#define OPJ_DWT3D_SIMD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * SSE4.1 variants (x86-64, compiled with -msse4.1)
 * ---------------------------------------------------------------------- */
#ifdef OPJ_JP3D_HAVE_SSE41
/**
 * @brief SSE4.1-accelerated forward 5/3 1-D lifting transform.
 * @param x       In/out: @p n interleaved samples → [low | high] non-interleaved.
 * @param n       Number of samples.
 * @param scratch Scratch buffer of at least @p n int32_t elements.
 */
void opj_dwt53_fwd_1d_sse41(int32_t *x, uint32_t n, int32_t *scratch);

/** @brief SSE4.1-accelerated inverse 5/3 1-D lifting transform. */
void opj_dwt53_inv_1d_sse41(int32_t *x, uint32_t n, int32_t *scratch);
#endif /* OPJ_JP3D_HAVE_SSE41 */

/* -------------------------------------------------------------------------
 * AVX2 variants (x86-64, compiled with -mavx2)
 * ---------------------------------------------------------------------- */
#ifdef OPJ_JP3D_HAVE_AVX2
/**
 * @brief AVX2-accelerated forward 5/3 1-D lifting transform.
 * @param x       In/out: @p n interleaved samples → [low | high] non-interleaved.
 * @param n       Number of samples.
 * @param scratch Scratch buffer of at least @p n int32_t elements.
 */
void opj_dwt53_fwd_1d_avx2(int32_t *x, uint32_t n, int32_t *scratch);

/** @brief AVX2-accelerated inverse 5/3 1-D lifting transform. */
void opj_dwt53_inv_1d_avx2(int32_t *x, uint32_t n, int32_t *scratch);
#endif /* OPJ_JP3D_HAVE_AVX2 */

/* -------------------------------------------------------------------------
 * NEON variants (AArch64, compiled with default AArch64 flags)
 * ---------------------------------------------------------------------- */
#ifdef OPJ_JP3D_HAVE_NEON
/**
 * @brief NEON-accelerated forward 5/3 1-D lifting transform.
 * @param x       In/out: @p n interleaved samples → [low | high] non-interleaved.
 * @param n       Number of samples.
 * @param scratch Scratch buffer of at least @p n int32_t elements.
 */
void opj_dwt53_fwd_1d_neon(int32_t *x, uint32_t n, int32_t *scratch);

/** @brief NEON-accelerated inverse 5/3 1-D lifting transform. */
void opj_dwt53_inv_1d_neon(int32_t *x, uint32_t n, int32_t *scratch);
#endif /* OPJ_JP3D_HAVE_NEON */

#ifdef __cplusplus
}
#endif

#endif /* OPJ_DWT3D_SIMD_H */
