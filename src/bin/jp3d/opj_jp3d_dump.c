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
 * @file opj_jp3d_dump.c
 * @brief CLI tool to inspect a JP3D codestream and print marker information.
 */

#include "openjp3d.h"
#include "opj_cs3d.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_help(const char *prog)
{
    fprintf(stdout,
        "Usage: %s [options] -i <input.jp3d>\n"
        "\n"
        "Inspect a JP3D codestream and print marker information.\n"
        "\n"
        "Required:\n"
        "  -i <file>          Input JP3D codestream file.\n"
        "\n"
        "Options:\n"
        "  -h, --help         Show this help message.\n"
        "  --version          Show version information.\n"
        "\n"
        "Example:\n"
        "  %s -i volume.jp3d\n",
        prog, prog);
}

static void print_version(void)
{
    fprintf(stdout, "opj_jp3d_dump %s\n", opj_jp3d_get_version());
}

static const char *marker_name(uint16_t m)
{
    switch (m) {
    case OPJ_CS3D_SOC:   return "SOC";
    case OPJ_CS3D_SIZ3D: return "SIZ3D";
    case OPJ_CS3D_COD3D: return "COD3D";
    case OPJ_CS3D_QCD3D: return "QCD3D";
    case OPJ_CS3D_SOT:   return "SOT";
    case OPJ_CS3D_SOD:   return "SOD";
    case OPJ_CS3D_EOC:   return "EOC";
    default:             return "UNKNOWN";
    }
}

static int dump_codestream(const uint8_t *data, size_t size)
{
    size_t pos = 0;
    int err = 0;
    int tile_count = 0;
    uint32_t numcomps = 0;

    printf("JP3D Codestream Dump (%zu bytes)\n", size);
    printf("========================================\n");

    while (pos + 2 <= size && !err) {
        size_t marker_off = pos;
        uint16_t marker = opj_cs3d_read_u16(data, &pos, size, &err);
        if (err) break;

        printf("\nMarker 0x%04X (%s) at offset %zu\n",
               marker, marker_name(marker), marker_off);

        if (marker == OPJ_CS3D_SOC) {
            printf("  Start of codestream\n");
            continue;
        }

        if (marker == OPJ_CS3D_EOC) {
            printf("  End of codestream\n");
            break;
        }

        if (marker == OPJ_CS3D_SIZ3D) {
            uint32_t x1 = opj_cs3d_read_u32(data, &pos, size, &err);
            uint32_t y1 = opj_cs3d_read_u32(data, &pos, size, &err);
            uint32_t z1 = opj_cs3d_read_u32(data, &pos, size, &err);
            uint32_t x0 = opj_cs3d_read_u32(data, &pos, size, &err);
            uint32_t y0 = opj_cs3d_read_u32(data, &pos, size, &err);
            uint32_t z0 = opj_cs3d_read_u32(data, &pos, size, &err);
            uint32_t tw = opj_cs3d_read_u32(data, &pos, size, &err);
            uint32_t th = opj_cs3d_read_u32(data, &pos, size, &err);
            uint32_t td = opj_cs3d_read_u32(data, &pos, size, &err);
            numcomps     = opj_cs3d_read_u32(data, &pos, size, &err);
            uint32_t cs  = opj_cs3d_read_u32(data, &pos, size, &err);
            if (err) break;

            printf("  Volume extent:  (%u, %u, %u) - (%u, %u, %u)\n",
                   x0, y0, z0, x1, y1, z1);
            printf("  Volume size:    %u x %u x %u\n",
                   x1 - x0, y1 - y0, z1 - z0);
            printf("  Tile size:      %u x %u x %u\n", tw, th, td);
            printf("  Num components: %u\n", numcomps);
            printf("  Colour space:   %u\n", cs);

            for (uint32_t ci = 0; ci < numcomps && !err; ci++) {
                uint32_t cw   = opj_cs3d_read_u32(data, &pos, size, &err);
                uint32_t ch   = opj_cs3d_read_u32(data, &pos, size, &err);
                uint32_t cd   = opj_cs3d_read_u32(data, &pos, size, &err);
                uint32_t prec = opj_cs3d_read_u32(data, &pos, size, &err);
                uint32_t sgnd = opj_cs3d_read_u32(data, &pos, size, &err);
                if (err) break;
                printf("  Component %u:    %ux%ux%u, prec=%u, sgnd=%u\n",
                       ci, cw, ch, cd, prec, sgnd);
            }
            continue;
        }

        if (marker == OPJ_CS3D_COD3D) {
            uint32_t filter = opj_cs3d_read_u32(data, &pos, size, &err);
            uint32_t htj2k  = opj_cs3d_read_u32(data, &pos, size, &err);
            uint32_t rx     = opj_cs3d_read_u32(data, &pos, size, &err);
            uint32_t ry     = opj_cs3d_read_u32(data, &pos, size, &err);
            uint32_t rz     = opj_cs3d_read_u32(data, &pos, size, &err);
            uint32_t cbw    = opj_cs3d_read_u32(data, &pos, size, &err);
            uint32_t cbh    = opj_cs3d_read_u32(data, &pos, size, &err);
            uint32_t cbd    = opj_cs3d_read_u32(data, &pos, size, &err);
            uint32_t layers = opj_cs3d_read_u32(data, &pos, size, &err);
            if (err) break;

            printf("  Filter:         %s (%u)\n",
                   filter == 0 ? "5/3 (lossless)" : "9/7 (lossy)", filter);
            printf("  HTJ2K:          %s\n", htj2k ? "yes" : "no");
            printf("  Resolutions:    %u x %u x %u\n", rx, ry, rz);
            printf("  Code-block:     %u x %u x %u\n", cbw, cbh, cbd);
            printf("  Quality layers: %u\n", layers);
            continue;
        }

        if (marker == OPJ_CS3D_QCD3D) {
            float rate = opj_cs3d_read_f32(data, &pos, size, &err);
            if (err) break;
            if (rate == 0.0f)
                printf("  Target rate:    lossless\n");
            else
                printf("  Target rate:    %.4f bits/sample\n", (double)rate);
            continue;
        }

        if (marker == OPJ_CS3D_SOT) {
            uint32_t tp_idx = opj_cs3d_read_u32(data, &pos, size, &err);
            uint32_t tp_len = opj_cs3d_read_u32(data, &pos, size, &err);
            if (err) break;

            printf("  Tile-part idx:  %u\n", tp_idx);
            printf("  Tile data len:  %u bytes\n", tp_len);
            tile_count++;
            continue;
        }

        if (marker == OPJ_CS3D_SOD) {
            printf("  Start of data\n");
            /* Skip to next marker — scan for SOT or EOC */
            while (pos + 2 <= size) {
                if (data[pos] == 0xFF && data[pos + 1] != 0x00) {
                    uint16_t next = ((uint16_t)data[pos] << 8) | data[pos + 1];
                    if (next == OPJ_CS3D_SOT || next == OPJ_CS3D_EOC) {
                        break;
                    }
                }
                pos++;
            }
            continue;
        }

        /* Unknown marker: skip single u16 worth of data and continue */
        printf("  (unknown marker, skipping)\n");
    }

    printf("\n========================================\n");
    printf("Total tile parts: %d\n", tile_count);
    return err ? 1 : 0;
}

