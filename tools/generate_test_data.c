/*
 * Copyright (c) 2024-2026, OpenJP3D Contributors
 * All rights reserved.
 * SPDX-License-Identifier: BSD-2-Clause
 */

/**
 * @file generate_test_data.c
 * @brief Generate synthetic 3-D volume test data for the OpenJP3D GUI.
 *
 * Produces a variety of raw (.raw) and JP3D (.jp3d) test files that
 * exercise every feature of the GUI application:
 *
 *   1. sphere_64x64x64_8bit.raw        — 8-bit sphere phantom
 *   2. gradient_64x64x32_16bit.raw      — 16-bit 3-D gradient
 *   3. shepp_logan_128x128x64_8bit.raw  — 8-bit Shepp–Logan-like phantom
 *   4. checkerboard_32x32x32_8bit.raw   — 8-bit 3-D checkerboard
 *   5. rgb_sphere_64x64x64_8bit.raw     — 3-component (RGB) sphere
 *   6. heart_128x128x128_8bit.raw        — anatomical heart phantom
 *   7. rgb_heart_128x128x128_8bit.raw    — colour-coded anatomical heart
 *   8. sphere_lossless.jp3d             — lossless JP3D codestream (5/3)
 *   9. sphere_lossy.jp3d               — lossy JP3D codestream (9/7)
 *  10. heart_lossless.jp3d             — lossless JP3D heart codestream
 *
 * Usage:
 *   generate_test_data [output_directory]
 *
 * If no directory is given, files are written to the current directory.
 * A companion README_TEST_DATA.txt is also produced describing each file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "openjp3d.h"
#include "opj_raw_io.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ------------------------------------------------------------------ */
/*  Helpers                                                           */
/* ------------------------------------------------------------------ */

/** Build a full path: dir/filename. */
static void build_path(char *out, size_t out_sz,
                       const char *dir, const char *filename)
{
    if (dir && dir[0]) {
        snprintf(out, out_sz, "%s/%s", dir, filename);
    } else {
        snprintf(out, out_sz, "%s", filename);
    }
}

/** Fill a volume component with zeros. */
static void fill_zero(opj_volume_comp_t *c)
{
    size_t n = (size_t)c->w * c->h * c->d;
    for (size_t i = 0; i < n; i++) {
        c->data[i] = 0;
    }
}

/* ------------------------------------------------------------------ */
/*  1. Sphere phantom (8-bit, 64³)                                    */
/* ------------------------------------------------------------------ */

static opj_volume_t *make_sphere(uint32_t dim)
{
    opj_volume_t *vol = opj_jp3d_create_volume(1, dim, dim, dim, 8, 0);
    if (!vol) return NULL;

    float cx = (float)dim / 2.0f;
    float cy = (float)dim / 2.0f;
    float cz = (float)dim / 2.0f;
    float r  = (float)dim * 0.4f;

    opj_volume_comp_t *c = &vol->comps[0];
    for (uint32_t z = 0; z < dim; z++) {
        for (uint32_t y = 0; y < dim; y++) {
            for (uint32_t x = 0; x < dim; x++) {
                float dx = (float)x - cx;
                float dy = (float)y - cy;
                float dz = (float)z - cz;
                float dist = sqrtf(dx * dx + dy * dy + dz * dz);
                int32_t val;
                if (dist <= r) {
                    /* Smooth falloff from centre */
                    val = (int32_t)(255.0f * (1.0f - dist / r));
                } else {
                    val = 0;
                }
                c->data[z * dim * dim + y * dim + x] = val;
            }
        }
    }
    return vol;
}

/* ------------------------------------------------------------------ */
/*  2. 3-D gradient (16-bit, 64×64×32)                                */
/* ------------------------------------------------------------------ */

static opj_volume_t *make_gradient(uint32_t w, uint32_t h, uint32_t d)
{
    opj_volume_t *vol = opj_jp3d_create_volume(1, w, h, d, 16, 0);
    if (!vol) return NULL;

    opj_volume_comp_t *c = &vol->comps[0];
    for (uint32_t z = 0; z < d; z++) {
        for (uint32_t y = 0; y < h; y++) {
            for (uint32_t x = 0; x < w; x++) {
                /* Diagonal gradient [0..65535] */
                float t = ((float)x / (float)(w - 1) +
                           (float)y / (float)(h - 1) +
                           (float)z / (float)(d - 1)) / 3.0f;
                c->data[z * w * h + y * w + x] = (int32_t)(t * 65535.0f);
            }
        }
    }
    return vol;
}

