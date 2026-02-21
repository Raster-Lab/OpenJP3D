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
 * @file test_jpip3d.c
 * @brief Integration tests for the JPIP 3-D streaming library.
 */

#include "openjpip3d.h"
#include "openjp3d.h"

/* Internal headers needed for stream / cache key tests */
#include "opj_jpip3d_stream.h"
#include "opj_jpip3d_cache.h"
#include "opj_jpip3d_meta.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* =========================================================================
 * Test infrastructure
 * ========================================================================= */

static int test_count = 0;
static int pass_count = 0;

#define ASSERT(cond) \
    do { \
        test_count++; \
        if (cond) { \
            pass_count++; \
        } else { \
            fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, #cond); \
        } \
    } while (0)

/* =========================================================================
 * Helper: encode a small volume and return codestream bytes.
 * Caller must opj_jp3d_free(*out_data).
 * ========================================================================= */
static int encode_small_volume(uint32_t w, uint32_t h, uint32_t d,
                                uint8_t **out_data, size_t *out_size)
{
    opj_volume_t *vol = opj_jp3d_create_volume(1, w, h, d, 8, 0);
    if (!vol) return 0;

    /* Fill with deterministic values. */
    size_t n = (size_t)w * h * d;
    for (size_t i = 0; i < n; i++)
        vol->comps[0].data[i] = (int32_t)(i % 256);

    opj_jp3d_enc_params_t enc;
    opj_jp3d_set_default_encoder_parameters(&enc);
    enc.num_resolutions_x = 1;
    enc.num_resolutions_y = 1;
    enc.num_resolutions_z = 1;
    enc.tile_width  = w;
    enc.tile_height = h;
    enc.tile_depth  = d;
    enc.cblk_width  = w;
    enc.cblk_height = h;
    enc.cblk_depth  = d;

    int ok = opj_jp3d_encode(vol, &enc, out_data, out_size, NULL, NULL);
    opj_jp3d_destroy_volume(vol);
    return ok;
}

/* =========================================================================
 * Test 1 — Request parsing
 * ========================================================================= */
static void test_request_parsing(void)
{
    opj_jpip3d_request_t req;
    int ok = opj_jpip3d_parse_request(
        "fsiz3d=512,512,128"
        "&roff3d=0,0,32"
        "&rsiz3d=256,256,64"
        "&comp=1"
        "&layers=2"
        "&level=1"
        "&dataset=vol1",
        &req);

    ASSERT(ok == 1);
    ASSERT(req.fsiz3d[0] == 512 && req.fsiz3d[1] == 512 && req.fsiz3d[2] == 128);
    ASSERT(req.roff3d[0] == 0   && req.roff3d[1] == 0   && req.roff3d[2] == 32);
    ASSERT(req.rsiz3d[0] == 256 && req.rsiz3d[1] == 256 && req.rsiz3d[2] == 64);
    ASSERT(req.component == 1);
    ASSERT(req.quality_layers == 2);
    ASSERT(req.resolution_level == 1);
    ASSERT(strcmp(req.dataset, "vol1") == 0);
}

/* =========================================================================
 * Test 2 — Request round-trip (parse → serialise → parse)
 * ========================================================================= */
static void test_request_roundtrip(void)
{
    opj_jpip3d_request_t req1, req2;
    opj_jpip3d_parse_request(
        "fsiz3d=64,32,16&roff3d=4,8,2&rsiz3d=16,8,4"
        "&comp=2&layers=3&level=0&dataset=test_ds",
        &req1);

    char buf[1024];
    int written = opj_jpip3d_request_to_url_params(&req1, buf, sizeof(buf));
    ASSERT(written > 0);

    int ok = opj_jpip3d_parse_request(buf, &req2);
    ASSERT(ok == 1);
    ASSERT(req2.fsiz3d[0] == req1.fsiz3d[0] &&
           req2.fsiz3d[1] == req1.fsiz3d[1] &&
           req2.fsiz3d[2] == req1.fsiz3d[2]);
    ASSERT(req2.roff3d[0] == req1.roff3d[0] &&
           req2.roff3d[1] == req1.roff3d[1] &&
           req2.roff3d[2] == req1.roff3d[2]);
    ASSERT(req2.rsiz3d[0] == req1.rsiz3d[0] &&
           req2.rsiz3d[1] == req1.rsiz3d[1] &&
           req2.rsiz3d[2] == req1.rsiz3d[2]);
    ASSERT(req2.component       == req1.component);
    ASSERT(req2.quality_layers  == req1.quality_layers);
    ASSERT(req2.resolution_level == req1.resolution_level);
    ASSERT(strcmp(req2.dataset, req1.dataset) == 0);
}

