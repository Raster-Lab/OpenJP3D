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
 * @file openjpip3d.h
 * @brief OpenJPIP3D — JPIP volumetric streaming API (JPEG 2000 Part 9).
 *
 * This header defines the public API for the OpenJPIP3D library, extending
 * the JPIP interactive protocol to support streaming of JP3D volumetric
 * datasets.
 */

#ifndef OPENJPIP3D_H
#define OPENJPIP3D_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ==========================================================================
 *   Shared library export macros
 * ==========================================================================
 */

#if defined(_WIN32) || defined(__CYGWIN__)
    #ifdef OPJ_JPIP3D_BUILDING_DLL
        #define OPJ_JPIP3D_API __declspec(dllexport)
    #elif defined(OPJ_JPIP3D_USING_DLL)
        #define OPJ_JPIP3D_API __declspec(dllimport)
    #else
        #define OPJ_JPIP3D_API
    #endif
#elif defined(__GNUC__) && __GNUC__ >= 4
    #define OPJ_JPIP3D_API __attribute__((visibility("default")))
#else
    #define OPJ_JPIP3D_API
#endif

/* -------------------------------------------------------------------------
 *   openjp3d types (opj_volume_t, memory functions)
 * ------------------------------------------------------------------------- */
#include "openjp3d.h"

/* -------------------------------------------------------------------------
 *   Forward declarations of opaque types
 * ------------------------------------------------------------------------- */

/** @brief Opaque cache handle. */
typedef struct opj_jpip3d_cache    opj_jpip3d_cache_t;
/** @brief Opaque session handle. */
typedef struct opj_jpip3d_session  opj_jpip3d_session_t;
/** @brief Opaque server handle. */
typedef struct opj_jpip3d_server   opj_jpip3d_server_t;
/** @brief Opaque metadata handle. */
typedef struct opj_jpip3d_metadata opj_jpip3d_metadata_t;

/* -------------------------------------------------------------------------
 *   Stream-type constants
 * ------------------------------------------------------------------------- */

/** @brief Raw JP3D codestream. */
#define OPJ_JPIP3D_STREAM_JP3D 0
/** @brief JPT-stream (tile-parts). */
#define OPJ_JPIP3D_STREAM_JPT  1
/** @brief JPP-stream (precinct-parts). */
#define OPJ_JPIP3D_STREAM_JPP  2

/** @brief Maximum dataset name length including NUL terminator. */
#define OPJ_JPIP3D_DATASET_NAME_MAX 256

/* -------------------------------------------------------------------------
 *   Data structures
 * ------------------------------------------------------------------------- */

/**
 * @brief 3-D window-of-interest region.
 */
typedef struct opj_jpip3d_region {
    uint32_t x0; /**< Left   bound (inclusive). */
    uint32_t y0; /**< Top    bound (inclusive). */
    uint32_t z0; /**< Front  bound (inclusive). */
    uint32_t x1; /**< Right  bound (exclusive). */
    uint32_t y1; /**< Bottom bound (exclusive). */
    uint32_t z1; /**< Back   bound (exclusive). */
} opj_jpip3d_region_t;

/**
 * @brief JPIP 3-D request parameters.
 */
typedef struct opj_jpip3d_request {
    char     dataset[OPJ_JPIP3D_DATASET_NAME_MAX]; /**< Target dataset name. */
    uint32_t fsiz3d[3];        /**< Full image size [w, h, d]. */
    uint32_t roff3d[3];        /**< Region offset [x0, y0, z0]. */
    uint32_t rsiz3d[3];        /**< Region size   [w,  h,  d]. */
    uint32_t component;        /**< Component index. */
    uint32_t quality_layers;   /**< Number of quality layers to deliver. */
    uint32_t resolution_level; /**< Resolution level (0 = full). */
    uint32_t session_id;       /**< Session identifier. */
    int      stream_type;      /**< OPJ_JPIP3D_STREAM_* constant. */
} opj_jpip3d_request_t;

/**
 * @brief JPIP 3-D response.
 *
 * Caller must free @c data with @c opj_jp3d_free() and
 * @c metadata with @c opj_jpip3d_metadata_destroy() when done.
 */
typedef struct opj_jpip3d_response {
    uint8_t               *data;        /**< Encoded payload (may be NULL). */
    size_t                 size;        /**< Payload byte count. */
    int                    stream_type; /**< OPJ_JPIP3D_STREAM_* constant. */
    opj_jpip3d_metadata_t *metadata;    /**< Associated metadata (may be NULL). */
} opj_jpip3d_response_t;

/* -------------------------------------------------------------------------
 *   Version
 * ------------------------------------------------------------------------- */

/**
 * @brief Return the JPIP3D library version string.
 *
 * @return Null-terminated version string.
 */
OPJ_JPIP3D_API const char *opj_jpip3d_get_version(void);

/* -------------------------------------------------------------------------
 *   Server API
 * ------------------------------------------------------------------------- */

/** @brief Create a new JPIP 3-D server. */
OPJ_JPIP3D_API opj_jpip3d_server_t *opj_jpip3d_server_create(void);
/** @brief Destroy a server and release all resources. */
OPJ_JPIP3D_API void opj_jpip3d_server_destroy(opj_jpip3d_server_t *server);
/** @brief Load a JP3D dataset from a file path. */
OPJ_JPIP3D_API int  opj_jpip3d_server_load_dataset(opj_jpip3d_server_t *server,
                        const char *name, const char *filepath);
