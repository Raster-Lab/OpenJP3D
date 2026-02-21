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
 * @file opj_cpu.h
 * @brief Runtime CPU feature detection for OpenJP3D SIMD dispatch.
 *
 * Call opj_cpu_features() once at startup (or on first use) to obtain a
 * bitmask of available ISA extensions.  The result is cached internally so
 * repeated calls are cheap.
 */

#ifndef OPJ_CPU_H
#define OPJ_CPU_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ==========================================================================
 *   CPU feature flags
 * ==========================================================================
 */

/** @brief No SIMD extensions detected. */
#define OPJ_CPU_FEATURE_NONE   0x00u

/** @brief SSE2 available (x86-64 baseline — always present on 64-bit x86). */
#define OPJ_CPU_FEATURE_SSE2   0x01u

/** @brief SSE4.1 available (x86-64). */
#define OPJ_CPU_FEATURE_SSE41  0x02u

/** @brief AVX2 available (x86-64). */
#define OPJ_CPU_FEATURE_AVX2   0x04u

/** @brief NEON available (AArch64 — mandatory on 64-bit Arm). */
#define OPJ_CPU_FEATURE_NEON   0x08u

/*
 * ==========================================================================
 *   API
 * ==========================================================================
 */

/**
 * @brief Detect available CPU SIMD features at runtime.
 *
 * On x86-64 the detection uses CPUID.  On AArch64, NEON is always present
 * (it is mandatory in the AArch64 architecture).  The result is computed
 * once and cached; subsequent calls return the cached value immediately.
 *
 * @return Bitmask of OPJ_CPU_FEATURE_* flags.
 */
uint32_t opj_cpu_features(void);

/**
 * @brief Return a human-readable string listing detected CPU features.
 *
 * The returned pointer is to a static buffer; it is safe to call from
 * multiple threads after the first call to opj_cpu_features().
 *
 * @return Null-terminated feature string, e.g. "SSE2 SSE4.1 AVX2".
 */
const char *opj_cpu_features_str(void);

#ifdef __cplusplus
}
#endif

#endif /* OPJ_CPU_H */