/* ------------------------------------------------------------------ */
/*  3. Shepp–Logan-like phantom (8-bit, 128×128×64)                   */
/* ------------------------------------------------------------------ */

/** A single ellipsoid element of the phantom. */
typedef struct {
    float cx, cy, cz;   /* centre (normalised 0..1) */
    float rx, ry, rz;   /* semi-axes (normalised) */
    float angle;         /* rotation around Z (radians) */
    float intensity;     /* additive intensity [-1..+1] */
} phantom_ellipsoid_t;

static const phantom_ellipsoid_t PHANTOM_ELEMS[] = {
    /* Outer skull */
    { 0.50f, 0.50f, 0.50f,  0.46f, 0.46f, 0.45f,  0.0f,        0.80f },
    /* Inner skull */
    { 0.50f, 0.52f, 0.50f,  0.44f, 0.42f, 0.43f,  0.0f,       -0.40f },
    /* Left ventricle */
    { 0.38f, 0.50f, 0.50f,  0.08f, 0.15f, 0.15f,  0.3f,        0.25f },
    /* Right ventricle */
    { 0.62f, 0.50f, 0.50f,  0.08f, 0.15f, 0.15f, -0.3f,        0.25f },
    /* Small feature */
    { 0.50f, 0.65f, 0.50f,  0.04f, 0.04f, 0.10f,  0.0f,        0.40f },
    /* Tumour-like spot */
    { 0.56f, 0.40f, 0.55f,  0.03f, 0.03f, 0.03f,  0.0f,        0.60f },
};
#define NUM_PHANTOM_ELEMS (sizeof(PHANTOM_ELEMS) / sizeof(PHANTOM_ELEMS[0]))

static opj_volume_t *make_shepp_logan(uint32_t w, uint32_t h, uint32_t d)
{
    opj_volume_t *vol = opj_jp3d_create_volume(1, w, h, d, 8, 0);
    if (!vol) return NULL;

    opj_volume_comp_t *c = &vol->comps[0];
    fill_zero(c);

    for (uint32_t z = 0; z < d; z++) {
        float nz = (float)z / (float)(d - 1);
        for (uint32_t y = 0; y < h; y++) {
            float ny = (float)y / (float)(h - 1);
            for (uint32_t x = 0; x < w; x++) {
                float nx = (float)x / (float)(w - 1);
                float accum = 0.0f;

                for (size_t e = 0; e < NUM_PHANTOM_ELEMS; e++) {
                    const phantom_ellipsoid_t *el = &PHANTOM_ELEMS[e];
                    float dx = nx - el->cx;
                    float dy = ny - el->cy;
                    float dz = nz - el->cz;
                    /* Simple rotation around Z */
                    float ca = cosf(el->angle);
                    float sa = sinf(el->angle);
                    float rx = dx * ca + dy * sa;
                    float ry = -dx * sa + dy * ca;
                    float rz = dz;
                    float t = (rx * rx) / (el->rx * el->rx) +
                              (ry * ry) / (el->ry * el->ry) +
                              (rz * rz) / (el->rz * el->rz);
                    if (t <= 1.0f) {
                        accum += el->intensity;
                    }
                }

                if (accum < 0.0f) accum = 0.0f;
                if (accum > 1.0f) accum = 1.0f;
                c->data[z * w * h + y * w + x] = (int32_t)(accum * 255.0f);
            }
        }
    }
    return vol;
}

/* ------------------------------------------------------------------ */
/*  4. 3-D checkerboard (8-bit, 32³)                                  */
/* ------------------------------------------------------------------ */

static opj_volume_t *make_checkerboard(uint32_t dim, uint32_t block_size)
{
    opj_volume_t *vol = opj_jp3d_create_volume(1, dim, dim, dim, 8, 0);
    if (!vol) return NULL;

    opj_volume_comp_t *c = &vol->comps[0];
    for (uint32_t z = 0; z < dim; z++) {
        for (uint32_t y = 0; y < dim; y++) {
            for (uint32_t x = 0; x < dim; x++) {
                int parity = ((x / block_size) + (y / block_size) +
                              (z / block_size)) % 2;
                c->data[z * dim * dim + y * dim + x] = parity ? 200 : 55;
            }
        }
    }
    return vol;
}

