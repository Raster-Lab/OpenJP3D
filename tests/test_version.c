/*
 * Copyright (c) 2024-2026, OpenJP3D Contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

/**
 * @file test_version.c
 * @brief Smoke test — verify opj_jp3d_get_version() returns a valid string.
 */

#include "openjp3d.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    const char *ver = opj_jp3d_get_version();

    if (ver == NULL) {
        fprintf(stderr, "FAIL: opj_jp3d_get_version() returned NULL\n");
        return 1;
    }

    if (strlen(ver) == 0) {
        fprintf(stderr, "FAIL: opj_jp3d_get_version() returned empty string\n");
        return 1;
    }

    if (strcmp(ver, OPJ_JP3D_VERSION) != 0) {
        fprintf(stderr,
                "FAIL: version mismatch: got \"%s\", expected \"%s\"\n",
                ver, OPJ_JP3D_VERSION);
        return 1;
    }

    printf("PASS: opj_jp3d_get_version() = \"%s\"\n", ver);
    return 0;
}
