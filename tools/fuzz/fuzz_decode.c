/*
 * Copyright (c) 2024-2026, OpenJP3D Contributors
 * All rights reserved.
 * SPDX-License-Identifier: BSD-2-Clause
 */

/**
 * @file fuzz_decode.c
 * @brief libFuzzer / AFL++ harness for the JP3D decoder.
 *
 * Build with:
 *   clang -fsanitize=fuzzer,address -O1 -g \
 *       -I src/lib/openjp3d fuzz_decode.c \
 *       -L build -lopenjp3d -lm -o fuzz_decode
 *
 * Run:
 *   ./fuzz_decode corpus_dir/
 */

#include "openjp3d.h"
#include <stdint.h>
#include <stddef.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* Attempt to decode arbitrary data; the decoder must not crash. */
    opj_volume_t *vol = opj_jp3d_decode(data, size, NULL, NULL, NULL);
    if (vol) {
        opj_jp3d_destroy_volume(vol);
    }
    return 0;
}