/* =========================================================================
 * Test 3 — Cache mark / check
 * ========================================================================= */
static void test_cache_mark_check(void)
{
    opj_jpip3d_cache_t *cache = opj_jpip3d_cache_create();
    ASSERT(cache != NULL);
    if (!cache) return;

    /* Mark a precinct for session 1. */
    ASSERT(opj_jpip3d_cache_mark_precinct_delivered(
               cache, 1, 0,0,0, 0,0,0, 0,0,0) == 1);

    /* Verify it shows as delivered. */
    ASSERT(opj_jpip3d_cache_is_precinct_delivered(
               cache, 1, 0,0,0, 0,0,0, 0,0,0) == 1);

    /* Different session should not see it. */
    ASSERT(opj_jpip3d_cache_is_precinct_delivered(
               cache, 2, 0,0,0, 0,0,0, 0,0,0) == 0);

    /* Different precinct coordinates should return 0. */
    ASSERT(opj_jpip3d_cache_is_precinct_delivered(
               cache, 1, 1,0,0, 0,0,0, 0,0,0) == 0);
    ASSERT(opj_jpip3d_cache_is_precinct_delivered(
               cache, 1, 0,0,0, 0,0,0, 1,2,3) == 0);

    opj_jpip3d_cache_destroy(cache);
}

/* =========================================================================
 * Test 4 — Cache reset_session
 * ========================================================================= */
static void test_cache_reset_session(void)
{
    opj_jpip3d_cache_t *cache = opj_jpip3d_cache_create();
    ASSERT(cache != NULL);
    if (!cache) return;

    /* Mark precincts for sessions 1 and 2. */
    opj_jpip3d_cache_mark_precinct_delivered(cache, 1, 0,0,0, 0,0,0, 0,0,0);
    opj_jpip3d_cache_mark_precinct_delivered(cache, 1, 1,0,0, 0,0,0, 0,0,0);
    opj_jpip3d_cache_mark_precinct_delivered(cache, 2, 0,0,0, 0,0,0, 0,0,0);

    /* Reset session 1. */
    opj_jpip3d_cache_reset_session(cache, 1);

    /* Session 1 precincts should now be gone. */
    ASSERT(opj_jpip3d_cache_is_precinct_delivered(
               cache, 1, 0,0,0, 0,0,0, 0,0,0) == 0);
    ASSERT(opj_jpip3d_cache_is_precinct_delivered(
               cache, 1, 1,0,0, 0,0,0, 0,0,0) == 0);

    /* Session 2 entry should still be present. */
    ASSERT(opj_jpip3d_cache_is_precinct_delivered(
               cache, 2, 0,0,0, 0,0,0, 0,0,0) == 1);

    opj_jpip3d_cache_destroy(cache);
}

/* =========================================================================
 * Test 5 — Metadata create / set / get
 * ========================================================================= */
static void test_metadata_set_get(void)
{
    opj_jpip3d_metadata_t *meta = opj_jpip3d_metadata_create();
    ASSERT(meta != NULL);
    if (!meta) return;

    const char *xml = "<volume><desc>test</desc></volume>";
    ASSERT(opj_jpip3d_metadata_set_xml(meta, xml) == 1);

    const char *got = opj_jpip3d_metadata_get_xml(meta);
    ASSERT(got != NULL);
    ASSERT(strcmp(got, xml) == 0);

    opj_jpip3d_metadata_destroy(meta);
}

/* =========================================================================
 * Test 6 — Metadata to_box / from_box
 * ========================================================================= */
