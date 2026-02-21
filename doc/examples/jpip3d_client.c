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
 * @file jpip3d_client.c
 * @brief Example: JPIP 3-D client/server session with metadata.
 *
 * This program demonstrates a full JPIP 3-D workflow: creating a server,
 * loading a dataset, opening a client session, attaching XML metadata,
 * and requesting a volumetric region.
 *
 * Build (assuming OpenJP3D is installed):
 * @code
 *   cc -o jpip3d_client jpip3d_client.c -lopenjpip3d -lopenjp3d
 * @endcode
 */

#include "openjp3d.h"
#include "openjpip3d.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** @brief Volume dimensions. */
#define VOL_W 32
#define VOL_H 32
#define VOL_D 8

int main(void)
{
    printf("OpenJP3D jpip3d_client example (version %s)\n",
           opj_jpip3d_get_version());

    /* ---- Create and encode a test volume ------------------------------- */

    opj_volume_t *vol = opj_jp3d_create_volume(1, VOL_W, VOL_H, VOL_D, 8, 0);
    if (!vol) {
        fprintf(stderr, "Failed to create volume\n");
        return EXIT_FAILURE;
    }
    for (uint32_t i = 0; i < VOL_W * VOL_H * VOL_D; i++)
        vol->comps[0].data[i] = (int32_t)(i & 0xFF);

    opj_jp3d_enc_params_t enc;
    opj_jp3d_set_default_encoder_parameters(&enc);

    uint8_t *cs = NULL;
    size_t   cs_size = 0;
    if (!opj_jp3d_encode(vol, &enc, &cs, &cs_size, NULL, NULL)) {
        fprintf(stderr, "Encoding failed\n");
        opj_jp3d_destroy_volume(vol);
        return EXIT_FAILURE;
    }
    opj_jp3d_destroy_volume(vol);

    /* ---- Server setup -------------------------------------------------- */

    opj_jpip3d_server_t *srv = opj_jpip3d_server_create();
    opj_jpip3d_server_load_dataset_mem(srv, "brain_ct", cs, cs_size);

    /* ---- Metadata ------------------------------------------------------ */

    opj_jpip3d_metadata_t *meta = opj_jpip3d_metadata_create();
    opj_jpip3d_metadata_set_xml(meta,
        "<dataset><modality>CT</modality>"
        "<description>Brain CT scan</description></dataset>");
    printf("Metadata XML: %s\n", opj_jpip3d_metadata_get_xml(meta));

    /* Serialise metadata to a JP2 box and back. */
    uint8_t *box = NULL;
    size_t   box_size = 0;
    opj_jpip3d_metadata_to_box(meta, &box, &box_size);
    printf("Metadata box: %zu bytes\n", box_size);

    opj_jpip3d_metadata_t *meta2 = opj_jpip3d_metadata_create();
    opj_jpip3d_metadata_from_box(meta2, box, box_size);
    printf("Round-tripped XML: %s\n", opj_jpip3d_metadata_get_xml(meta2));

    opj_jp3d_free(box);
    opj_jpip3d_metadata_destroy(meta);
    opj_jpip3d_metadata_destroy(meta2);

    /* ---- Client session ------------------------------------------------ */

    opj_jpip3d_session_t *sess = opj_jpip3d_session_open(srv);

    /* Build a request using the URL-based API. */
    const char *query =
        "dataset=brain_ct&fsiz3d=32,32,8&roff3d=0,0,0&rsiz3d=16,16,4";
    opj_jpip3d_request_t req;
    memset(&req, 0, sizeof(req));
    if (!opj_jpip3d_parse_request(query, &req)) {
        fprintf(stderr, "Request parsing failed\n");
    }

    /* Serialise back to a URL string. */
    char url_buf[512];
    int url_len = opj_jpip3d_request_to_url_params(&req, url_buf,
                                                    sizeof(url_buf));
    if (url_len > 0)
        printf("Serialised request: %s\n", url_buf);

    /* Send the request and receive a sub-volume. */
    opj_volume_t *sub = opj_jpip3d_session_receive_volume(sess, &req);
    if (sub) {
        printf("Received: %u x %u x %u, %u-bit\n",
               sub->comps[0].w, sub->comps[0].h,
               sub->comps[0].d, sub->comps[0].prec);
        opj_jp3d_destroy_volume(sub);
    }

    /* ---- Cleanup ------------------------------------------------------- */

    opj_jpip3d_session_close(sess);
    opj_jp3d_free(cs);
    opj_jpip3d_server_destroy(srv);

    printf("Done.\n");
    return EXIT_SUCCESS;
}
