/*
 * Copyright (c) 2024-2026, OpenJP3D Contributors
 * All rights reserved.
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * openjp3d_wrap.c — Thin C wrapper for the OpenJP3D shared library.
 *
 * The openjp3d library is loaded at runtime (not at compile/link time) via
 * dlopen()/LoadLibrary().  All entry points are resolved with dlsym() /
 * GetProcAddress().  This allows the R package to be compiled and installed
 * without having the openjp3d shared library present on the build machine.
 *
 * Exported .Call() entry points (registered in R_init_openjp3d):
 *
 *   ojp3d_load_lib(path)
 *   ojp3d_get_version()
 *   ojp3d_encode(data_int, w, h, d, numcomps, prec, sgnd,
 *                params_int, target_rate, cb_sxp)
 *   ojp3d_decode(raw_data, verbose, cb_sxp)
 *   ojp3d_transcode(raw_data, has_params, params_int, target_rate, cb_sxp)
 */

#include <R.h>
#include <Rinternals.h>
#include <R_ext/Rdynload.h>

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* =========================================================================
 * Platform dynamic-loading shims
 * ========================================================================= */

#ifdef _WIN32
#  include <windows.h>
typedef HMODULE lib_handle_t;
static lib_handle_t lib_open(const char *p) { return LoadLibraryA(p); }
static void *lib_sym(lib_handle_t h, const char *n) {
    return (void *)GetProcAddress(h, n);
}
static void lib_close(lib_handle_t h) { FreeLibrary(h); }
static const char *lib_last_error(void) {
    static char buf[256];
    DWORD e = GetLastError();
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, e, 0, buf, (DWORD)sizeof(buf), NULL);
    return buf;
}
#else
#  include <dlfcn.h>
typedef void *lib_handle_t;
static lib_handle_t lib_open(const char *p) {
    return dlopen(p, RTLD_LAZY | RTLD_LOCAL);
}
static void *lib_sym(lib_handle_t h, const char *n) { return dlsym(h, n); }
static void lib_close(lib_handle_t h) { (void)dlclose(h); }
static const char *lib_last_error(void) { return dlerror(); }
#endif

/* =========================================================================
 * C struct mirrors — layout must match openjp3d.h exactly
 * ========================================================================= */

typedef struct {
    uint32_t  w, h, d, prec;
    int32_t   sgnd;
    float     dz;
    int32_t  *data;
} ojp3d_volume_comp_t;

typedef struct {
    uint32_t            numcomps;
    ojp3d_volume_comp_t *comps;
    uint32_t            x0, y0, z0;
    uint32_t            x1, y1, z1;
    uint32_t            color_space;
} ojp3d_volume_t;

typedef struct {
    uint32_t tile_width;
    uint32_t tile_height;
    uint32_t tile_depth;
    uint32_t num_resolutions_x;
    uint32_t num_resolutions_y;
    uint32_t num_resolutions_z;
    uint32_t cblk_width;
    uint32_t cblk_height;
    uint32_t cblk_depth;
    int32_t  filter;
    uint32_t num_layers;
    float    target_rate;
    int32_t  use_htj2k;
    int32_t  verbose;
} ojp3d_enc_params_t;

typedef struct {
    int32_t verbose;
} ojp3d_dec_params_t;

/* =========================================================================
 * Function pointer types
 * ========================================================================= */

typedef const char       *(*fn_get_version_t)   (void);
typedef ojp3d_volume_t   *(*fn_create_volume_t) (uint32_t, uint32_t,
                                                  uint32_t, uint32_t,
                                                  uint32_t, int32_t);
typedef void              (*fn_destroy_volume_t)(ojp3d_volume_t *);
typedef int32_t           (*fn_encode_t)        (const ojp3d_volume_t *,
                                                  const ojp3d_enc_params_t *,
                                                  uint8_t **, size_t *,
                                                  void *, void *);
typedef ojp3d_volume_t   *(*fn_decode_t)        (const uint8_t *, size_t,
                                                  const ojp3d_dec_params_t *,
                                                  void *, void *);
typedef int32_t           (*fn_transcode_t)     (const uint8_t *, size_t,
                                                  const ojp3d_enc_params_t *,
                                                  uint8_t **, size_t *,
                                                  void *, void *);
typedef void              (*fn_free_t)          (void *);

/* =========================================================================
 * Module-level state
 * ========================================================================= */

static lib_handle_t        s_lib            = (lib_handle_t)0;
static fn_get_version_t    s_get_version    = NULL;
static fn_create_volume_t  s_create_volume  = NULL;
static fn_destroy_volume_t s_destroy_volume = NULL;
static fn_encode_t         s_encode         = NULL;
static fn_decode_t         s_decode         = NULL;
static fn_transcode_t      s_transcode      = NULL;
static fn_free_t           s_jp3d_free      = NULL;

