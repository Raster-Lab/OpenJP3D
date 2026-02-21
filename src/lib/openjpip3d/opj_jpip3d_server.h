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
 * @file opj_jpip3d_server.h
 * @brief Internal header: JPIP 3-D server (dataset manager + request handler).
 */

#ifndef OPJ_JPIP3D_SERVER_H
#define OPJ_JPIP3D_SERVER_H

#include "openjpip3d.h"
#include "opj_jpip3d_cache.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Dataset record
 * ------------------------------------------------------------------------- */

/**
 * @brief A named JP3D dataset held in the server.
 */
typedef struct opj_jpip3d_dataset {
    char     name[256];          /**< Dataset identifier. */
    uint8_t *codestream;         /**< Raw JP3D codestream bytes. */
    size_t   codestream_size;    /**< Byte count of @c codestream. */
    uint32_t width;              /**< Width  of the volume (samples). */
    uint32_t height;             /**< Height of the volume (samples). */
    uint32_t depth;              /**< Depth  of the volume (samples). */
    uint32_t numcomps;           /**< Number of components. */
} opj_jpip3d_dataset_t;

/* -------------------------------------------------------------------------
 * Server struct
 * ------------------------------------------------------------------------- */

/**
 * @brief JPIP 3-D server holding a set of datasets and a shared cache.
 */
typedef struct opj_jpip3d_server {
    opj_jpip3d_dataset_t *datasets;      /**< Dataset array. */
    size_t                num_datasets;  /**< Number of active datasets. */
    size_t                cap_datasets;  /**< Allocated capacity. */
    opj_jpip3d_cache_t   *cache;         /**< Shared precinct-delivery cache. */
} opj_jpip3d_server_t;

/* -------------------------------------------------------------------------
 * Server API
 * ------------------------------------------------------------------------- */

/** @brief Allocate a new server.  Returns NULL on failure. */
opj_jpip3d_server_t *opj_jpip3d_server_create(void);
/** @brief Destroy @p server and all owned resources. */
void                 opj_jpip3d_server_destroy(opj_jpip3d_server_t *server);
/** @brief Load a dataset from a file path. */
int                  opj_jpip3d_server_load_dataset(opj_jpip3d_server_t *server,
                         const char *name, const char *filepath);
/** @brief Load a dataset from a memory buffer (the bytes are copied). */
int                  opj_jpip3d_server_load_dataset_mem(opj_jpip3d_server_t *server,
                         const char *name,
                         const uint8_t *data, size_t size);
/** @brief Handle a JPIP 3-D request; fills @p resp. */
int                  opj_jpip3d_server_handle_request(opj_jpip3d_server_t *server,
                         const opj_jpip3d_request_t *req,
                         opj_jpip3d_response_t *resp);

#ifdef __cplusplus
}
#endif

#endif /* OPJ_JPIP3D_SERVER_H */
