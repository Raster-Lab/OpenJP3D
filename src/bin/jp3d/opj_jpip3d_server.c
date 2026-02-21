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
 * @file opj_jpip3d_server.c
 * @brief Standalone JPIP server CLI for JP3D datasets.
 *
 * This is a minimal demonstration server that loads a JP3D dataset,
 * accepts JPIP 3-D requests from stdin (one URL query per line),
 * and writes responses to stdout.
 */

#include "openjpip3d.h"
#include "openjp3d.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_help(const char *prog)
{
    fprintf(stdout,
        "Usage: %s [options] -d <name> -f <file.jp3d>\n"
        "\n"
        "Standalone JPIP server for JP3D datasets.\n"
        "\n"
        "Reads JPIP 3-D query strings from stdin (one per line) and\n"
        "writes responses to stdout.\n"
        "\n"
        "Required:\n"
        "  -d <name>          Dataset name.\n"
        "  -f <file>          JP3D codestream file to serve.\n"
        "\n"
        "Options:\n"
        "  -v                 Verbose output.\n"
        "  -h, --help         Show this help message.\n"
        "  --version          Show version information.\n"
        "\n"
        "Example:\n"
        "  echo 'dataset=vol&fsiz3d=256,256,128&roff3d=0,0,0&rsiz3d=64,64,32' \\\n"
        "    | %s -d vol -f volume.jp3d\n",
        prog, prog);
}

static void print_version(void)
{
    fprintf(stdout, "opj_jpip3d_server %s\n", opj_jpip3d_get_version());
}

int main(int argc, char *argv[])
{
    const char *dataset_name = NULL;
    const char *filepath = NULL;
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
        if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            dataset_name = argv[++i];
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            filepath = argv[++i];
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = 1;
        } else {
            fprintf(stderr, "Error: unknown option '%s'\n", argv[i]);
            print_help(argv[0]);
            return 1;
        }
    }

    if (!dataset_name || !filepath) {
        fprintf(stderr, "Error: -d and -f are required.\n");
        print_help(argv[0]);
        return 1;
    }

    /* Create server */
    opj_jpip3d_server_t *server = opj_jpip3d_server_create();
    if (!server) {
        fprintf(stderr, "Error: failed to create JPIP server.\n");
        return 1;
    }

    /* Load dataset */
    if (!opj_jpip3d_server_load_dataset(server, dataset_name, filepath)) {
        fprintf(stderr, "Error: failed to load dataset '%s' from '%s'.\n",
                dataset_name, filepath);
        opj_jpip3d_server_destroy(server);
        return 1;
    }

    if (verbose) {
        fprintf(stderr, "Loaded dataset '%s' from '%s'.\n",
                dataset_name, filepath);
        fprintf(stderr, "Waiting for requests on stdin...\n");
    }

    /* Process requests from stdin */
    char line[4096];
    int req_count = 0;
    while (fgets(line, sizeof(line), stdin)) {
        /* Strip trailing newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (len == 0) continue;

        opj_jpip3d_request_t req;
        memset(&req, 0, sizeof(req));

        if (!opj_jpip3d_parse_request(line, &req)) {
            fprintf(stderr, "Error: failed to parse request: '%s'\n", line);
            continue;
        }

        opj_jpip3d_response_t resp;
        memset(&resp, 0, sizeof(resp));

        if (!opj_jpip3d_server_handle_request(server, &req, &resp)) {
            fprintf(stderr, "Error: request handling failed.\n");
            continue;
        }

        if (resp.data && resp.size > 0) {
            fwrite(resp.data, 1, resp.size, stdout);
            fflush(stdout);
        }

        if (verbose) {
            fprintf(stderr, "Request %d: %zu bytes response.\n",
                    ++req_count, resp.size);
        }

        if (resp.data) opj_jp3d_free(resp.data);
        if (resp.metadata) opj_jpip3d_metadata_destroy(resp.metadata);
    }

    opj_jpip3d_server_destroy(server);

    if (verbose) {
        fprintf(stderr, "Server shutdown. Processed %d request(s).\n",
                req_count);
    }

    return 0;
}
