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
 * @file gui_volume.cpp
 * @brief Volume state management, slice extraction, statistics, and
 *        OpenGL rendering helpers for Phase 8B of the OpenJP3D GUI.
 */

#include "gui_volume.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cassert>
#include <algorithm>
#include <vector>

/* ================================================================== */
/*  Lifecycle                                                         */
/* ================================================================== */

void gui_volume_state_init(GuiVolumeState *s)
{
    memset(s, 0, sizeof(*s));
    s->win_center = 128.0f;
    s->win_width  = 256.0f;
    s->auto_wl    = true;
    s->rc_azimuth   = 30.0f;
    s->rc_elevation = 20.0f;
    s->rc_density   = 1.0f;
}

void gui_volume_state_free(GuiVolumeState *s)
{
    if (s->vol) {
        opj_jp3d_destroy_volume(s->vol);
        s->vol    = NULL;
        s->loaded = false;
    }
    if (s->tex_id) {
        glDeleteTextures(1, &s->tex_id);
        s->tex_id = 0;
    }
    if (s->vol_tex_id) {
        glDeleteTextures(1, &s->vol_tex_id);
        s->vol_tex_id = 0;
    }
    if (s->rc_color_tex) {
        glDeleteTextures(1, &s->rc_color_tex);
        s->rc_color_tex = 0;
    }
    if (s->rc_fbo) {
        glDeleteFramebuffers(1, &s->rc_fbo);
        s->rc_fbo = 0;
    }
    if (s->rc_program) {
        glDeleteProgram(s->rc_program);
        s->rc_program = 0;
    }
    if (s->comp_stats) {
        free(s->comp_stats);
        s->comp_stats = NULL;
    }

    /* Reset viewer state but keep window preferences */
    float wc = s->win_center;
    float ww = s->win_width;
    bool awl  = s->auto_wl;
    gui_volume_state_init(s);
    s->win_center = wc;
    s->win_width  = ww;
    s->auto_wl    = awl;
}

/* ================================================================== */
/*  Volume loading                                                    */
/* ================================================================== */

/* Forward declaration of the JPIP3D message callback adapter */
static void load_msg_cb(opj_jp3d_msg_level_t /*level*/,
                        const char * /*msg*/, void * /*data*/) {}

bool gui_volume_load_raw(GuiVolumeState *s,
                         const char *path,
                         const RawOpenParams *p,
                         char *errbuf, size_t errlen)
{
    gui_volume_state_free(s);

    if (!p->width || !p->height || !p->depth || !p->prec || !p->numcomps) {
        snprintf(errbuf, errlen, "Invalid raw parameters (zero dimension or prec).");
        return false;
    }

    opj_volume_t *vol = opj_jp3d_create_volume(
        p->numcomps, p->width, p->height, p->depth, p->prec, p->sgnd);
    if (!vol) {
        snprintf(errbuf, errlen, "Failed to allocate volume (%ux%ux%u x%u).",
                 p->width, p->height, p->depth, p->numcomps);
        return false;
    }

    if (!opj_raw_io_read(vol, path)) {
        opj_jp3d_destroy_volume(vol);
        snprintf(errbuf, errlen, "Failed to read raw file: %s", path);
        return false;
    }

    s->vol    = vol;
    s->loaded = true;
    s->is_jp3d    = false;
    s->htj2k_mode = false;
    s->filter_type    = OPJ_JP3D_FILTER_53;
    s->num_resolutions = 0;
    s->slice_axis = SLICE_AXIS_Z;
    s->slice_idx  = (int)(p->depth / 2);  /* Start at middle slice */
    s->tex_dirty     = true;
    s->vol_tex_dirty = true;
    s->auto_wl       = true;
    strncpy(s->filepath, path, sizeof(s->filepath) - 1);
    s->filepath[sizeof(s->filepath) - 1] = '\0';

    gui_volume_compute_stats(s);
    return true;
}

