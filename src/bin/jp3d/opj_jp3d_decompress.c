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
 * @file opj_jp3d_decompress.c
 * @brief CLI tool to decode a JP3D codestream into a raw binary volume.
 */

#include "openjp3d.h"
#include "opj_raw_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_help(const char *prog)
{
    fprintf(stdout,
        "Usage: %s [options] -i <input.jp3d> -o <output.raw>\n"
        "\n"
        "Decode a JP3D codestream into a raw binary volume.\n"
        "\n"
        "Required:\n"
        "  -i <file>          Input JP3D codestream file.\n"
        "  -o <file>          Output raw binary volume file.\n"
        "\n"
        "Options:\n"
        "  -v                 Verbose output.\n"
        "  -h, --help         Show this help message.\n"
        "  --version          Show version information.\n"
        "\n"
        "Example:\n"
        "  %s -i volume.jp3d -o volume.raw\n",
        prog, prog);
}

static void print_version(void)
{
    fprintf(stdout, "opj_jp3d_decompress %s\n", opj_jp3d_get_version());
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

int main(int argc, char *argv[])
{
    const char *infile = NULL;
    const char *outfile = NULL;
    int verbose = 0;

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
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = 1;
        } else {
            fprintf(stderr, "Error: unknown option '%s'\n", argv[i]);
            print_help(argv[0]);
            return 1;
        }
    }

    if (!infile || !outfile) {
        fprintf(stderr, "Error: -i and -o are required.\n");
        print_help(argv[0]);
        return 1;
    }

    /* Read input codestream */
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

    size_t cs_sz = (size_t)flen;
    uint8_t *cs = (uint8_t *)opj_jp3d_malloc(cs_sz);
    if (!cs) {
        fprintf(stderr, "Error: out of memory.\n");
        fclose(fp);
        return 1;
    }
    if (fread(cs, 1, cs_sz, fp) != cs_sz) {
        fprintf(stderr, "Error: failed to read '%s'.\n", infile);
        fclose(fp);
        opj_jp3d_free(cs);
        return 1;
    }
    fclose(fp);

    if (verbose) {
        fprintf(stderr, "Read %zu bytes from '%s'.\n", cs_sz, infile);
    }

    /* Decode */
    opj_jp3d_dec_params_t dec;
    opj_jp3d_set_default_decoder_parameters(&dec);
    dec.verbose = verbose;

    opj_volume_t *vol = opj_jp3d_decode(cs, cs_sz, &dec,
                                         verbose ? msg_callback : NULL, NULL);
    opj_jp3d_free(cs);

    if (!vol) {
        fprintf(stderr, "Error: decoding failed.\n");
        return 1;
    }

    if (verbose && vol->numcomps > 0) {
        fprintf(stderr, "Decoded: %ux%ux%u, %u-bit %s, %u component(s)\n",
                vol->comps[0].w, vol->comps[0].h, vol->comps[0].d,
                vol->comps[0].prec,
                vol->comps[0].sgnd ? "signed" : "unsigned",
                vol->numcomps);
    }

    /* Write raw output */
    if (!opj_raw_io_write(vol, outfile)) {
        fprintf(stderr, "Error: failed to write '%s'.\n", outfile);
        opj_jp3d_destroy_volume(vol);
        return 1;
    }

    if (verbose) {
        fprintf(stderr, "Written to '%s'.\n", outfile);
    }

    opj_jp3d_destroy_volume(vol);
    return 0;
}