static void test_metadata_box(void)
{
    opj_jpip3d_metadata_t *meta = opj_jpip3d_metadata_create();
    ASSERT(meta != NULL);
    if (!meta) return;

    const char *xml = "<v>hello</v>";
    opj_jpip3d_metadata_set_xml(meta, xml);

    uint8_t *box  = NULL;
    size_t   bsz  = 0;
    ASSERT(opj_jpip3d_metadata_to_box(meta, &box, &bsz) == 1);
    ASSERT(box != NULL && bsz >= 8);

    /* First 4 bytes: big-endian total length. */
    uint32_t len32 = ((uint32_t)box[0] << 24) | ((uint32_t)box[1] << 16) |
                     ((uint32_t)box[2] <<  8) |  (uint32_t)box[3];
    ASSERT(len32 == (uint32_t)bsz);

    /* Bytes 4–7: box type 'xml ' = 0x786D6C20. */
    uint32_t type = ((uint32_t)box[4] << 24) | ((uint32_t)box[5] << 16) |
                    ((uint32_t)box[6] <<  8) |  (uint32_t)box[7];
    ASSERT(type == UINT32_C(0x786D6C20));

    /* Round-trip through from_box. */
    opj_jpip3d_metadata_t *meta2 = opj_jpip3d_metadata_create();
    ASSERT(meta2 != NULL);
    if (meta2) {
        ASSERT(opj_jpip3d_metadata_from_box(meta2, box, bsz) == 1);
        const char *got = opj_jpip3d_metadata_get_xml(meta2);
        ASSERT(got != NULL && strcmp(got, xml) == 0);
        opj_jpip3d_metadata_destroy(meta2);
    }

    opj_jp3d_free(box);
    opj_jpip3d_metadata_destroy(meta);
}

/* =========================================================================
 * Test 7 — Stream JPT write
 * ========================================================================= */
static void test_stream_jpt_write(void)
{
    opj_jpip3d_stream_t *s = opj_jpip3d_stream_create();
    ASSERT(s != NULL);
    if (!s) return;

    uint8_t payload[2] = {0xAB, 0xCD};
    ASSERT(opj_jpip3d_jpt_write_tile_part(s, 1, 2, 3, 0, 0, 0,
                                           payload, 2) == 1);

    const uint8_t *d = opj_jpip3d_stream_get_data(s);
    size_t         sz = opj_jpip3d_stream_get_size(s);
    /* 4 (magic) + 7*4 (header u32s) + 2 (payload) = 34 */
    ASSERT(sz == 34);
    ASSERT(d != NULL);
    ASSERT(d[0] == 'J' && d[1] == 'P' && d[2] == 'T' && d[3] == '3');

    opj_jpip3d_stream_destroy(s);
}

/* =========================================================================
 * Test 8 — Stream JPP write
 * ========================================================================= */
static void test_stream_jpp_write(void)
{
    opj_jpip3d_stream_t *s = opj_jpip3d_stream_create();
    ASSERT(s != NULL);
    if (!s) return;

    uint8_t payload[1] = {0xFF};
    ASSERT(opj_jpip3d_jpp_write_precinct(s, 0,0,0, 1,2,3, 0,0,0,
                                          payload, 1) == 1);

    const uint8_t *d = opj_jpip3d_stream_get_data(s);
    size_t         sz = opj_jpip3d_stream_get_size(s);
    /* 4 (magic) + 10*4 (header u32s) + 1 (payload) = 45 */
    ASSERT(sz == 45);
    ASSERT(d != NULL);
    ASSERT(d[0] == 'J' && d[1] == 'P' && d[2] == 'P' && d[3] == '3');

    opj_jpip3d_stream_destroy(s);
}

/* =========================================================================
 * Test 9 — Server create / destroy smoke test
 * ========================================================================= */
static void test_server_create_destroy(void)
{
    opj_jpip3d_server_t *srv = opj_jpip3d_server_create();
    ASSERT(srv != NULL);
    opj_jpip3d_server_destroy(srv);
}

/* =========================================================================
 * Test 10 — Server load dataset
 * ========================================================================= */
static void test_server_load_dataset(void)
{
    uint8_t *cs = NULL;
    size_t   cs_sz = 0;
    ASSERT(encode_small_volume(4, 4, 4, &cs, &cs_sz) == 1);
    if (!cs) return;

    opj_jpip3d_server_t *srv = opj_jpip3d_server_create();
    ASSERT(srv != NULL);
    if (srv) {
        ASSERT(opj_jpip3d_server_load_dataset_mem(srv, "vol4", cs, cs_sz) == 1);
        opj_jpip3d_server_destroy(srv);
    }
    opj_jp3d_free(cs);
}