bool gui_volume_load_jp3d(GuiVolumeState *s,
                          const char *path,
                          char *errbuf, size_t errlen)
{
    gui_volume_state_free(s);

    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(errbuf, errlen, "Cannot open file: %s", path);
        return false;
    }
    fseek(f, 0, SEEK_END);
    long fsz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsz <= 0) {
        fclose(f);
        snprintf(errbuf, errlen, "File is empty: %s", path);
        return false;
    }

    std::vector<uint8_t> buf((size_t)fsz);
    if (fread(buf.data(), 1, (size_t)fsz, f) != (size_t)fsz) {
        fclose(f);
        snprintf(errbuf, errlen, "Read error: %s", path);
        return false;
    }
    fclose(f);

    opj_jp3d_dec_params_t dec = {};
    dec.verbose = 0;
    opj_volume_t *vol = opj_jp3d_decode(buf.data(), buf.size(),
                                        &dec, load_msg_cb, NULL);
    if (!vol) {
        snprintf(errbuf, errlen, "opj_jp3d_decode failed for: %s", path);
        return false;
    }

    s->vol    = vol;
    s->loaded = true;
    s->is_jp3d = true;
    /* Detect HTJ2K / filter from codestream marker (approximate detection) */
    s->htj2k_mode  = false;
    s->filter_type = OPJ_JP3D_FILTER_53;
    s->num_resolutions = 0;
    if (buf.size() >= 4) {
        /* Scan for COD3D marker 0xFF91 */
        for (size_t i = 0; i + 12 < buf.size(); ++i) {
            if (buf[i] == 0xFF && buf[i + 1] == 0x91) {
                if (i + 6 < buf.size()) {
                    s->filter_type    = (buf[i + 4] == 1) ? OPJ_JP3D_FILTER_97
                                                           : OPJ_JP3D_FILTER_53;
                    s->htj2k_mode     = (buf[i + 5] != 0);
                    s->num_resolutions = buf[i + 6];
                }
                break;
            }
        }
    }
    s->slice_axis = SLICE_AXIS_Z;
    s->slice_idx  = (int)(vol->comps[0].d / 2);  /* Start at middle slice */
    s->tex_dirty     = true;
    s->vol_tex_dirty = true;
    s->auto_wl       = true;
    strncpy(s->filepath, path, sizeof(s->filepath) - 1);
    s->filepath[sizeof(s->filepath) - 1] = '\0';

    gui_volume_compute_stats(s);
    return true;
}

/* ================================================================== */
/*  Statistics                                                        */
/* ================================================================== */

void gui_volume_compute_stats(GuiVolumeState *s)
{
    if (!s->vol || !s->loaded) return;

    uint32_t nc = s->vol->numcomps;
    if (s->comp_stats) {
        free(s->comp_stats);
    }
    s->comp_stats = (GuiCompStats *)calloc(nc, sizeof(GuiCompStats));
    if (!s->comp_stats) return;

    for (uint32_t c = 0; c < nc; ++c) {
        const opj_volume_comp_t *comp = &s->vol->comps[c];
        size_t n = (size_t)comp->w * comp->h * comp->d;
        if (!comp->data || n == 0) {
            s->comp_stats[c].computed = false;
            continue;
        }

        double sum = 0.0, sum2 = 0.0;
        int32_t mn = comp->data[0], mx = comp->data[0];
        for (size_t i = 0; i < n; ++i) {
            int32_t v = comp->data[i];
            if (v < mn) mn = v;
            if (v > mx) mx = v;
            sum  += v;
            sum2 += (double)v * v;
        }
        double mean   = sum / (double)n;
        double var    = sum2 / (double)n - mean * mean;
        double stddev = (var > 0.0) ? sqrt(var) : 0.0;

        GuiCompStats *st = &s->comp_stats[c];
        st->min_val = (float)mn;
        st->max_val = (float)mx;
        st->mean    = (float)mean;
        st->stddev  = (float)stddev;
        st->computed = true;

        /* Build normalised histogram */
        memset(st->histogram, 0, sizeof(st->histogram));
        double range = (double)(mx - mn);
        if (range < 1.0) range = 1.0;
        for (size_t i = 0; i < n; ++i) {
            int bin = (int)(((double)(comp->data[i] - mn) / range) *
                            (GUI_HIST_BINS - 1));
            if (bin < 0) bin = 0;
            if (bin >= GUI_HIST_BINS) bin = GUI_HIST_BINS - 1;
            st->histogram[bin] += 1.0f;
        }
        /* Normalise to [0, 1] */
        float hist_max = 1.0f;
        for (int b = 0; b < GUI_HIST_BINS; ++b)
            if (st->histogram[b] > hist_max) hist_max = st->histogram[b];
        for (int b = 0; b < GUI_HIST_BINS; ++b)
            st->histogram[b] /= hist_max;
    }

    /* Auto window/level from component 0 */
    if (s->auto_wl && s->comp_stats[0].computed) {
        float mn = s->comp_stats[0].min_val;
        float mx = s->comp_stats[0].max_val;
        s->win_width  = (mx - mn) > 0.0f ? (mx - mn) : 1.0f;
        s->win_center = mn + s->win_width * 0.5f;
    }
    s->tex_dirty     = true;
    s->vol_tex_dirty = true;
}

