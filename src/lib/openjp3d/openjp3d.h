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
 * @file openjp3d.h
 * @brief OpenJP3D — JPEG 2000 Part 10 (JP3D) volumetric codec API.
 *
 * This header defines the public API for the OpenJP3D library, providing
 * lossless and lossy compression of 3-D image volumes conforming to
 * ISO/IEC 15444-10.
 */

#ifndef OPENJP3D_H
#define OPENJP3D_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ==========================================================================
 *   Version information
 * ==========================================================================
 */

/** @brief Major version number. */
#define OPJ_JP3D_VERSION_MAJOR 1

/** @brief Minor version number. */
#define OPJ_JP3D_VERSION_MINOR 0

/** @brief Patch version number. */
#define OPJ_JP3D_VERSION_PATCH 0

/** @brief Full version string. */
#define OPJ_JP3D_VERSION "1.0.0"

/*
 * ==========================================================================
 *   Shared library export macros
 * ==========================================================================
 */

#if defined(_WIN32) || defined(__CYGWIN__)
    #ifdef OPJ_JP3D_BUILDING_DLL
        #define OPJ_JP3D_API __declspec(dllexport)
    #elif defined(OPJ_JP3D_USING_DLL)
        #define OPJ_JP3D_API __declspec(dllimport)
    #else
        #define OPJ_JP3D_API
    #endif
#elif defined(__GNUC__) && __GNUC__ >= 4
    #define OPJ_JP3D_API __attribute__((visibility("default")))
#else
    #define OPJ_JP3D_API
#endif

/*
 * ==========================================================================
 *   Basic types
 * ==========================================================================
 */

/** @brief Boolean type for the JP3D API. */
typedef int32_t opj_jp3d_bool_t;

/** @brief True value. */
#define OPJ_JP3D_TRUE  1

/** @brief False value. */
#define OPJ_JP3D_FALSE 0

/*
 * ==========================================================================
 *   Error handling
 * ==========================================================================
 */

/**
 * @brief Severity levels for event messages.
 */
typedef enum {
    OPJ_JP3D_MSG_INFO    = 0, /**< Informational message. */
    OPJ_JP3D_MSG_WARNING = 1, /**< Warning message. */
    OPJ_JP3D_MSG_ERROR   = 2  /**< Error message. */
} opj_jp3d_msg_level_t;

/**
 * @brief Callback function type for event messages.
 *
 * @param level   Severity level of the message.
 * @param message Null-terminated message string.
 * @param data    User-provided context pointer.
 */
typedef void (*opj_jp3d_msg_callback_t)(opj_jp3d_msg_level_t level,
                                        const char *message,
                                        void *data);

/*
 * ==========================================================================
 *   Filter and colour-space constants
 * ==========================================================================
 */

/** @brief Lossless 5/3 integer lifting filter. */
#define OPJ_JP3D_FILTER_53  0
/** @brief Lossy 9/7 floating-point lifting filter. */
#define OPJ_JP3D_FILTER_97  1

/**
 * @brief Flag for opj_jp3d_enc_params_t::use_htj2k.
 *
 * When set, the encoder uses the High-Throughput block coder (HTJ2K / Part 15
 * adapted to 3-D code-blocks) instead of the EBCOT Tier-1 coder.
 * The decoder auto-detects the coding mode from the codestream marker.
 */
#define OPJ_JP3D_USE_HTJ2K  1

/** @brief Unknown colour space. */
#define OPJ_JP3D_CS_UNKNOWN 0
/** @brief sRGB. */
#define OPJ_JP3D_CS_SRGB    1
/** @brief Greyscale. */
#define OPJ_JP3D_CS_GRAY    2
/** @brief YCbCr (YUV). */
#define OPJ_JP3D_CS_YUV     3

/*
 * ==========================================================================
 *   Volume data structures
 * ==========================================================================
 */

/**
 * @brief A single component (channel) of a 3-D volume.
 */