/* ------------------------------------------------------------------ */
/*  5. RGB sphere (3-component, 8-bit, 64³)                           */
/* ------------------------------------------------------------------ */

static opj_volume_t *make_rgb_sphere(uint32_t dim)
{
    opj_volume_t *vol = opj_jp3d_create_volume(3, dim, dim, dim, 8, 0);
    if (!vol) return NULL;

    float cx = (float)dim / 2.0f;
    float cy = (float)dim / 2.0f;
    float cz = (float)dim / 2.0f;
    float r  = (float)dim * 0.4f;

    for (uint32_t comp = 0; comp < 3; comp++) {
        opj_volume_comp_t *c = &vol->comps[comp];
        for (uint32_t z = 0; z < dim; z++) {
            for (uint32_t y = 0; y < dim; y++) {
                for (uint32_t x = 0; x < dim; x++) {
                    float dx = (float)x - cx;
                    float dy = (float)y - cy;
                    float dz = (float)z - cz;
                    float dist = sqrtf(dx * dx + dy * dy + dz * dz);
                    int32_t val = 0;
                    if (dist <= r) {
                        float t = 1.0f - dist / r;
                        /* Different colour channels */
                        switch (comp) {
                            case 0: /* R: radial falloff */
                                val = (int32_t)(255.0f * t);
                                break;
                            case 1: /* G: angular (azimuth) */
                                val = (int32_t)(127.0f + 127.0f *
                                    sinf(atan2f(dy, dx)));
                                break;
                            case 2: /* B: depth-based */
                                val = (int32_t)(255.0f *
                                    ((float)z / (float)dim));
                                break;
                        }
                    }
                    c->data[z * dim * dim + y * dim + x] = val;
                }
            }
        }
    }
    return vol;
}

/* ------------------------------------------------------------------ */
/*  6. Anatomical Human Heart (8-bit, 128³)                           */
/*                                                                    */
/*  Multi-ellipsoid phantom simulating a realistic human heart:       */
/*    - Outer pericardium                                             */
/*    - Myocardial wall (bright)                                      */
/*    - Left ventricle cavity (dark)                                  */
/*    - Right ventricle cavity (dark)                                 */
/*    - Left atrium (dark)                                            */
/*    - Right atrium (dark)                                           */
/*    - Interventricular septum (bright)                              */
/*    - Aorta ascending from LV (medium)                              */
/*    - Pulmonary artery from RV (medium)                             */
/*    - Superior/inferior vena cava entering RA (medium)              */
/*    - Papillary muscles inside LV (bright spots)                    */
/* ------------------------------------------------------------------ */

/** Ellipsoid test: returns 1 if point (px,py,pz) is inside the
 *  ellipsoid centred at (cx,cy,cz) with semi-axes (rx,ry,rz). */
static int inside_ellipsoid(float px, float py, float pz,
                            float cx, float cy, float cz,
                            float rx, float ry, float rz)
{
    float dx = (px - cx) / rx;
    float dy = (py - cy) / ry;
    float dz = (pz - cz) / rz;
    return (dx * dx + dy * dy + dz * dz) <= 1.0f;
}

/** Cylinder test along Y axis: returns 1 if point is inside a
 *  cylinder centred at (cx,cz) with radius r, between y0 and y1. */
static int inside_cylinder_y(float px, float py, float pz,
                             float cx, float cz, float r,
                             float y0, float y1)
{
    if (py < y0 || py > y1) return 0;
    float dx = px - cx;
    float dz = pz - cz;
    return (dx * dx + dz * dz) <= (r * r);
}

