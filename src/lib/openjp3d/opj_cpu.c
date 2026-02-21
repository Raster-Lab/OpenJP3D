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
 * @file opj_cpu.c
 * @brief Runtime CPU feature detection implementation.
 *
 * x86-64: uses the __get_cpuid / __get_cpuid_count GCC/Clang built-ins
 *         (or inline CPUID on MSVC) to test SSE2, SSE4.1, and AVX2.
 * AArch64: NEON is mandatory per the architecture spec, so the flag is
 *          always set.
 * All other targets: returns OPJ_CPU_FEATURE_NONE.
 */

#include "opj_cpu.h"

#include <stdio.h>
#include <string.h>

/* =========================================================================
 * Platform-specific CPUID helpers
 * ========================================================================= */

#if defined(__x86_64__) || defined(_M_X64) || \
    defined(__i386__)   || defined(_M_IX86)

#  if defined(_MSC_VER)
#    include <intrin.h>
static void opj_cpuid(uint32_t leaf, uint32_t subleaf,
                       uint32_t *eax, uint32_t *ebx,
                       uint32_t *ecx, uint32_t *edx)
{
    int regs[4];
    __cpuidex(regs, (int)leaf, (int)subleaf);
    *eax = (uint32_t)regs[0];
    *ebx = (uint32_t)regs[1];
    *ecx = (uint32_t)regs[2];
    *edx = (uint32_t)regs[3];
}
#  else
#    include <cpuid.h>
static void opj_cpuid(uint32_t leaf, uint32_t subleaf,
                       uint32_t *eax, uint32_t *ebx,
                       uint32_t *ecx, uint32_t *edx)
{
    __cpuid_count(leaf, subleaf, *eax, *ebx, *ecx, *edx);
}
#  endif /* _MSC_VER */

#  define OPJ_CPU_IS_X86 1
#endif /* x86 */

#if defined(__aarch64__) || defined(_M_ARM64)
#  define OPJ_CPU_IS_AARCH64 1
#endif

/* =========================================================================
 * Feature detection
 * ========================================================================= */

static uint32_t opj_detect_features(void)
{
    uint32_t features = OPJ_CPU_FEATURE_NONE;

#if defined(OPJ_CPU_IS_X86)
    uint32_t eax, ebx, ecx, edx;

    /* CPUID leaf 0: get maximum basic leaf */
    opj_cpuid(0u, 0u, &eax, &ebx, &ecx, &edx);
    uint32_t max_leaf = eax;

    if (max_leaf >= 1u) {
        opj_cpuid(1u, 0u, &eax, &ebx, &ecx, &edx);

        /* SSE2: EDX bit 26 */
        if (edx & (1u << 26))
            features |= OPJ_CPU_FEATURE_SSE2;

        /* SSE4.1: ECX bit 19 */
        if (ecx & (1u << 19))
            features |= OPJ_CPU_FEATURE_SSE41;
    }

    if (max_leaf >= 7u) {
        opj_cpuid(7u, 0u, &eax, &ebx, &ecx, &edx);

        /* AVX2: EBX bit 5 */
        if (ebx & (1u << 5))
            features |= OPJ_CPU_FEATURE_AVX2;
    }

#elif defined(OPJ_CPU_IS_AARCH64)
    /* NEON is mandatory in AArch64 (ARMv8-A baseline). */
    features |= OPJ_CPU_FEATURE_NEON;

#else
    /* Unknown architecture — no SIMD extensions detected. */
    (void)features;
#endif

    return features;
}

/* =========================================================================
 * Public API
 * ========================================================================= */

uint32_t opj_cpu_features(void)
{
    /* Cache after first call.  A benign data race is acceptable here: the
     * detection is idempotent and all threads will arrive at the same value.
     */
    static uint32_t cached = (uint32_t)-1;
    if (cached == (uint32_t)-1)
        cached = opj_detect_features();
    return cached;
}

const char *opj_cpu_features_str(void)
{
    static char buf[64];
    uint32_t f = opj_cpu_features();

    if (f == OPJ_CPU_FEATURE_NONE) {
        memcpy(buf, "none", 5);
        return buf;
    }

    int pos = 0;
    const char *sep = "";
    if (f & OPJ_CPU_FEATURE_SSE2)  { pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "%sSSE2",   sep); sep = " "; }
    if (f & OPJ_CPU_FEATURE_SSE41) { pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "%sSSE4.1", sep); sep = " "; }
    if (f & OPJ_CPU_FEATURE_AVX2)  { pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "%sAVX2",   sep); sep = " "; }
    if (f & OPJ_CPU_FEATURE_NEON)  { pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "%sNEON",   sep); sep = " "; }
    (void)sep;

    return buf;
}