/* R callback slot — initialised in R_init_openjp3d */
static SEXP s_r_callback;

/* =========================================================================
 * Internal: C-side message callback that calls back into R
 * ========================================================================= */

static void c_msg_callback(int level, const char *msg, void *data)
{
    (void)data;
    if (s_r_callback == R_NilValue) return;
    if (!msg) msg = "";
    SEXP call = PROTECT(lang3(s_r_callback,
                              ScalarInteger(level),
                              mkString(msg)));
    eval(call, R_GlobalEnv);
    UNPROTECT(1);
}

/* =========================================================================
 * ojp3d_load_lib(path_sxp) -> NULL
 *
 * Called from R's .onLoad().  Loads the shared library and resolves all
 * function pointers.
 * ========================================================================= */

SEXP ojp3d_load_lib(SEXP path_sxp)
{
    if (!Rf_isString(path_sxp) || Rf_length(path_sxp) < 1)
        Rf_error("openjp3d: path must be a character string");

    const char *path = CHAR(STRING_ELT(path_sxp, 0));

    if (s_lib) {
        lib_close(s_lib);
        s_lib = (lib_handle_t)0;
    }

    s_lib = lib_open(path);
    if (!s_lib) {
        const char *err = lib_last_error();
        Rf_error("openjp3d: cannot open library '%s': %s",
                 path, err ? err : "(unknown error)");
    }

#define LOAD_SYM(fn_var, c_name)                                        \
    do {                                                                 \
        s_##fn_var = (fn_##fn_var##_t) lib_sym(s_lib, #c_name);        \
        if (!s_##fn_var)                                                 \
            Rf_error("openjp3d: symbol '" #c_name "' not found");       \
    } while (0)

    LOAD_SYM(get_version,    opj_jp3d_get_version);
    LOAD_SYM(create_volume,  opj_jp3d_create_volume);
    LOAD_SYM(destroy_volume, opj_jp3d_destroy_volume);
    LOAD_SYM(encode,         opj_jp3d_encode);
    LOAD_SYM(decode,         opj_jp3d_decode);
    LOAD_SYM(transcode,      opj_jp3d_transcode_to_ht);

#undef LOAD_SYM

    /* load free separately to avoid macro name-mangling collision */
    s_jp3d_free = (fn_free_t) lib_sym(s_lib, "opj_jp3d_free");
    if (!s_jp3d_free)
        Rf_error("openjp3d: symbol 'opj_jp3d_free' not found");

    return R_NilValue;
}

/* =========================================================================
 * ojp3d_get_version() -> character(1)
 * ========================================================================= */

SEXP ojp3d_get_version(void)
{
    if (!s_get_version)
        Rf_error("openjp3d library not loaded; call .load_lib() first");
    const char *v = s_get_version();
    return mkString(v ? v : "");
}

/* =========================================================================
 * ojp3d_encode(data_int, w, h, d, numcomps, prec, sgnd,
 *              params_int, target_rate, cb_sxp)
 *  -> raw vector (JP3D codestream bytes)
 *
 * params_int: integer(13) = [tw, th, td, rx, ry, rz, bw, bh, bd,
 *                             filter, num_layers, use_htj2k, verbose]
 * target_rate: double(1)
 * data_int: integer vector of length w*h*d*numcomps in C row-major order
 *           (caller must already have permuted from R column-major)
 * ========================================================================= */

SEXP ojp3d_encode(SEXP data_sxp, SEXP w_sxp, SEXP h_sxp, SEXP d_sxp,
                  SEXP numcomps_sxp, SEXP prec_sxp, SEXP sgnd_sxp,
                  SEXP params_sxp, SEXP rate_sxp, SEXP cb_sxp)
{
    if (!s_encode)
        Rf_error("openjp3d library not loaded");

    uint32_t w        = (uint32_t) asInteger(w_sxp);
    uint32_t h        = (uint32_t) asInteger(h_sxp);
    uint32_t d        = (uint32_t) asInteger(d_sxp);
    uint32_t numcomps = (uint32_t) asInteger(numcomps_sxp);
    uint32_t prec     = (uint32_t) asInteger(prec_sxp);
    int32_t  sgnd     = (int32_t)  asInteger(sgnd_sxp);

    if (!Rf_isInteger(params_sxp) || Rf_length(params_sxp) < 13)
        Rf_error("openjp3d: params_int must be integer(13)");
    const int *pi = INTEGER(params_sxp);

    ojp3d_enc_params_t enc;
    enc.tile_width        = (uint32_t) pi[0];
    enc.tile_height       = (uint32_t) pi[1];
    enc.tile_depth        = (uint32_t) pi[2];
    enc.num_resolutions_x = (uint32_t) pi[3];
    enc.num_resolutions_y = (uint32_t) pi[4];
    enc.num_resolutions_z = (uint32_t) pi[5];
    enc.cblk_width        = (uint32_t) pi[6];
    enc.cblk_height       = (uint32_t) pi[7];
    enc.cblk_depth        = (uint32_t) pi[8];
    enc.filter            = (int32_t)  pi[9];
    enc.num_layers        = (uint32_t) pi[10];
    enc.target_rate       = (float)    asReal(rate_sxp);
    enc.use_htj2k         = (int32_t)  pi[11];
    enc.verbose           = (int32_t)  pi[12];

    size_t n_voxels = (size_t)w * h * d;
    size_t n_total  = n_voxels * numcomps;

    if ((size_t)Rf_length(data_sxp) < n_total)
        Rf_error("openjp3d: data vector too short (expected %zu, got %d)",
                 n_total, Rf_length(data_sxp));

    ojp3d_volume_t *vol = s_create_volume(numcomps, w, h, d, prec, sgnd);
    if (!vol) Rf_error("openjp3d: opj_jp3d_create_volume() returned NULL");

    const int *src = INTEGER(data_sxp);
    for (uint32_t c = 0; c < numcomps; c++) {
        int32_t *dst = vol->comps[c].data;
        const int *s = src + (size_t)c * n_voxels;
        for (size_t i = 0; i < n_voxels; i++)
            dst[i] = (int32_t) s[i];
    }

    /* Set up callback */
    void (*cb_fn)(int, const char *, void *) = NULL;
    if (cb_sxp != R_NilValue && !Rf_isNull(cb_sxp)) {
        s_r_callback = cb_sxp;
        cb_fn = c_msg_callback;
    }

    uint8_t *out_data = NULL;
    size_t   out_size = 0;
    int32_t  ok = s_encode(vol, &enc, &out_data, &out_size,
                           (void *)cb_fn, NULL);

    s_r_callback = R_NilValue;
    s_destroy_volume(vol);

    if (!ok || !out_data)
        Rf_error("openjp3d: opj_jp3d_encode() failed");

    SEXP result = PROTECT(Rf_allocVector(RAWSXP, (R_xlen_t)out_size));
    memcpy(RAW(result), out_data, out_size);
    s_jp3d_free(out_data);
    UNPROTECT(1);
    return result;
}

/* =========================================================================
 * ojp3d_decode(raw_data, verbose, cb_sxp)
 *  -> list(data=integer, w, h, d, numcomps, prec, sgnd)
 * ========================================================================= */

SEXP ojp3d_decode(SEXP raw_sxp, SEXP verbose_sxp, SEXP cb_sxp)
{
    if (!s_decode)
        Rf_error("openjp3d library not loaded");

    if (TYPEOF(raw_sxp) != RAWSXP)
        Rf_error("openjp3d: data must be a raw vector");

    const uint8_t *src  = RAW(raw_sxp);
    size_t         size = (size_t) Rf_length(raw_sxp);
    int            verb = asLogical(verbose_sxp);

    ojp3d_dec_params_t dec;
    dec.verbose = (int32_t)(verb == TRUE ? 1 : 0);

    void (*cb_fn)(int, const char *, void *) = NULL;
    if (cb_sxp != R_NilValue && !Rf_isNull(cb_sxp)) {
        s_r_callback = cb_sxp;
        cb_fn = c_msg_callback;
    }

    ojp3d_volume_t *vol = s_decode(src, size, &dec, (void *)cb_fn, NULL);
    s_r_callback = R_NilValue;

    if (!vol)
        Rf_error("openjp3d: opj_jp3d_decode() returned NULL — decoding failed");

    uint32_t numcomps = vol->numcomps;
    uint32_t w        = vol->comps[0].w;
    uint32_t h        = vol->comps[0].h;
    uint32_t d        = vol->comps[0].d;
    uint32_t prec     = vol->comps[0].prec;
    int32_t  sgnd     = vol->comps[0].sgnd;
    size_t   n_voxels = (size_t)w * h * d;
    size_t   n_total  = n_voxels * numcomps;

    SEXP data_sxp = PROTECT(Rf_allocVector(INTSXP, (R_xlen_t)n_total));
    int *dst = INTEGER(data_sxp);

    for (uint32_t c = 0; c < numcomps; c++) {
        const int32_t *src2 = vol->comps[c].data;
        int *d2 = dst + (size_t)c * n_voxels;
        for (size_t i = 0; i < n_voxels; i++)
            d2[i] = (int) src2[i];
    }
    s_destroy_volume(vol);

    /* Build return list */
    const char *names[] = { "data", "w", "h", "d",
                             "numcomps", "prec", "sgnd", "" };
    SEXP result = PROTECT(Rf_mkNamed(VECSXP, names));
    SET_VECTOR_ELT(result, 0, data_sxp);
    SET_VECTOR_ELT(result, 1, ScalarInteger((int)w));
    SET_VECTOR_ELT(result, 2, ScalarInteger((int)h));
    SET_VECTOR_ELT(result, 3, ScalarInteger((int)d));
    SET_VECTOR_ELT(result, 4, ScalarInteger((int)numcomps));
    SET_VECTOR_ELT(result, 5, ScalarInteger((int)prec));
    SET_VECTOR_ELT(result, 6, ScalarInteger((int)sgnd));
    UNPROTECT(2);
    return result;
}

/* =========================================================================
 * ojp3d_transcode(raw_data, has_params, params_int, target_rate, cb_sxp)
 *  -> raw vector (JP3D codestream bytes)
 * ========================================================================= */

SEXP ojp3d_transcode(SEXP raw_sxp, SEXP has_params_sxp,
                     SEXP params_sxp, SEXP rate_sxp, SEXP cb_sxp)
{
    if (!s_transcode)
        Rf_error("openjp3d library not loaded");

    if (TYPEOF(raw_sxp) != RAWSXP)
        Rf_error("openjp3d: data must be a raw vector");

    const uint8_t *src  = RAW(raw_sxp);
    size_t         size = (size_t) Rf_length(raw_sxp);
    int            use_params = asLogical(has_params_sxp);

    ojp3d_enc_params_t enc;
    ojp3d_enc_params_t *enc_ptr = NULL;

    if (use_params == TRUE) {
        if (!Rf_isInteger(params_sxp) || Rf_length(params_sxp) < 13)
            Rf_error("openjp3d: params_int must be integer(13)");
        const int *pi = INTEGER(params_sxp);
        enc.tile_width        = (uint32_t) pi[0];
        enc.tile_height       = (uint32_t) pi[1];
        enc.tile_depth        = (uint32_t) pi[2];
        enc.num_resolutions_x = (uint32_t) pi[3];
        enc.num_resolutions_y = (uint32_t) pi[4];
        enc.num_resolutions_z = (uint32_t) pi[5];
        enc.cblk_width        = (uint32_t) pi[6];
        enc.cblk_height       = (uint32_t) pi[7];
        enc.cblk_depth        = (uint32_t) pi[8];
        enc.filter            = (int32_t)  pi[9];
        enc.num_layers        = (uint32_t) pi[10];
        enc.target_rate       = (float)    asReal(rate_sxp);
        enc.use_htj2k         = 1;  /* forced for transcoding */
        enc.verbose           = (int32_t)  pi[12];
        enc_ptr = &enc;
    }

    void (*cb_fn)(int, const char *, void *) = NULL;
    if (cb_sxp != R_NilValue && !Rf_isNull(cb_sxp)) {
        s_r_callback = cb_sxp;
        cb_fn = c_msg_callback;
    }

    uint8_t *out_data = NULL;
    size_t   out_size = 0;
    int32_t  ok = s_transcode(src, size, enc_ptr,
                              &out_data, &out_size,
                              (void *)cb_fn, NULL);

    s_r_callback = R_NilValue;

    if (!ok || !out_data)
        Rf_error("openjp3d: opj_jp3d_transcode_to_ht() failed");

    SEXP result = PROTECT(Rf_allocVector(RAWSXP, (R_xlen_t)out_size));
    memcpy(RAW(result), out_data, out_size);
    s_jp3d_free(out_data);
    UNPROTECT(1);
    return result;
}

/* =========================================================================
 * Registration table
 * ========================================================================= */

static const R_CallMethodDef CallEntries[] = {
    { "ojp3d_load_lib",   (DL_FUNC) &ojp3d_load_lib,   1 },
    { "ojp3d_get_version",(DL_FUNC) &ojp3d_get_version, 0 },
    { "ojp3d_encode",     (DL_FUNC) &ojp3d_encode,     10 },
    { "ojp3d_decode",     (DL_FUNC) &ojp3d_decode,      3 },
    { "ojp3d_transcode",  (DL_FUNC) &ojp3d_transcode,   5 },
    { NULL, NULL, 0 }
};

void R_init_openjp3d(DllInfo *info)
{
    /* Initialise the callback slot to R's nil value */
    s_r_callback = R_NilValue;
    R_registerRoutines(info, NULL, CallEntries, NULL, NULL);
    R_useDynamicSymbols(info, FALSE);
}