static opj_volume_t *make_heart(uint32_t dim)
{
    opj_volume_t *vol = opj_jp3d_create_volume(1, dim, dim, dim, 8, 0);
    if (!vol) return NULL;

    opj_volume_comp_t *c = &vol->comps[0];
    float half = (float)dim / 2.0f;
    /* Normalise voxel coords to [0..1] */
    float inv_dim = 1.0f / (float)dim;

    for (uint32_t vz = 0; vz < dim; vz++) {
        for (uint32_t vy = 0; vy < dim; vy++) {
            for (uint32_t vx = 0; vx < dim; vx++) {
                /* Normalised coordinates [0..1], centre at (0.5, 0.5, 0.5)
                 *   x = left-right
                 *   y = inferior-superior (bottom-top)
                 *   z = anterior-posterior (front-back)
                 */
                float x = ((float)vx + 0.5f) * inv_dim;
                float y = ((float)vy + 0.5f) * inv_dim;
                float z = ((float)vz + 0.5f) * inv_dim;

                int32_t val = 0;  /* background = 0 (air) */

                /* --- Pericardium / outer myocardial shell --- */
                /* Main heart body: slightly tilted left, egg-shaped */
                int in_outer = inside_ellipsoid(x, y, z,
                    0.48f, 0.45f, 0.50f,   /* centre slightly left, low */
                    0.28f, 0.32f, 0.25f);  /* semi-axes */

                if (in_outer) {
                    val = 180;  /* myocardial wall (bright tissue) */
                }

                /* --- Left Ventricle (LV) cavity --- */
                /* Thick-walled, lower-left */
                int in_lv = inside_ellipsoid(x, y, z,
                    0.44f, 0.38f, 0.50f,
                    0.10f, 0.18f, 0.10f);
                if (in_lv && in_outer) {
                    val = 30;  /* blood pool (dark) */
                }

                /* --- Right Ventricle (RV) cavity --- */
                /* Thinner wall, wraps around LV anteriorly */
                int in_rv = inside_ellipsoid(x, y, z,
                    0.58f, 0.38f, 0.45f,
                    0.10f, 0.16f, 0.08f);
                if (in_rv && in_outer) {
                    val = 35;  /* blood pool */
                }

                /* --- Interventricular septum --- */
                /* Bright wall between LV and RV */
                if (in_outer && !in_lv && !in_rv) {
                    float sept_x = 0.51f;
                    float dx = x - sept_x;
                    if (fabsf(dx) < 0.025f && y < 0.50f && y > 0.25f
                        && z > 0.40f && z < 0.58f) {
                        val = 210;  /* septum (brighter than wall) */
                    }
                }

                /* --- Left Atrium (LA) --- */
                /* Upper-left-posterior */
                int in_la = inside_ellipsoid(x, y, z,
                    0.42f, 0.58f, 0.55f,
                    0.09f, 0.10f, 0.08f);
                if (in_la && in_outer) {
                    val = 25;  /* blood pool */
                }

                /* --- Right Atrium (RA) --- */
                /* Upper-right */
                int in_ra = inside_ellipsoid(x, y, z,
                    0.60f, 0.58f, 0.48f,
                    0.08f, 0.10f, 0.08f);
                if (in_ra && in_outer) {
                    val = 28;  /* blood pool */
                }

                /* --- Aorta --- */
                /* Ascending from LV upward-right */
                if (inside_cylinder_y(x, y, z,
                        0.44f, 0.48f, 0.04f, 0.55f, 0.82f)) {
                    val = 140;  /* vessel wall */
                    /* Lumen (inner hollow) */
                    if (inside_cylinder_y(x, y, z,
                            0.44f, 0.48f, 0.025f, 0.56f, 0.81f)) {
                        val = 40;  /* blood */
                    }
                }

                /* --- Aortic arch curve --- */
                /* Curve rightward at top */
                {
                    float arch_cx = 0.50f;
                    float arch_cy = 0.80f;
                    float arch_r  = 0.06f;
                    float dx = x - arch_cx;
                    float dy = y - arch_cy;
                    float dist2d = sqrtf(dx * dx + dy * dy);
                    float dz_arch = z - 0.48f;
                    if (dist2d < arch_r + 0.04f && dist2d > arch_r - 0.04f
                        && fabsf(dz_arch) < 0.04f && y > 0.76f) {
                        val = 140;
                        if (dist2d < arch_r + 0.025f
                            && dist2d > arch_r - 0.025f
                            && fabsf(dz_arch) < 0.025f) {
                            val = 40;
                        }
                    }
                }

                /* --- Pulmonary artery --- */
                /* From RV upward-left */
                if (inside_cylinder_y(x, y, z,
                        0.55f, 0.43f, 0.035f, 0.55f, 0.78f)) {
                    val = 130;
                    if (inside_cylinder_y(x, y, z,
                            0.55f, 0.43f, 0.02f, 0.56f, 0.77f)) {
                        val = 38;
                    }
                }

                /* --- Superior Vena Cava (SVC) --- */
                if (inside_cylinder_y(x, y, z,
                        0.62f, 0.50f, 0.03f, 0.60f, 0.82f)) {
                    val = 120;
                    if (inside_cylinder_y(x, y, z,
                            0.62f, 0.50f, 0.018f, 0.61f, 0.81f)) {
                        val = 32;
                    }
                }

                /* --- Inferior Vena Cava (IVC) --- */
                if (inside_cylinder_y(x, y, z,
                        0.62f, 0.52f, 0.03f, 0.15f, 0.38f)) {
                    val = 120;
                    if (inside_cylinder_y(x, y, z,
                            0.62f, 0.52f, 0.018f, 0.16f, 0.37f)) {
                        val = 32;
                    }
                }

                /* --- Papillary muscles inside LV --- */
                if (in_lv) {
                    int pap1 = inside_ellipsoid(x, y, z,
                        0.40f, 0.30f, 0.47f,
                        0.015f, 0.04f, 0.015f);
                    int pap2 = inside_ellipsoid(x, y, z,
                        0.48f, 0.30f, 0.53f,
                        0.015f, 0.04f, 0.015f);
                    if (pap1 || pap2) {
                        val = 200;  /* muscle (bright) */
                    }
                }

                /* --- Coronary arteries on surface --- */
                /* Left anterior descending (LAD) */
                if (in_outer) {
                    float dist_lad = sqrtf(
                        (x - 0.48f) * (x - 0.48f) +
                        (z - 0.38f) * (z - 0.38f));
                    if (dist_lad < 0.015f && y > 0.30f && y < 0.60f) {
                        val = 240;  /* calcified / contrast */
                    }
                    /* Right coronary artery */
                    float dist_rca = sqrtf(
                        (x - 0.64f) * (x - 0.64f) +
                        (z - 0.50f) * (z - 0.50f));
                    if (dist_rca < 0.012f && y > 0.35f && y < 0.55f) {
                        val = 235;
                    }
                }

                /* --- Apex (bottom point, slightly rounded) --- */
                {
                    float apex_dist = sqrtf(
                        (x - 0.46f) * (x - 0.46f) +
                        (y - 0.15f) * (y - 0.15f) +
                        (z - 0.50f) * (z - 0.50f));
                    if (apex_dist < 0.06f && in_outer) {
                        val = 195;  /* thick apical wall */
                    }
                }

                c->data[vz * dim * dim + vy * dim + vx] = val;
            }
        }
    }
    return vol;
}