/* ================================================================== */
/*  Slice texture                                                     */
/* ================================================================== */

int gui_volume_axis_depth(const GuiVolumeState *s)
{
    if (!s->vol || !s->loaded || !s->vol->numcomps) return 1;
    const opj_volume_comp_t *c0 = &s->vol->comps[0];
    switch (s->slice_axis) {
    case SLICE_AXIS_Z: return (int)c0->d;
    case SLICE_AXIS_X: return (int)c0->w;
    case SLICE_AXIS_Y: return (int)c0->h;
    }
    return 1;
}

void gui_volume_clamp_slice(GuiVolumeState *s)
{
    int depth = gui_volume_axis_depth(s);
    if (s->slice_idx < 0)          s->slice_idx = 0;
    if (s->slice_idx >= depth)     s->slice_idx = depth - 1;
}

/**
 * Apply window/level to a raw integer sample and return a uint8 value.
 * out = clamp((v - (center - width/2)) / width, 0, 1) * 255
 */
static uint8_t wl_to_u8(int32_t v, float center, float width)
{
    float lo = center - width * 0.5f;
    float t  = ((float)v - lo) / width;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return (uint8_t)(t * 255.0f + 0.5f);
}

void gui_volume_update_slice_texture(GuiVolumeState *s)
{
    if (!s->vol || !s->loaded) return;

    const opj_volume_comp_t *c0 = &s->vol->comps[0];
    uint32_t W = c0->w, H = c0->h, D = c0->d;
    uint32_t nc = s->vol->numcomps;


    int tex_w, tex_h;
    switch (s->slice_axis) {
    case SLICE_AXIS_Z: tex_w = (int)W; tex_h = (int)H; break;
    case SLICE_AXIS_X: tex_w = (int)D; tex_h = (int)H; break;
    case SLICE_AXIS_Y: tex_w = (int)W; tex_h = (int)D; break;
    default:           tex_w = (int)W; tex_h = (int)H; break;
    }

    int slice = s->slice_idx;
    /* Allocate RGBA pixel buffer */
    size_t npix = (size_t)tex_w * tex_h;
    std::vector<uint8_t> pixels(npix * 4);

    auto getv = [&](const opj_volume_comp_t *comp, int x, int y, int z) -> int32_t {
        int32_t idx = z * (int32_t)comp->w * (int32_t)comp->h
                    + y * (int32_t)comp->w + x;
        return comp->data[idx];
    };

    bool is_rgb = (nc >= 3);

    for (int row = 0; row < tex_h; ++row) {
        for (int col = 0; col < tex_w; ++col) {
            int x, y, z;
            switch (s->slice_axis) {
            case SLICE_AXIS_Z:
                x = col; y = row; z = slice;
                break;
            case SLICE_AXIS_X:
                x = slice; y = row; z = col;
                break;
            case SLICE_AXIS_Y:
                x = col; y = slice; z = row;
                break;
            default:
                x = col; y = row; z = slice;
                break;
            }

            size_t pidx = ((size_t)row * tex_w + col) * 4;
            if (is_rgb) {
                pixels[pidx + 0] = wl_to_u8(getv(&s->vol->comps[0], x, y, z),
                                             s->win_center, s->win_width);
                pixels[pidx + 1] = wl_to_u8(getv(&s->vol->comps[1], x, y, z),
                                             s->win_center, s->win_width);
                pixels[pidx + 2] = wl_to_u8(getv(&s->vol->comps[2], x, y, z),
                                             s->win_center, s->win_width);
                pixels[pidx + 3] = 255;
            } else {
                uint8_t lum = wl_to_u8(getv(&s->vol->comps[0], x, y, z),
                                       s->win_center, s->win_width);
                pixels[pidx + 0] = lum;
                pixels[pidx + 1] = lum;
                pixels[pidx + 2] = lum;
                pixels[pidx + 3] = 255;
            }
        }
    }

    /* Upload to OpenGL */

    if (!s->tex_id) {
        glGenTextures(1, &s->tex_id);
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glBindTexture(GL_TEXTURE_2D, s->tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#if defined(__APPLE__)
    /* macOS Core Profile requires explicit swizzle for RGBA textures */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
#endif
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 tex_w, tex_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);


    s->tex_w     = tex_w;
    s->tex_h     = tex_h;
    s->tex_dirty = false;
}

/* ================================================================== */
/*  3-D texture for ray-casting                                       */
/* ================================================================== */

void gui_volume_build_3d_texture(GuiVolumeState *s)
{
    if (!s->vol || !s->loaded) return;

    const opj_volume_comp_t *c0 = &s->vol->comps[0];
    uint32_t W = c0->w, H = c0->h, D = c0->d;
    size_t n = (size_t)W * H * D;

    /* Normalise component 0 to uint8 for the 3-D texture */
    float mn = s->comp_stats ? s->comp_stats[0].min_val : 0.0f;
    float mx = s->comp_stats ? s->comp_stats[0].max_val : 255.0f;
    float rng = (mx - mn) > 0.0f ? (mx - mn) : 1.0f;

    std::vector<uint8_t> vol8(n);
    for (size_t i = 0; i < n; ++i) {
        float t = ((float)c0->data[i] - mn) / rng;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        vol8[i] = (uint8_t)(t * 255.0f + 0.5f);
    }

    if (!s->vol_tex_id) {
        glGenTextures(1, &s->vol_tex_id);
    }
    glBindTexture(GL_TEXTURE_3D, s->vol_tex_id);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R8,
                 (GLsizei)W, (GLsizei)H, (GLsizei)D,
                 0, GL_RED, GL_UNSIGNED_BYTE, vol8.data());
    glBindTexture(GL_TEXTURE_3D, 0);

    s->vol_tex_dirty = false;
}

