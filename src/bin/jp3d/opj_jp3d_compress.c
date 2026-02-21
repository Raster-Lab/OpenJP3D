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
 * @file opj_jp3d_compress.c
 * @brief CLI tool to encode a raw binary volume into a JP3D codestream.
 */

#include "openjp3d.h"
#include "opj_raw_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_help(const char *prog)
{
    fprintf(stdout,
        "Usage: %s [options] -i <input.raw> -o <output.jp3d>\n"
        "\n"
        "Encode a raw binary volume into a JP3D codestream.\n"
        "\n"
        "Required:\n"
        "  -i <file>          Input raw binary volume file.\n"
        "  -o <file>          Output JP3D codestream file.\n"
        "  -W <w>             Volume width  (X samples).\n"
        "  -H <h>             Volume height (Y samples).\n"
        "  -D <d>             Volume depth  (Z slices).\n"
        "\n"
        "Options:\n"
        "  -p <prec>          Bit depth per sample (default: 8).\n"
        "  -s                 Signed samples (default: unsigned).\n"
        "  -c <numcomps>      Number of components (default: 1).\n"
        "  -t <tw,th,td>      Tile size (default: whole volume).\n"
        "  -n <rx,ry,rz>      Decomposition levels per axis (default: 3,3,3).\n"
        "  -r <rate>          Target bits/sample (0 = lossless, default: 0).\n"
        "  -f <53|97>         Filter: 53 (lossless) or 97 (lossy) (default: 53).\n"
        "  -H2K               Enable HTJ2K high-throughput block coder.\n"
        "  -v                 Verbose output.\n"
        "  -h, --help         Show this help message.\n"
        "  --version          Show version information.\n"
        "\n"
        "Example:\n"
        "  %s -i volume.raw -o volume.jp3d -W 256 -H 256 -D 128 -p 16\n",
        prog, prog);
}

static void print_version(void)
{
    fprintf(stdout, "opj_jp3d_compress %s\n", opj_jp3d_get_version());
}

static void msg_callback(opj_jp3d_msg_level_t level, const char *msg,
                          void *data)
{
    (void)data;
    const char *prefix = "INFO";
    if (level == OPJ_JP3D_MSG_WARNING) prefix = "WARN";
    else if (level == OPJ_JP3D_MSG_ERROR) prefix = "ERROR";
    fprintf(stderr, "[%s] %s\n", prefix, msg);
}

static int parse_triple(const char *s, uint32_t *a, uint32_t *b, uint32_t *c)
{
    unsigned long x, y, z;
    char *end;
    x = strtoul(s, &end, 10);
    if (*end != ',') return 0;
    y = strtoul(end + 1, &end, 10);
    if (*end != ',') return 0;
    z = strtoul(end + 1, &end, 10);
    if (*end != '\0') return 0;
    *a = (uint32_t)x;
    *b = (uint32_t)y;
    *c = (uint32_t)z;
    return 1;
}

int main(int argc, char *argv[])
{
    const char *infile = NULL;
    const char *outfile = NULL;
    uint32_t width = 0, height = 0, depth = 0;
    uint32_t prec = 8;
    int32_t  sgnd = 0;
    uint32_t numcomps = 1;
    int      verbose = 0;
    int      use_htj2k = 0;

    opj_jp3d_enc_params_t enc;
    opj_jp3d_set_default_encoder_parameters(&enc);

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
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            outfile = argv[++i];
        } else if (strcmp(argv[i], "-W") == 0 && i + 1 < argc) {
            width = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "-H") == 0 && i + 1 < argc) {
            height = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "-D") == 0 && i + 1 < argc) {
            depth = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            prec = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "-s") == 0) {
            sgnd = 1;
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            numcomps = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            i++;
            if (!parse_triple(argv[i], &enc.tile_width, &enc.tile_height,
                              &enc.tile_depth)) {
                fprintf(stderr, "Error: invalid tile size '%s'\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            i++;
            if (!parse_triple(argv[i], &enc.num_resolutions_x,
                              &enc.num_resolutions_y,
                              &enc.num_resolutions_z)) {
                fprintf(stderr, "Error: invalid decomposition levels '%s'\n",
                        argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            enc.target_rate = (float)strtod(argv[++i], NULL);
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            i++;
            if (strcmp(argv[i], "53") == 0) {
                enc.filter = OPJ_JP3D_FILTER_53;
            } else if (strcmp(argv[i], "97") == 0) {
                enc.filter = OPJ_JP3D_FILTER_97;
            } else {
                fprintf(stderr, "Error: unknown filter '%s'\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "-H2K") == 0) {
            use_htj2k = 1;
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = 1;
        } else {
            fprintf(stderr, "Error: unknown option '%s'\n", argv[i]);
            print_help(argv[0]);
            return 1;
        }
    }

    if (!infile || !outfile || width == 0 || height == 0 || depth == 0) {
        fprintf(stderr,
                "Error: -i, -o, -W, -H, -D are required.\n");
        print_help(argv[0]);
        return 1;
    }

    enc.verbose = verbose;
    if (use_htj2k) {
        enc.use_htj2k = OPJ_JP3D_USE_HTJ2K;
    }

    /* Create volume */
    opj_volume_t *vol = opj_jp3d_create_volume(numcomps, width, height, depth,
                                                prec, sgnd);
    if (!vol) {
        fprintf(stderr, "Error: failed to allocate volume.\n");
        return 1;
    }

    /* Read raw data */
    if (!opj_raw_io_read(vol, infile)) {
        fprintf(stderr, "Error: failed to read '%s'.\n", infile);
        opj_jp3d_destroy_volume(vol);
        return 1;
    }

    if (verbose) {
        fprintf(stderr, "Input: %ux%ux%u, %u-bit %s, %u component(s)\n",
                width, height, depth, prec, sgnd ? "signed" : "unsigned",
                numcomps);
    }

    /* Encode */
    uint8_t *cs = NULL;
    size_t cs_sz = 0;
    opj_jp3d_bool_t ok = opj_jp3d_encode(vol, &enc, &cs, &cs_sz,
                                          verbose ? msg_callback : NULL, NULL);
    opj_jp3d_destroy_volume(vol);

    if (!ok) {
        fprintf(stderr, "Error: encoding failed.\n");
        return 1;
    }

    if (verbose) {
        fprintf(stderr, "Encoded %zu bytes.\n", cs_sz);
    }

    /* Write output */
    FILE *fp = fopen(outfile, "wb");
    if (!fp) {
        fprintf(stderr, "Error: cannot open '%s' for writing.\n", outfile);
        opj_jp3d_free(cs);
        return 1;
    }
    if (fwrite(cs, 1, cs_sz, fp) != cs_sz) {
        fprintf(stderr, "Error: write failed.\n");
        fclose(fp);
        opj_jp3d_free(cs);
        return 1;
    }
    fclose(fp);
    opj_jp3d_free(cs);

    if (verbose) {
        fprintf(stderr, "Written to '%s'.\n", outfile);
    }

    return 0;
}