/* ------------------------------------------------------------------ */
/*  7. RGB Heart (3-component, 8-bit, 128³)                           */
/*  Colour-coded anatomy for clear visualisation:                     */
/*    Red    = myocardial wall + muscle                               */
/*    Blue   = blood pools (chambers)                                 */
/*    Yellow = vessel walls                                           */
/*    White  = coronary arteries                                      */
/* ------------------------------------------------------------------ */

static opj_volume_t *make_rgb_heart(uint32_t dim)
{
    /* First build the greyscale heart to get the tissue labels */
    opj_volume_t *grey = make_heart(dim);
    if (!grey) return NULL;

    opj_volume_t *vol = opj_jp3d_create_volume(3, dim, dim, dim, 8, 0);
    if (!vol) {
        opj_jp3d_destroy_volume(grey);
        return NULL;
    }

    size_t n = (size_t)dim * dim * dim;
    for (size_t i = 0; i < n; i++) {
        int32_t g = grey->comps[0].data[i];
        int32_t r = 0, gv = 0, b = 0;

        if (g == 0) {
            /* Background: black */
        } else if (g <= 40) {
            /* Blood pool: dark red-blue */
            r = 60;  gv = 20;  b = 120;
        } else if (g >= 230) {
            /* Coronary arteries: bright white-yellow */
            r = 255; gv = 240; b = 180;
        } else if (g >= 190) {
            /* Dense muscle / septum / apex: deep red */
            r = 200; gv = 50;  b = 50;
        } else if (g >= 160) {
            /* Myocardial wall: warm red */
            r = 180; gv = 70;  b = 65;
        } else if (g >= 110) {
            /* Vessel walls: pink-tan */
            r = 200; gv = 130; b = 120;
        } else {
            /* Other tissue */
            r = 150; gv = 100; b = 90;
        }

        vol->comps[0].data[i] = r;
        vol->comps[1].data[i] = gv;
        vol->comps[2].data[i] = b;
    }

    opj_jp3d_destroy_volume(grey);
    return vol;
}

/* ------------------------------------------------------------------ */
/*  JP3D codestream generation                                        */
/* ------------------------------------------------------------------ */