/* ================================================================== */
/*  Ray-cast renderer                                                 */
/* ================================================================== */

/* ---- GLSL source strings ---- */

static const char *kRcVertSrc =
#if defined(__APPLE__)
    "#version 150\n"
#else
    "#version 130\n"
#endif
    "in  vec2 aPos;\n"
    "out vec2 vUV;\n"
    "void main() {\n"
    "    vUV = aPos * 0.5 + 0.5;\n"
    "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
    "}\n";

static const char *kRcFragSrc =
#if defined(__APPLE__)
    "#version 150\n"
#else
    "#version 130\n"
#endif
    "uniform sampler3D uVolume;\n"
    "uniform mat4      uInvMVP;\n"
    "uniform float     uDensity;\n"
    "in  vec2 vUV;\n"
    "out vec4 fragColor;\n"
    "\n"
    "vec2 intersect_box(vec3 orig, vec3 dir) {\n"
    "    const vec3 box_min = vec3(0.0);\n"
    "    const vec3 box_max = vec3(1.0);\n"
    "    vec3 inv_dir = 1.0 / dir;\n"
    "    vec3 tmin = (box_min - orig) * inv_dir;\n"
    "    vec3 tmax = (box_max - orig) * inv_dir;\n"
    "    vec3 t0 = min(tmin, tmax);\n"
    "    vec3 t1 = max(tmin, tmax);\n"
    "    return vec2(max(max(t0.x, t0.y), t0.z),\n"
    "                min(min(t1.x, t1.y), t1.z));\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    // Reconstruct ray in object (volume) space\n"
    "    vec4 ndc     = vec4(vUV * 2.0 - 1.0, -1.0, 1.0);\n"
    "    vec4 obj_far = uInvMVP * vec4(vUV * 2.0 - 1.0,  1.0, 1.0);\n"
    "    vec4 obj_near= uInvMVP * ndc;\n"
    "    vec3 ro = obj_near.xyz / obj_near.w;\n"
    "    vec3 rd = normalize(obj_far.xyz / obj_far.w - ro);\n"
    "\n"
    "    vec2 t = intersect_box(ro, rd);\n"
    "    if (t.x >= t.y) { fragColor = vec4(0.0); return; }\n"
    "    t.x = max(t.x, 0.0);\n"
    "\n"
    "    int   STEPS = 128;\n"
    "    float step  = (t.y - t.x) / float(STEPS);\n"
    "    vec4  accum = vec4(0.0);\n"
    "    for (int i = 0; i < STEPS; ++i) {\n"
    "        vec3  pos = ro + rd * (t.x + (float(i) + 0.5) * step);\n"
    "        float val = texture(uVolume, pos).r;\n"
    "        float a   = val * uDensity * step * 4.0;\n"
    "        a = clamp(a, 0.0, 1.0);\n"
    "        accum += (1.0 - accum.a) * vec4(val, val, val, a);\n"
    "        if (accum.a > 0.99) break;\n"
    "    }\n"
    "    fragColor = accum;\n"
    "}\n";