/* =========================================================================
 * Test 11 — Server handle request
 * ========================================================================= */
static void test_server_handle_request(void)
{
    uint8_t *cs = NULL;
    size_t   cs_sz = 0;
    ASSERT(encode_small_volume(4, 4, 4, &cs, &cs_sz) == 1);
    if (!cs) return;

    opj_jpip3d_server_t *srv = opj_jpip3d_server_create();
    ASSERT(srv != NULL);
    if (!srv) { opj_jp3d_free(cs); return; }

    ASSERT(opj_jpip3d_server_load_dataset_mem(srv, "vol", cs, cs_sz) == 1);
    opj_jp3d_free(cs);

    opj_jpip3d_request_t req;
    memset(&req, 0, sizeof(req));
    strncpy(req.dataset, "vol", sizeof(req.dataset) - 1);
    req.roff3d[0] = 0; req.roff3d[1] = 0; req.roff3d[2] = 0;
    req.rsiz3d[0] = 4; req.rsiz3d[1] = 4; req.rsiz3d[2] = 4;
    req.session_id = 999;

    opj_jpip3d_response_t resp;
    ASSERT(opj_jpip3d_server_handle_request(srv, &req, &resp) == 1);
    ASSERT(resp.data != NULL);
    ASSERT(resp.size > 0);

    opj_jp3d_free(resp.data);
    opj_jpip3d_server_destroy(srv);
}

/* =========================================================================
 * Test 12 — Server cache model (same request twice → second is empty)
 * ========================================================================= */
static void test_server_cache_model(void)
{
    uint8_t *cs = NULL;
    size_t   cs_sz = 0;
    ASSERT(encode_small_volume(4, 4, 4, &cs, &cs_sz) == 1);
    if (!cs) return;

    opj_jpip3d_server_t *srv = opj_jpip3d_server_create();
    ASSERT(srv != NULL);
    if (!srv) { opj_jp3d_free(cs); return; }

    opj_jpip3d_server_load_dataset_mem(srv, "vol", cs, cs_sz);
    opj_jp3d_free(cs);

    opj_jpip3d_request_t req;
    memset(&req, 0, sizeof(req));
    strncpy(req.dataset, "vol", sizeof(req.dataset) - 1);
    req.roff3d[0] = 0; req.roff3d[1] = 0; req.roff3d[2] = 0;
    req.rsiz3d[0] = 4; req.rsiz3d[1] = 4; req.rsiz3d[2] = 4;
    req.session_id = 42;

    /* First request — should deliver data. */
    opj_jpip3d_response_t resp1;
    ASSERT(opj_jpip3d_server_handle_request(srv, &req, &resp1) == 1);
    ASSERT(resp1.size > 0);
    opj_jp3d_free(resp1.data);

    /* Second identical request — should return empty (already cached). */
    opj_jpip3d_response_t resp2;
    ASSERT(opj_jpip3d_server_handle_request(srv, &req, &resp2) == 1);
    ASSERT(resp2.size == 0);
    ASSERT(resp2.data == NULL);

    opj_jpip3d_server_destroy(srv);
}

/* =========================================================================
 * Test 13 — Client session open / close smoke test
 * ========================================================================= */
static void test_client_session_open_close(void)
{
    opj_jpip3d_server_t *srv = opj_jpip3d_server_create();
    ASSERT(srv != NULL);
    if (!srv) return;

    opj_jpip3d_session_t *sess = opj_jpip3d_session_open(srv);
    ASSERT(sess != NULL);
    opj_jpip3d_session_close(sess);
    opj_jpip3d_server_destroy(srv);
}

/* =========================================================================
 * Test 14 — Client receive_volume (4×4×4)
 * ========================================================================= */