static int write_jp3d(const opj_volume_t *vol,
                      const char *path, int lossy)
{
    opj_jp3d_enc_params_t enc;
    opj_jp3d_set_default_encoder_parameters(&enc);

    if (lossy) {
        enc.filter      = OPJ_JP3D_FILTER_97;
        enc.target_rate = 2.0f;  /* ~2 bits per sample */
    } else {
        enc.filter      = OPJ_JP3D_FILTER_53;
        enc.target_rate = 0.0f;  /* lossless */
    }

    uint8_t *cs_data = NULL;
    size_t   cs_size = 0;
    if (!opj_jp3d_encode(vol, &enc, &cs_data, &cs_size, NULL, NULL)) {
        fprintf(stderr, "ERROR: opj_jp3d_encode failed for %s\n", path);
        return 0;
    }

    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "ERROR: cannot open %s for writing\n", path);
        opj_jp3d_free(cs_data);
        return 0;
    }
    fwrite(cs_data, 1, cs_size, f);
    fclose(f);
    opj_jp3d_free(cs_data);

    printf("  Wrote %s (%zu bytes)\n", path, cs_size);
    return 1;
}

/* ------------------------------------------------------------------ */
/*  README generation                                                 */
/* ------------------------------------------------------------------ */

static void write_readme(const char *dir)
{
    char path[1024];
    build_path(path, sizeof(path), dir, "README_TEST_DATA.txt");

    FILE *f = fopen(path, "w");
    if (!f) return;

    fprintf(f,
"OpenJP3D GUI — Test Data\n"
"========================\n\n"
"Generated by generate_test_data. These files exercise the GUI's\n"
"volume viewer, codec panels, and round-trip testing features.\n\n"
"Raw Volumes (open via File > Open Raw Volume):\n"
"----------------------------------------------\n\n"
"  File                                  | Dims       | Bits | Comps | Notes\n"
"  --------------------------------------|------------|------|-------|----------------------------\n"
"  sphere_64x64x64_8bit.raw             | 64×64×64   |  8   |  1    | Smooth sphere, radial falloff\n"
"  gradient_64x64x32_16bit.raw          | 64×64×32   | 16   |  1    | Diagonal 3-D gradient 0..65535\n"
"  shepp_logan_128x128x64_8bit.raw      | 128×128×64 |  8   |  1    | Shepp-Logan-like CT phantom\n"
"  checkerboard_32x32x32_8bit.raw       | 32×32×32   |  8   |  1    | 3-D checkerboard (4-voxel blocks)\n"
"  rgb_sphere_64x64x64_8bit.raw         | 64×64×64   |  8   |  3    | 3-component (RGB) sphere\n"
"  heart_128x128x128_8bit.raw            | 128×128×128|  8   |  1    | Anatomical heart (chambers, vessels)\n"
"  rgb_heart_128x128x128_8bit.raw        | 128×128×128|  8   |  3    | Colour-coded anatomical heart\n\n"
"JP3D Codestreams (open via File > Open JP3D):\n"
"---------------------------------------------\n\n"
"  File                  | Source           | Mode           | Notes\n"
"  ----------------------|------------------|----------------|----------------------------\n"
"  sphere_lossless.jp3d  | sphere 64³       | 5/3 lossless   | Bit-exact round-trip\n"
"  sphere_lossy.jp3d     | sphere 64³       | 9/7 @ 2 bps    | ~4:1 compression\n"
"  gradient_lossless.jp3d| gradient 64x64x32| 5/3 lossless   | 16-bit lossless\n"
"  phantom_lossless.jp3d | Shepp-Logan 128³ | 5/3 lossless   | Medical phantom codestream\n"
"  heart_lossless.jp3d  | heart 128³       | 5/3 lossless   | Anatomical heart codestream\n\n"
"GUI Testing Guide:\n"
"------------------\n\n"
"1. Volume Viewer (Phase 8B):\n"
"   - Open sphere_64x64x64_8bit.raw (dims: 64, 64, 64, 8-bit, 1 comp)\n"
"   - Test slice navigation (axial / sagittal / coronal)\n"
"   - Test window/level adjustment\n"
"   - Test 3-D ray-cast view toggle\n"
"   - Open rgb_sphere_64x64x64_8bit.raw (dims: 64, 64, 64, 8-bit, 3 comps)\n"
"   - Verify multi-component display\n\n"
"2. Codec Panel (Phase 8C):\n"
"   - Open sphere_64x64x64_8bit.raw, then encode to JP3D (lossless + lossy)\n"
"   - Open sphere_lossless.jp3d directly — should decode and display\n"
"   - Test transcode from EBCOT to HTJ2K\n\n"
"3. Round-trip Testing (Phase 8D):\n"
"   - Open shepp_logan_128x128x64_8bit.raw\n"
"   - Run one-click round-trip (encode → decode → diff)\n"
"   - Verify lossless: PSNR=inf, MSE=0, max-err=0\n"
"   - Switch to lossy 9/7 and verify PSNR / error map\n"
"   - Run batch test suite\n\n"
"4. Codestream Inspector (Phase 8D.4):\n"
"   - Open sphere_lossless.jp3d\n"
"   - Verify tree-view shows SOC, SIZ3D, COD3D, QCD3D markers\n"
"   - Inspect tile-part / packet structure\n\n"
"5. 3-D Volume Rendering:\n"
"   - Open heart_128x128x128_8bit.raw (dims: 128, 128, 128, 8-bit, 1 comp)\n"
"   - Toggle 3-D ray-cast view to see the anatomical heart\n"
"   - Adjust azimuth, elevation, and density sliders to explore\n"
"   - Visible structures: LV, RV, LA, RA, aorta, pulmonary artery\n"
"   - Open rgb_heart_128x128x128_8bit.raw for colour-coded anatomy\n\n"
    );

    fclose(f);
    printf("  Wrote %s\n", path);
}