/** @brief Load a JP3D dataset from a memory buffer. */
OPJ_JPIP3D_API int  opj_jpip3d_server_load_dataset_mem(opj_jpip3d_server_t *server,
                        const char *name, const uint8_t *data, size_t size);
/** @brief Handle an incoming JPIP 3-D request. */
OPJ_JPIP3D_API int  opj_jpip3d_server_handle_request(opj_jpip3d_server_t *server,
                        const opj_jpip3d_request_t *req,
                        opj_jpip3d_response_t *resp);

/* -------------------------------------------------------------------------
 *   Client API
 * ------------------------------------------------------------------------- */

/** @brief Open a new JPIP 3-D session against @p server. */
OPJ_JPIP3D_API opj_jpip3d_session_t *opj_jpip3d_session_open(
    opj_jpip3d_server_t *server);
/** @brief Close a session and release its resources. */
OPJ_JPIP3D_API void opj_jpip3d_session_close(opj_jpip3d_session_t *session);
/** @brief Request a volumetric region; fills @p resp. */
OPJ_JPIP3D_API int  opj_jpip3d_session_request_region(
    opj_jpip3d_session_t *session,
    opj_jpip3d_request_t *req,
    opj_jpip3d_response_t *resp);
/** @brief Request a region and decode it into a volume. */
OPJ_JPIP3D_API opj_volume_t *opj_jpip3d_session_receive_volume(
    opj_jpip3d_session_t *session,
    opj_jpip3d_request_t *req);

/* -------------------------------------------------------------------------
 *   Cache API  (individual-parameter form for public consumers)
 * ------------------------------------------------------------------------- */

/** @brief Create a new precinct-delivery cache. */
OPJ_JPIP3D_API opj_jpip3d_cache_t *opj_jpip3d_cache_create(void);
/** @brief Destroy the cache. */
OPJ_JPIP3D_API void opj_jpip3d_cache_destroy(opj_jpip3d_cache_t *cache);
/** @brief Return 1 if the precinct was already delivered in this session. */
OPJ_JPIP3D_API int  opj_jpip3d_cache_is_precinct_delivered(
    const opj_jpip3d_cache_t *cache,
    uint32_t session_id,
    uint32_t tile_x,    uint32_t tile_y,    uint32_t tile_z,
    uint32_t layer,     uint32_t res,       uint32_t comp,
    uint32_t prec_x,    uint32_t prec_y,    uint32_t prec_z);
/** @brief Mark a precinct as delivered for this session. */
OPJ_JPIP3D_API int  opj_jpip3d_cache_mark_precinct_delivered(
    opj_jpip3d_cache_t *cache,
    uint32_t session_id,
    uint32_t tile_x,    uint32_t tile_y,    uint32_t tile_z,
    uint32_t layer,     uint32_t res,       uint32_t comp,
    uint32_t prec_x,    uint32_t prec_y,    uint32_t prec_z);
/** @brief Invalidate all cache entries for @p session_id. */
OPJ_JPIP3D_API void opj_jpip3d_cache_reset_session(
    opj_jpip3d_cache_t *cache, uint32_t session_id);

/* -------------------------------------------------------------------------
 *   Metadata API
 * ------------------------------------------------------------------------- */

/** @brief Create a metadata wrapper. */
OPJ_JPIP3D_API opj_jpip3d_metadata_t *opj_jpip3d_metadata_create(void);
/** @brief Destroy a metadata wrapper. */
OPJ_JPIP3D_API void        opj_jpip3d_metadata_destroy(opj_jpip3d_metadata_t *meta);
/** @brief Set the XML string (copied internally). */
OPJ_JPIP3D_API int         opj_jpip3d_metadata_set_xml(opj_jpip3d_metadata_t *meta,
                                const char *xml);
/** @brief Get the stored XML string (pointer is valid until next set or destroy). */
OPJ_JPIP3D_API const char *opj_jpip3d_metadata_get_xml(
                                const opj_jpip3d_metadata_t *meta);
/** @brief Serialise to a JP2 XML box; caller frees @p *out with opj_jp3d_free(). */
OPJ_JPIP3D_API int         opj_jpip3d_metadata_to_box(
                                const opj_jpip3d_metadata_t *meta,
                                uint8_t **out, size_t *out_size);
/** @brief Parse from JP2 XML box bytes. */
OPJ_JPIP3D_API int         opj_jpip3d_metadata_from_box(
                                opj_jpip3d_metadata_t *meta,
                                const uint8_t *box, size_t box_size);

/* -------------------------------------------------------------------------
 *   Request parsing / serialisation
 * ------------------------------------------------------------------------- */

/**
 * @brief Parse a JPIP 3-D URL query string into @p req.
 *
 * Supported keys: fsiz3d, roff3d, rsiz3d, comp, layers, level, sid, dataset.
 * @return 1 on success, 0 on failure.
 */
OPJ_JPIP3D_API int opj_jpip3d_parse_request(const char *query,
                       opj_jpip3d_request_t *req);
/**
 * @brief Serialise @p req to a URL query string in @p buf.
 *
 * @return Number of bytes written (excluding NUL), or -1 on error.
 */
OPJ_JPIP3D_API int opj_jpip3d_request_to_url_params(const opj_jpip3d_request_t *req,
                       char *buf, size_t buflen);

#ifdef __cplusplus
}
#endif

#endif /* OPENJPIP3D_H */