typedef struct opj_volume_comp {
    uint32_t  w;      /**< Width  (X samples). */
    uint32_t  h;      /**< Height (Y samples). */
    uint32_t  d;      /**< Depth  (Z samples). */
    uint32_t  prec;   /**< Bit depth per sample. */
    int32_t   sgnd;   /**< 1 = signed, 0 = unsigned. */
    float     dz;     /**< Voxel spacing in Z (1.0 = isotropic). */
    int32_t  *data;   /**< Samples: data[z*w*h + y*w + x]. */
} opj_volume_comp_t;

/**
 * @brief A 3-D image volume (possibly multi-component).
 */
typedef struct opj_volume {
    uint32_t            numcomps;   /**< Number of components. */
    opj_volume_comp_t  *comps;      /**< Component array. */
    uint32_t            x0, y0, z0; /**< Volume origin. */
    uint32_t            x1, y1, z1; /**< Volume extent (exclusive). */
    uint32_t            color_space; /**< OPJ_JP3D_CS_* constant. */
} opj_volume_t;

/*
 * ==========================================================================
 *   Codec parameter structures
 * ==========================================================================
 */

/**
 * @brief Encoder parameters for opj_jp3d_encode().
 */
typedef struct opj_jp3d_enc_params {
    uint32_t tile_width;           /**< Tile width  (0 = whole image). */
    uint32_t tile_height;          /**< Tile height (0 = whole image). */
    uint32_t tile_depth;           /**< Tile depth  (0 = whole image). */
    uint32_t num_resolutions_x;    /**< DWT levels along X. */
    uint32_t num_resolutions_y;    /**< DWT levels along Y. */
    uint32_t num_resolutions_z;    /**< DWT levels along Z. */
    uint32_t cblk_width;           /**< Code-block width. */
    uint32_t cblk_height;          /**< Code-block height. */
    uint32_t cblk_depth;           /**< Code-block depth. */
    int32_t  filter;               /**< OPJ_JP3D_FILTER_53 or _97. */
    uint32_t num_layers;           /**< Number of quality layers. */
    float    target_rate;          /**< Target bits/sample (0 = lossless). */
    int32_t  use_htj2k;            /**< OPJ_JP3D_USE_HTJ2K to enable HT block coder. */
    int32_t  verbose;              /**< Non-zero = enable info messages. */
} opj_jp3d_enc_params_t;

/**
 * @brief Decoder parameters for opj_jp3d_decode().
 */
typedef struct opj_jp3d_dec_params {
    int32_t verbose; /**< Non-zero = enable info messages. */
} opj_jp3d_dec_params_t;

/*
 * ==========================================================================
 *   Forward declarations
 * ==========================================================================
 */

/** @brief Opaque codec handle for JP3D encoding/decoding. */
typedef struct opj_jp3d_codec opj_jp3d_codec_t;

/*
 * ==========================================================================
 *   Library lifecycle
 * ==========================================================================
 */

/**
 * @brief Return the library version string.
 *
 * @return Null-terminated version string (e.g. "0.1.0").
 */
OPJ_JP3D_API const char *opj_jp3d_get_version(void);

/*
 * ==========================================================================
 *   Memory management
 * ==========================================================================
 */

/** @brief Allocate @p size bytes; returns NULL on failure. */
OPJ_JP3D_API void *opj_jp3d_malloc(size_t size);
/** @brief Allocate @p n * @p size zeroed bytes; returns NULL on failure. */
OPJ_JP3D_API void *opj_jp3d_calloc(size_t n, size_t size);
/** @brief Resize allocation; returns NULL on failure. */
OPJ_JP3D_API void *opj_jp3d_realloc(void *ptr, size_t size);
/** @brief Free memory allocated by the opj_jp3d_malloc family. */
OPJ_JP3D_API void  opj_jp3d_free(void *ptr);

/*
 * ==========================================================================
 *   Volume lifecycle
 * ==========================================================================
 */

/**
 * @brief Create a new volume with @p numcomps components.
 *
 * All components share the same dimensions (@p w, @p h, @p d),
 * bit depth (@p prec), and signed flag (@p sgnd).
 *
 * @return Pointer to the new volume, or NULL on allocation failure.
 */