/* ------------------------------------------------------------------ */
/*  Main                                                              */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    const char *dir = (argc > 1) ? argv[1] : ".";
    char path[1024];
    int ok = 1;

    printf("OpenJP3D GUI Test Data Generator\n");
    printf("================================\n");
    printf("Output directory: %s\n\n", dir);

    /* ---- 1. Sphere 64³ 8-bit ---- */
    printf("[1/12] Sphere phantom (64³, 8-bit)...\n");
    opj_volume_t *sphere = make_sphere(64);
    if (sphere) {
        build_path(path, sizeof(path), dir, "sphere_64x64x64_8bit.raw");
        if (!opj_raw_io_write(sphere, path)) {
            fprintf(stderr, "  ERROR writing %s\n", path);
            ok = 0;
        } else {
            printf("  Wrote %s\n", path);
        }
    } else {
        fprintf(stderr, "  ERROR creating sphere volume\n");
        ok = 0;
    }

    /* ---- 2. Gradient 64×64×32 16-bit ---- */
    printf("[2/12] 3-D gradient (64×64×32, 16-bit)...\n");
    opj_volume_t *grad = make_gradient(64, 64, 32);
    if (grad) {
        build_path(path, sizeof(path), dir, "gradient_64x64x32_16bit.raw");
        if (!opj_raw_io_write(grad, path)) {
            fprintf(stderr, "  ERROR writing %s\n", path);
            ok = 0;
        } else {
            printf("  Wrote %s\n", path);
        }
    } else {
        fprintf(stderr, "  ERROR creating gradient volume\n");
        ok = 0;
    }

    /* ---- 3. Shepp–Logan phantom 128×128×64 ---- */
    printf("[3/12] Shepp-Logan phantom (128×128×64, 8-bit)...\n");
    opj_volume_t *phantom = make_shepp_logan(128, 128, 64);
    if (phantom) {
        build_path(path, sizeof(path), dir,
                   "shepp_logan_128x128x64_8bit.raw");
        if (!opj_raw_io_write(phantom, path)) {
            fprintf(stderr, "  ERROR writing %s\n", path);
            ok = 0;
        } else {
            printf("  Wrote %s\n", path);
        }
    } else {
        fprintf(stderr, "  ERROR creating phantom volume\n");
        ok = 0;
    }

    /* ---- 4. Checkerboard 32³ ---- */
    printf("[4/12] Checkerboard (32³, 8-bit)...\n");
    opj_volume_t *checker = make_checkerboard(32, 4);
    if (checker) {
        build_path(path, sizeof(path), dir,
                   "checkerboard_32x32x32_8bit.raw");
        if (!opj_raw_io_write(checker, path)) {
            fprintf(stderr, "  ERROR writing %s\n", path);
            ok = 0;
        } else {
            printf("  Wrote %s\n", path);
        }
    } else {
        fprintf(stderr, "  ERROR creating checkerboard volume\n");
        ok = 0;
    }

    /* ---- 5. RGB sphere 64³ ---- */
    printf("[5/12] RGB sphere (64³, 8-bit, 3 comps)...\n");
    opj_volume_t *rgb = make_rgb_sphere(64);
    if (rgb) {
        build_path(path, sizeof(path), dir,
                   "rgb_sphere_64x64x64_8bit.raw");
        if (!opj_raw_io_write(rgb, path)) {
            fprintf(stderr, "  ERROR writing %s\n", path);
            ok = 0;
        } else {
            printf("  Wrote %s\n", path);
        }
    } else {
        fprintf(stderr, "  ERROR creating RGB sphere volume\n");
        ok = 0;
    }

    /* ---- 6. Anatomical Heart 128³ 8-bit ---- */
    printf("[6/12] Anatomical heart (128³, 8-bit)...\n");
    opj_volume_t *heart = make_heart(128);
    if (heart) {
        build_path(path, sizeof(path), dir, "heart_128x128x128_8bit.raw");
        if (!opj_raw_io_write(heart, path)) {
            fprintf(stderr, "  ERROR writing %s\n", path);
            ok = 0;
        } else {
            printf("  Wrote %s\n", path);
        }
    } else {
        fprintf(stderr, "  ERROR creating heart volume\n");
        ok = 0;
    }

    /* ---- 7. RGB Heart 128³ ---- */
    printf("[7/12] RGB anatomical heart (128³, 8-bit, 3 comps)...\n");
    opj_volume_t *rgb_heart = make_rgb_heart(128);
    if (rgb_heart) {
        build_path(path, sizeof(path), dir,
                   "rgb_heart_128x128x128_8bit.raw");
        if (!opj_raw_io_write(rgb_heart, path)) {
            fprintf(stderr, "  ERROR writing %s\n", path);
            ok = 0;
        } else {
            printf("  Wrote %s\n", path);
        }
    } else {
        fprintf(stderr, "  ERROR creating RGB heart volume\n");
        ok = 0;
    }

    /* ---- 8. JP3D lossless (from sphere) ---- */
    printf("[8/12] Sphere lossless JP3D...\n");
    if (sphere) {
        build_path(path, sizeof(path), dir, "sphere_lossless.jp3d");
        if (!write_jp3d(sphere, path, 0)) ok = 0;
    }

    /* ---- 7. JP3D lossy (from sphere) ---- */
    printf("[9/12] Sphere lossy JP3D...\n");
    if (sphere) {
        build_path(path, sizeof(path), dir, "sphere_lossy.jp3d");
        if (!write_jp3d(sphere, path, 1)) ok = 0;
    }

    /* ---- 8. JP3D lossless (from gradient, 16-bit) ---- */
    printf("[10/12] Gradient lossless JP3D (16-bit)...\n");
    if (grad) {
        build_path(path, sizeof(path), dir, "gradient_lossless.jp3d");
        if (!write_jp3d(grad, path, 0)) ok = 0;
    }

    /* ---- 9. JP3D lossless (from phantom) ---- */
    printf("[11/12] Phantom lossless JP3D...\n");
    if (phantom) {
        build_path(path, sizeof(path), dir, "phantom_lossless.jp3d");
        if (!write_jp3d(phantom, path, 0)) ok = 0;
    }

    /* ---- 10. JP3D lossless (from heart) ---- */
    printf("[12/12] Heart lossless JP3D...\n");
    if (heart) {
        build_path(path, sizeof(path), dir, "heart_lossless.jp3d");
        if (!write_jp3d(heart, path, 0)) ok = 0;
    }

    /* ---- README ---- */
    write_readme(dir);

    /* ---- Cleanup ---- */
    if (sphere)    opj_jp3d_destroy_volume(sphere);
    if (grad)      opj_jp3d_destroy_volume(grad);
    if (phantom)   opj_jp3d_destroy_volume(phantom);
    if (checker)   opj_jp3d_destroy_volume(checker);
    if (rgb)       opj_jp3d_destroy_volume(rgb);
    if (heart)     opj_jp3d_destroy_volume(heart);
    if (rgb_heart) opj_jp3d_destroy_volume(rgb_heart);

    printf("\n%s\n", ok ? "All test data generated successfully."
                        : "Some files failed — see errors above.");
    return ok ? 0 : 1;
}

