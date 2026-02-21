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
 * @file opj_jpip3d_meta.h
 * @brief Internal header: JP2/JP3D XML metadata wrapper.
 */

#ifndef OPJ_JPIP3D_META_H
#define OPJ_JPIP3D_META_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque metadata handle. */
typedef struct opj_jpip3d_metadata opj_jpip3d_metadata_t;

/** @brief Allocate an empty metadata wrapper. Returns NULL on failure. */
opj_jpip3d_metadata_t *opj_jpip3d_metadata_create(void);
/** @brief Destroy @p meta and free all memory. */
void                   opj_jpip3d_metadata_destroy(opj_jpip3d_metadata_t *meta);
/**
 * @brief Set (or replace) the stored XML string.
 *
 * @p xml is copied internally.
 * @return 1 on success, 0 on allocation failure.
 */
int                    opj_jpip3d_metadata_set_xml(opj_jpip3d_metadata_t *meta,
                                                   const char *xml);
/**
 * @brief Return the stored XML string.
 *
 * The returned pointer is valid until the next call to set_xml or destroy.
 */
const char            *opj_jpip3d_metadata_get_xml(const opj_jpip3d_metadata_t *meta);

/**
 * @brief Serialise the stored XML to a JP2 XML box.
 *
 * Box layout: 4-byte big-endian total length (header + XML) |
 *             4-byte type 0x786D6C20 ('xml ') | XML bytes.
 *
 * Caller must free @p *out with opj_jp3d_free().
 * @return 1 on success, 0 on failure.
 */
int opj_jpip3d_metadata_to_box(const opj_jpip3d_metadata_t *meta,
                                uint8_t **out, size_t *out_size);

/**
 * @brief Parse XML from JP2 XML box bytes.
 *
 * Validates the 8-byte header and extracts the XML payload.
 * @return 1 on success, 0 on failure.
 */
int opj_jpip3d_metadata_from_box(opj_jpip3d_metadata_t *meta,
                                  const uint8_t *box, size_t box_size);

#ifdef __cplusplus
}
#endif

#endif /* OPJ_JPIP3D_META_H */