OPJ_JP3D_API opj_volume_t *opj_jp3d_create_volume(
    uint32_t numcomps,
    uint32_t w, uint32_t h, uint32_t d,
    uint32_t prec, int32_t sgnd);

/** @brief Free a volume and all its component data. */
OPJ_JP3D_API void opj_jp3d_destroy_volume(opj_volume_t *vol);

/*
 * ==========================================================================
 *   Parameter initialisation
 * ==========================================================================
 */

/** @brief Fill @p params with safe default encoder settings. */
OPJ_JP3D_API void opj_jp3d_set_default_encoder_parameters(
    opj_jp3d_enc_params_t *params);

/** @brief Fill @p params with safe default decoder settings. */
OPJ_JP3D_API void opj_jp3d_set_default_decoder_parameters(
    opj_jp3d_dec_params_t *params);

/*
 * ==========================================================================
 *   Codec
 * ==========================================================================
 */

/**
 * @brief Encode a volume to a JP3D codestream.
 *
 * The caller must free @p *out_data with opj_jp3d_free() when done.
 *
 * @param volume        Input volume.
 * @param params        Encoder parameters.
 * @param out_data      Output: pointer to allocated codestream bytes.
 * @param out_size      Output: number of bytes in @p *out_data.
 * @param callback      Optional message callback (may be NULL).
 * @param callback_data User data for @p callback.
 * @return OPJ_JP3D_TRUE on success, OPJ_JP3D_FALSE on failure.
 */
OPJ_JP3D_API opj_jp3d_bool_t opj_jp3d_encode(
    const opj_volume_t          *volume,
    const opj_jp3d_enc_params_t *params,
    uint8_t                    **out_data,
    size_t                      *out_size,
    opj_jp3d_msg_callback_t      callback,
    void                        *callback_data);

/**
 * @brief Decode a JP3D codestream to a volume.
 *
 * @param data          Codestream bytes.
 * @param size          Number of bytes.
 * @param params        Decoder parameters (may be NULL for defaults).
 * @param callback      Optional message callback (may be NULL).
 * @param callback_data User data for @p callback.
 * @return Pointer to decoded volume, or NULL on failure.
 *         Caller must free with opj_jp3d_destroy_volume().
 */
OPJ_JP3D_API opj_volume_t *opj_jp3d_decode(
    const uint8_t               *data,
    size_t                       size,
    const opj_jp3d_dec_params_t *params,
    opj_jp3d_msg_callback_t      callback,
    void                        *callback_data);

/**
 * @brief Losslessly transcode a JP3D codestream from EBCOT to HT block coding.
 *
 * Decodes @p src_data using EBCOT Tier-1, then re-encodes the recovered
 * volume with the HTJ2K high-throughput block coder, preserving the tile
 * structure and DWT parameters.  The output codestream is functionally
 * equivalent to the input (same decoded pixel values) but uses the HT
 * block coder for every tile.
 *
 * @param src_data      Source JP3D codestream bytes (EBCOT or HT).
 * @param src_size      Number of bytes in @p src_data.
 * @param enc_params    Encoder parameters to use for the transcoded stream.
 *                      The @c use_htj2k field is forced to @c OPJ_JP3D_USE_HTJ2K.
 *                      Pass NULL to derive parameters from the source header.
 * @param out_data      Output: pointer to the transcoded codestream bytes.
 *                      Caller must free with opj_jp3d_free().
 * @param out_size      Output: number of bytes in @p *out_data.
 * @param callback      Optional message callback (may be NULL).
 * @param callback_data User data for @p callback.
 * @return OPJ_JP3D_TRUE on success, OPJ_JP3D_FALSE on failure.
 */
OPJ_JP3D_API opj_jp3d_bool_t opj_jp3d_transcode_to_ht(
    const uint8_t               *src_data,
    size_t                       src_size,
    const opj_jp3d_enc_params_t *enc_params,
    uint8_t                    **out_data,
    size_t                      *out_size,
    opj_jp3d_msg_callback_t      callback,
    void                        *callback_data);

#ifdef __cplusplus
}
#endif

#endif /* OPENJP3D_H */
