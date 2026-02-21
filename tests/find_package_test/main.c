/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Minimal program that links against the installed OpenJP3D library and
 * prints the version string.  Used by the CI packaging job to validate
 * that find_package(OpenJP3D) works correctly.
 */

#include "openjp3d.h"
#include <stdio.h>

int main(void)
{
    const char *ver = opj_jp3d_get_version();
    if (!ver) return 1;
    printf("OpenJP3D version: %s\n", ver);
    return 0;
}