static void test_client_receive_volume(void)
{
    uint8_t *cs = NULL;
    size_t   cs_sz = 0;
    ASSERT(encode_small_volume(4, 4, 4, &cs, &cs_sz) == 1);
    if (!cs) return;

    opj_jpip3d_server_t *srv = opj_jpip3d_server_create();
    ASSERT(srv != NULL);
    if (!srv) { opj_jp3d_free(cs); return; }

    opj_jpip3d_server_load_dataset_mem(srv, "vol4", cs, cs_sz);
    opj_jp3d_free(cs);

    opj_jpip3d_session_t *sess = opj_jpip3d_session_open(srv);
    ASSERT(sess != NULL);
    if (!sess) { opj_jpip3d_server_destroy(srv); return; }

    opj_jpip3d_request_t req;
    memset(&req, 0, sizeof(req));
    strncpy(req.dataset, "vol4", sizeof(req.dataset) - 1);
    req.roff3d[0] = 0; req.roff3d[1] = 0; req.roff3d[2] = 0;
    req.rsiz3d[0] = 4; req.rsiz3d[1] = 4; req.rsiz3d[2] = 4;

    opj_volume_t *vol = opj_jpip3d_session_receive_volume(sess, &req);
    ASSERT(vol != NULL);
    if (vol) {
        ASSERT(vol->comps[0].w == 4);
        ASSERT(vol->comps[0].h == 4);
        ASSERT(vol->comps[0].d == 4);
        opj_jp3d_destroy_volume(vol);
    }

    opj_jpip3d_session_close(sess);
    opj_jpip3d_server_destroy(srv);
}

/* =========================================================================
 * Test 15 — Client receive sub-volume (8×8×4 → request 4×4×2)
 * ========================================================================= */
static void test_client_receive_subvolume(void)
{
    /* Encode an 8×8×4 volume. */
    opj_volume_t *orig = opj_jp3d_create_volume(1, 8, 8, 4, 8, 0);
    ASSERT(orig != NULL);
    if (!orig) return;

    size_t n = 8u * 8u * 4u;
    for (size_t i = 0; i < n; i++)
        orig->comps[0].data[i] = (int32_t)(i % 128);

    opj_jp3d_enc_params_t enc;
    opj_jp3d_set_default_encoder_parameters(&enc);
    enc.num_resolutions_x = 1; enc.num_resolutions_y = 1; enc.num_resolutions_z = 1;
    enc.tile_width = 8; enc.tile_height = 8; enc.tile_depth = 4;
    enc.cblk_width = 8; enc.cblk_height = 8; enc.cblk_depth = 4;

    uint8_t *cs = NULL; size_t cs_sz = 0;
    ASSERT(opj_jp3d_encode(orig, &enc, &cs, &cs_sz, NULL, NULL) == 1);
    opj_jp3d_destroy_volume(orig);
    if (!cs) return;

    opj_jpip3d_server_t *srv = opj_jpip3d_server_create();
    ASSERT(srv != NULL);
    if (!srv) { opj_jp3d_free(cs); return; }

    opj_jpip3d_server_load_dataset_mem(srv, "vol8", cs, cs_sz);
    opj_jp3d_free(cs);

    opj_jpip3d_session_t *sess = opj_jpip3d_session_open(srv);
    ASSERT(sess != NULL);
    if (!sess) { opj_jpip3d_server_destroy(srv); return; }

    /* Request sub-volume 4×4×2 starting at origin. */
    opj_jpip3d_request_t req;
    memset(&req, 0, sizeof(req));
    strncpy(req.dataset, "vol8", sizeof(req.dataset) - 1);
    req.roff3d[0] = 0; req.roff3d[1] = 0; req.roff3d[2] = 0;
    req.rsiz3d[0] = 4; req.rsiz3d[1] = 4; req.rsiz3d[2] = 2;

    opj_volume_t *vol = opj_jpip3d_session_receive_volume(sess, &req);
    ASSERT(vol != NULL);
    if (vol) {
        ASSERT(vol->comps[0].w == 4);
        ASSERT(vol->comps[0].h == 4);
        ASSERT(vol->comps[0].d == 2);
        opj_jp3d_destroy_volume(vol);
    }

    opj_jpip3d_session_close(sess);
    opj_jpip3d_server_destroy(srv);
}

/* =========================================================================
 * Test 16 — Multi-session independence
 * ========================================================================= */