static GLuint compile_shader(GLenum type, const char *src)
{
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(sh, sizeof(log), NULL, log);
        fprintf(stderr, "[gui_volume] Shader compile error: %s\n", log);
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

static GLuint link_program(GLuint vert, GLuint frag)
{
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glBindAttribLocation(prog, 0, "aPos");
    glLinkProgram(prog);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), NULL, log);
        fprintf(stderr, "[gui_volume] Program link error: %s\n", log);
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

/* Simple column-major 4×4 float matrix */
struct Mat4 { float m[16]; };

static Mat4 mat4_identity()
{
    Mat4 r = {};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

static Mat4 mat4_mul(const Mat4 &a, const Mat4 &b)
{
    Mat4 c = {};
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            for (int k = 0; k < 4; ++k)
                c.m[col * 4 + row] += a.m[k * 4 + row] * b.m[col * 4 + k];
    return c;
}

static Mat4 mat4_rotate_y(float deg)
{
    float r = deg * 3.14159265f / 180.0f;
    Mat4 m = mat4_identity();
    m.m[0]  =  cosf(r);
    m.m[8]  = -sinf(r);
    m.m[2]  =  sinf(r);
    m.m[10] =  cosf(r);
    return m;
}

static Mat4 mat4_rotate_x(float deg)
{
    float r = deg * 3.14159265f / 180.0f;
    Mat4 m = mat4_identity();
    m.m[5]  =  cosf(r);
    m.m[9]  = -sinf(r);
    m.m[6]  =  sinf(r);
    m.m[10] =  cosf(r);
    return m;
}

static Mat4 mat4_translate(float tx, float ty, float tz)
{
    Mat4 m = mat4_identity();
    m.m[12] = tx; m.m[13] = ty; m.m[14] = tz;
    return m;
}

static Mat4 mat4_perspective(float fov_deg, float aspect, float znear, float zfar)
{
    float f = 1.0f / tanf(fov_deg * 0.5f * 3.14159265f / 180.0f);
    Mat4 m = {};
    m.m[0]  = f / aspect;
    m.m[5]  = f;
    m.m[10] = (zfar + znear) / (znear - zfar);
    m.m[11] = -1.0f;
    m.m[14] = (2.0f * zfar * znear) / (znear - zfar);
    return m;
}

static bool mat4_invert(const Mat4 &src, Mat4 &dst)
{
    /* Standard cofactor inversion for 4×4 */
    const float *m = src.m;
    float inv[16];

    inv[0]  =  m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15]
             + m[9]*m[7]*m[14]  + m[13]*m[6]*m[11]  - m[13]*m[7]*m[10];
    inv[4]  = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14]  + m[8]*m[6]*m[15]
             - m[8]*m[7]*m[14]  - m[12]*m[6]*m[11]  + m[12]*m[7]*m[10];
    inv[8]  =  m[4]*m[9]*m[15]  - m[4]*m[11]*m[13]  - m[8]*m[5]*m[15]
             + m[8]*m[7]*m[13]  + m[12]*m[5]*m[11]  - m[12]*m[7]*m[9];
    inv[12] = -m[4]*m[9]*m[14]  + m[4]*m[10]*m[13]  + m[8]*m[5]*m[14]
             - m[8]*m[6]*m[13]  - m[12]*m[5]*m[10]  + m[12]*m[6]*m[9];
    inv[1]  = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14]  + m[9]*m[2]*m[15]
             - m[9]*m[3]*m[14]  - m[13]*m[2]*m[11]  + m[13]*m[3]*m[10];
    inv[5]  =  m[0]*m[10]*m[15] - m[0]*m[11]*m[14]  - m[8]*m[2]*m[15]
             + m[8]*m[3]*m[14]  + m[12]*m[2]*m[11]  - m[12]*m[3]*m[10];
    inv[9]  = -m[0]*m[9]*m[15]  + m[0]*m[11]*m[13]  + m[8]*m[1]*m[15]
             - m[8]*m[3]*m[13]  - m[12]*m[1]*m[11]  + m[12]*m[3]*m[9];
    inv[13] =  m[0]*m[9]*m[14]  - m[0]*m[10]*m[13]  - m[8]*m[1]*m[14]
             + m[8]*m[2]*m[13]  + m[12]*m[1]*m[10]  - m[12]*m[2]*m[9];
    inv[2]  =  m[1]*m[6]*m[15]  - m[1]*m[7]*m[14]   - m[5]*m[2]*m[15]
             + m[5]*m[3]*m[14]  + m[13]*m[2]*m[7]   - m[13]*m[3]*m[6];
    inv[6]  = -m[0]*m[6]*m[15]  + m[0]*m[7]*m[14]   + m[4]*m[2]*m[15]
             - m[4]*m[3]*m[14]  - m[12]*m[2]*m[7]   + m[12]*m[3]*m[6];
    inv[10] =  m[0]*m[5]*m[15]  - m[0]*m[7]*m[13]   - m[4]*m[1]*m[15]
             + m[4]*m[3]*m[13]  + m[12]*m[1]*m[7]   - m[12]*m[3]*m[5];
    inv[14] = -m[0]*m[5]*m[14]  + m[0]*m[6]*m[13]   + m[4]*m[1]*m[14]
             - m[4]*m[2]*m[13]  - m[12]*m[1]*m[6]   + m[12]*m[2]*m[5];
    inv[3]  = -m[1]*m[6]*m[11]  + m[1]*m[7]*m[10]   + m[5]*m[2]*m[11]
             - m[5]*m[3]*m[10]  - m[9]*m[2]*m[7]    + m[9]*m[3]*m[6];
    inv[7]  =  m[0]*m[6]*m[11]  - m[0]*m[7]*m[10]   - m[4]*m[2]*m[11]
             + m[4]*m[3]*m[10]  + m[8]*m[2]*m[7]    - m[8]*m[3]*m[6];
    inv[11] = -m[0]*m[5]*m[11]  + m[0]*m[7]*m[9]    + m[4]*m[1]*m[11]
             - m[4]*m[3]*m[9]   - m[8]*m[1]*m[7]    + m[8]*m[3]*m[5];
    inv[15] =  m[0]*m[5]*m[10]  - m[0]*m[6]*m[9]    - m[4]*m[1]*m[10]
             + m[4]*m[2]*m[9]   + m[8]*m[1]*m[6]    - m[8]*m[2]*m[5];

    float det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];
    if (fabsf(det) < 1e-8f) return false;
    float inv_det = 1.0f / det;
    for (int i = 0; i < 16; ++i) dst.m[i] = inv[i] * inv_det;
    return true;
}