int main(int argc, char *argv[])
{
    const char *infile = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "--version") == 0) {
            print_version();
            return 0;
        }
        if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
            infile = argv[++i];
        } else {
            fprintf(stderr, "Error: unknown option '%s'\n", argv[i]);
            print_help(argv[0]);
            return 1;
        }
    }

    if (!infile) {
        fprintf(stderr, "Error: -i is required.\n");
        print_help(argv[0]);
        return 1;
    }

    /* Read input file */
    FILE *fp = fopen(infile, "rb");
    if (!fp) {
        fprintf(stderr, "Error: cannot open '%s'.\n", infile);
        return 1;
    }
    fseek(fp, 0, SEEK_END);
    long flen = ftell(fp);
    if (flen <= 0) {
        fprintf(stderr, "Error: empty or unreadable file '%s'.\n", infile);
        fclose(fp);
        return 1;
    }
    fseek(fp, 0, SEEK_SET);

    size_t sz = (size_t)flen;
    uint8_t *data = (uint8_t *)opj_jp3d_malloc(sz);
    if (!data) {
        fprintf(stderr, "Error: out of memory.\n");
        fclose(fp);
        return 1;
    }
    if (fread(data, 1, sz, fp) != sz) {
        fprintf(stderr, "Error: failed to read '%s'.\n", infile);
        fclose(fp);
        opj_jp3d_free(data);
        return 1;
    }
    fclose(fp);

    int ret = dump_codestream(data, sz);
    opj_jp3d_free(data);
    return ret;
}