static void test_multi_session_independence(void)
{
    uint8_t *cs = NULL;
    size_t   cs_sz = 0;
    ASSERT(encode_small_volume(4, 4, 4, &cs, &cs_sz) == 1);
    if (!cs) return;

    opj_jpip3d_server_t *srv = opj_jpip3d_server_create();
    ASSERT(srv != NULL);
    if (!srv) { opj_jp3d_free(cs); return; }

    opj_jpip3d_server_load_dataset_mem(srv, "vol", cs, cs_sz);
    opj_jp3d_free(cs);

    opj_jpip3d_session_t *sessA = opj_jpip3d_session_open(srv);
    opj_jpip3d_session_t *sessB = opj_jpip3d_session_open(srv);
    ASSERT(sessA != NULL);
    ASSERT(sessB != NULL);
    if (!sessA || !sessB) {
        opj_jpip3d_session_close(sessA);
        opj_jpip3d_session_close(sessB);
        opj_jpip3d_server_destroy(srv);
        return;
    }

    opj_jpip3d_request_t reqA, reqB;
    memset(&reqA, 0, sizeof(reqA));
    strncpy(reqA.dataset, "vol", sizeof(reqA.dataset) - 1);
    reqA.roff3d[0] = 0; reqA.roff3d[1] = 0; reqA.roff3d[2] = 0;
    reqA.rsiz3d[0] = 4; reqA.rsiz3d[1] = 4; reqA.rsiz3d[2] = 4;
    reqB = reqA; /* same region */

    /* Session A: receive volume (first delivery). */
    opj_volume_t *volA = opj_jpip3d_session_receive_volume(sessA, &reqA);
    ASSERT(volA != NULL);
    if (volA) opj_jp3d_destroy_volume(volA);

    /* Session B: must also receive data (different session_id). */
    opj_volume_t *volB = opj_jpip3d_session_receive_volume(sessB, &reqB);
    ASSERT(volB != NULL);
    if (volB) opj_jp3d_destroy_volume(volB);

    /* Session A again: should return NULL (already cached for A). */
    opj_volume_t *volA2 = opj_jpip3d_session_receive_volume(sessA, &reqA);
    ASSERT(volA2 == NULL);

    opj_jpip3d_session_close(sessA);
    opj_jpip3d_session_close(sessB);
    opj_jpip3d_server_destroy(srv);
}

/* =========================================================================
 * Test 17 — Server create/load/handle/destroy lifecycle (MT-JPIP-002)
 *
 * Verifies that a full JPIP server lifecycle — create, load dataset,
 * handle a request, destroy — completes without leaks or crashes.
 * ========================================================================= */
static void test_server_lifecycle(void)
{
    /* Encode a small test volume for the server to serve. */
    uint8_t *cs = NULL;
    size_t   cs_sz = 0;
    ASSERT(encode_small_volume(4, 4, 4, &cs, &cs_sz) == 1);
    if (!cs) return;

    /* Step 1 — Create server. */
    opj_jpip3d_server_t *srv = opj_jpip3d_server_create();
    ASSERT(srv != NULL);
    if (!srv) { opj_jp3d_free(cs); return; }

    /* Step 2 — Load dataset. */
    int loaded = opj_jpip3d_server_load_dataset_mem(srv, "life", cs, cs_sz);
    ASSERT(loaded == 1);
    opj_jp3d_free(cs);

    /* Step 3 — Handle a request. */
    opj_jpip3d_request_t req;
    memset(&req, 0, sizeof(req));
    strncpy(req.dataset, "life", sizeof(req.dataset) - 1);
    req.roff3d[0] = 0; req.roff3d[1] = 0; req.roff3d[2] = 0;
    req.rsiz3d[0] = 4; req.rsiz3d[1] = 4; req.rsiz3d[2] = 4;
    req.session_id = 100;

    opj_jpip3d_response_t resp;
    int handled = opj_jpip3d_server_handle_request(srv, &req, &resp);
    ASSERT(handled == 1);
    ASSERT(resp.size > 0);
    if (resp.data) opj_jp3d_free(resp.data);

    /* Step 4 — Clean destroy. */
    opj_jpip3d_server_destroy(srv);
}

/* =========================================================================
 * main
 * ========================================================================= */

int main(void)
{
    test_request_parsing();
    test_request_roundtrip();
    test_cache_mark_check();
    test_cache_reset_session();
    test_metadata_set_get();
    test_metadata_box();
    test_stream_jpt_write();
    test_stream_jpp_write();
    test_server_create_destroy();
    test_server_load_dataset();
    test_server_handle_request();
    test_server_cache_model();
    test_client_session_open_close();
    test_client_receive_volume();
    test_client_receive_subvolume();
    test_multi_session_independence();
    test_server_lifecycle();

    printf("Passed %d/%d tests\n", pass_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}