/* Full-screen triangle VBO (created once) */
static GLuint s_fs_vao = 0;
static GLuint s_fs_vbo = 0;

static void ensure_fs_quad()
{
    if (s_fs_vao) return;
    static const float verts[8] = { -1,-1,  1,-1,  -1,1,  1,1 };
    glGenVertexArrays(1, &s_fs_vao);
    glGenBuffers(1, &s_fs_vbo);
    glBindVertexArray(s_fs_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_fs_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);
    glBindVertexArray(0);
}

void gui_volume_render_raycast(GuiVolumeState *s, int width, int height)
{
    if (!s->vol || !s->loaded || width <= 0 || height <= 0) return;

    /* Build 3-D texture if dirty */
    if (s->vol_tex_dirty || !s->vol_tex_id) {
        gui_volume_build_3d_texture(s);
    }
    if (!s->vol_tex_id) return;

    /* Compile shader program once */
    if (!s->rc_program) {
        GLuint vert = compile_shader(GL_VERTEX_SHADER,   kRcVertSrc);
        GLuint frag = compile_shader(GL_FRAGMENT_SHADER, kRcFragSrc);
        if (vert && frag) {
            s->rc_program = link_program(vert, frag);
        }
        if (vert) glDeleteShader(vert);
        if (frag) glDeleteShader(frag);
        if (!s->rc_program) return;
    }

    /* (Re)create FBO + colour texture if size changed */
    if (!s->rc_fbo || s->rc_w != width || s->rc_h != height) {
        if (s->rc_fbo)       glDeleteFramebuffers(1, &s->rc_fbo);
        if (s->rc_color_tex) glDeleteTextures(1, &s->rc_color_tex);
        glGenFramebuffers(1, &s->rc_fbo);
        glGenTextures(1, &s->rc_color_tex);
        glBindTexture(GL_TEXTURE_2D, s->rc_color_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                     width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, s->rc_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, s->rc_color_tex, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        s->rc_w = width;
        s->rc_h = height;
    }

    ensure_fs_quad();

    /* Build MVP: translate to centre box at origin, rotate, perspective */
    Mat4 T   = mat4_translate(-0.5f, -0.5f, -0.5f);
    Mat4 Ry  = mat4_rotate_y(s->rc_azimuth);
    Mat4 Rx  = mat4_rotate_x(-s->rc_elevation);
    Mat4 Tz  = mat4_translate(0.0f, 0.0f, -2.0f);
    Mat4 Proj= mat4_perspective(45.0f, (float)width / (float)height,
                                0.01f, 10.0f);
    Mat4 MVP = mat4_mul(Proj, mat4_mul(Tz, mat4_mul(Rx, mat4_mul(Ry, T))));
    Mat4 invMVP;
    if (!mat4_invert(MVP, invMVP)) return;

    /* Render */
    GLint prev_fbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, s->rc_fbo);
    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(s->rc_program);

    /* Bind 3-D volume texture to unit 0 */
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, s->vol_tex_id);
    glUniform1i(glGetUniformLocation(s->rc_program, "uVolume"), 0);

    glUniformMatrix4fv(glGetUniformLocation(s->rc_program, "uInvMVP"),
                       1, GL_FALSE, invMVP.m);
    glUniform1f(glGetUniformLocation(s->rc_program, "uDensity"), s->rc_density);

    glBindVertexArray(s_fs_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);
}
