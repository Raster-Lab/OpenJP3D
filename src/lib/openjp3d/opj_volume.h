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
 * @file opj_volume.h
 * @brief Volume data structures and lifecycle functions.
 */

#ifndef OPJ_VOLUME_H
#define OPJ_VOLUME_H

#include <stdint.h>
#include "openjp3d.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Allocate and initialise a volume with @p numcomps components,
 *        each of size @p w x @p h x @p d with @p prec bits, signed flag
 *        @p sgnd.
 * @return Pointer to the new volume, or NULL on allocation failure.
 */
opj_volume_t *opj_jp3d_create_volume(uint32_t numcomps,
                                      uint32_t w, uint32_t h, uint32_t d,
                                      uint32_t prec, int32_t sgnd);

/**
 * @brief Free a volume and all its component data.
 */
void opj_jp3d_destroy_volume(opj_volume_t *vol);

#ifdef __cplusplus
}
#endif

#endif /* OPJ_VOLUME_H */
