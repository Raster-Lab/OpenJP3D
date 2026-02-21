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
 * @file opj_jpip3d_meta.c
 * @brief JP2/JP3D XML metadata wrapper implementation.
 */

#include "opj_jpip3d_meta.h"

#include "openjp3d.h"   /* opj_jp3d_malloc / free */

#include <string.h>
#include <stddef.h>

/* JP2 XML box type: 'xml ' = 0x786D6C20 */
#define XML_BOX_TYPE UINT32_C(0x786D6C20)

/* Size of the JP2 box header (4-byte length + 4-byte type). */
#define BOX_HEADER_SIZE 8

/* -------------------------------------------------------------------------
 * Internal struct definition
 * ------------------------------------------------------------------------- */

struct opj_jpip3d_metadata {
    char   *xml;
    size_t  xml_len; /* length of xml string, NOT counting NUL */
};

/* -------------------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------------------- */

opj_jpip3d_metadata_t *opj_jpip3d_metadata_create(void)
{
    opj_jpip3d_metadata_t *m =
        (opj_jpip3d_metadata_t *)opj_jp3d_malloc(sizeof(opj_jpip3d_metadata_t));
    if (!m) return NULL;
    m->xml     = NULL;
    m->xml_len = 0;
    return m;
}

void opj_jpip3d_metadata_destroy(opj_jpip3d_metadata_t *meta)
{
    if (!meta) return;
    opj_jp3d_free(meta->xml);
    opj_jp3d_free(meta);
}

/* -------------------------------------------------------------------------
 * Accessors
 * ------------------------------------------------------------------------- */

int opj_jpip3d_metadata_set_xml(opj_jpip3d_metadata_t *meta, const char *xml)
{
    if (!meta || !xml) return 0;

    size_t len = strlen(xml);
    char *copy = (char *)opj_jp3d_malloc(len + 1);
    if (!copy) return 0;
    memcpy(copy, xml, len + 1); /* includes NUL */

    opj_jp3d_free(meta->xml);
    meta->xml     = copy;
    meta->xml_len = len;
    return 1;
}

const char *opj_jpip3d_metadata_get_xml(const opj_jpip3d_metadata_t *meta)
{
    return (meta && meta->xml) ? meta->xml : NULL;
}

/* -------------------------------------------------------------------------
 * Serialisation / deserialisation
 * ------------------------------------------------------------------------- */

int opj_jpip3d_metadata_to_box(const opj_jpip3d_metadata_t *meta,
                                uint8_t **out, size_t *out_size)
{
    if (!meta || !out || !out_size) return 0;
    if (!meta->xml) return 0;

    size_t total = BOX_HEADER_SIZE + meta->xml_len;
    uint8_t *buf = (uint8_t *)opj_jp3d_malloc(total);
    if (!buf) return 0;

    /* Big-endian 4-byte total length */
    uint32_t len32 = (uint32_t)total;
    buf[0] = (uint8_t)((len32 >> 24) & 0xFF);
    buf[1] = (uint8_t)((len32 >> 16) & 0xFF);
    buf[2] = (uint8_t)((len32 >>  8) & 0xFF);
    buf[3] = (uint8_t)( len32        & 0xFF);

    /* Big-endian 4-byte box type 'xml ' */
    buf[4] = (uint8_t)((XML_BOX_TYPE >> 24) & 0xFF);
    buf[5] = (uint8_t)((XML_BOX_TYPE >> 16) & 0xFF);
    buf[6] = (uint8_t)((XML_BOX_TYPE >>  8) & 0xFF);
    buf[7] = (uint8_t)( XML_BOX_TYPE        & 0xFF);

    /* XML payload (without NUL) */
    memcpy(buf + BOX_HEADER_SIZE, meta->xml, meta->xml_len);

    *out      = buf;
    *out_size = total;
    return 1;
}

int opj_jpip3d_metadata_from_box(opj_jpip3d_metadata_t *meta,
                                  const uint8_t *box, size_t box_size)
{
    if (!meta || !box) return 0;
    if (box_size < BOX_HEADER_SIZE) return 0;

    /* Read 4-byte big-endian box type at offset 4 */
    uint32_t type = ((uint32_t)box[4] << 24) |
                    ((uint32_t)box[5] << 16) |
                    ((uint32_t)box[6] <<  8) |
                     (uint32_t)box[7];
    if (type != XML_BOX_TYPE) return 0;

    /* Read 4-byte big-endian length at offset 0 */
    uint32_t declared_len = ((uint32_t)box[0] << 24) |
                            ((uint32_t)box[1] << 16) |
                            ((uint32_t)box[2] <<  8) |
                             (uint32_t)box[3];
    if (declared_len < BOX_HEADER_SIZE || declared_len > box_size) return 0;

    size_t xml_len = (size_t)declared_len - BOX_HEADER_SIZE;
    char *xml_copy = (char *)opj_jp3d_malloc(xml_len + 1);
    if (!xml_copy) return 0;
    memcpy(xml_copy, box + BOX_HEADER_SIZE, xml_len);
    xml_copy[xml_len] = '\0';

    opj_jp3d_free(meta->xml);
    meta->xml     = xml_copy;
    meta->xml_len = xml_len;
    return 1;
}
