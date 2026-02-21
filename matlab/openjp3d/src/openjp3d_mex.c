/*
 * Copyright (c) 2024-2026, OpenJP3D Contributors
 * All rights reserved.
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * openjp3d_mex.c — Thin MEX wrapper for the OpenJP3D shared library.
 *
 * The openjp3d library is loaded at runtime (not at compile/link time) via
 * dlopen()/LoadLibrary().  All entry points are resolved with dlsym() /
 * GetProcAddress().  This allows the MEX file to be compiled and used
 * without having the openjp3d shared library present at build time.
 *
 * Usage from MATLAB/Octave:
 *
 *   openjp3d_mex('load_lib', path)
 *   v   = openjp3d_mex('get_version')
 *   cs  = openjp3d_mex('encode', flat_int32, w, h, d, numcomps, prec, sgnd,
 *                                 params_int13, target_rate [, callback])
 *   res = openjp3d_mex('decode', uint8_data, verbose [, callback])
 *   out = openjp3d_mex('transcode', uint8_data, has_params,
 *                                   params_int13, target_rate [, callback])
 */

#include "mex.h"

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

/* MATLAB callback function handle — set before each call, cleared after */
static mxArray *s_mx_callback = NULL;

/* =========================================================================
 * Internal: C-side message callback that calls back into MATLAB/Octave
 * ========================================================================= */

static void c_msg_callback(int level, const char *msg, void *data)
{
    (void)data;
    if (!s_mx_callback) return;
    if (!msg) msg = "";

    /* feval(handle, level, message) */
    mxArray *args[3];
    args[0] = s_mx_callback;                        /* function handle */
    args[1] = mxCreateDoubleScalar((double)level);
    args[2] = mxCreateString(msg);
    mexCallMATLAB(0, NULL, 3, args, "feval");
    mxDestroyArray(args[1]);
    mxDestroyArray(args[2]);
}

/* =========================================================================
 * Command: load_lib
 * ========================================================================= */

static void cmd_load_lib(int nlhs, mxArray *plhs[],
                         int nrhs, const mxArray *prhs[])
{
    (void)nlhs; (void)plhs;
    if (nrhs < 2 || !mxIsChar(prhs[1]))
        mexErrMsgIdAndTxt("openjp3d:badarg",
                          "load_lib requires a path string argument");

    char path[4096];
    if (mxGetString(prhs[1], path, sizeof(path)) != 0)
        mexErrMsgIdAndTxt("openjp3d:badarg", "load_lib: path string too long");

    if (s_lib) {
        lib_close(s_lib);
        s_lib = (lib_handle_t)0;
    }

    s_lib = lib_open(path);
    if (!s_lib) {
        const char *err = lib_last_error();
        mexErrMsgIdAndTxt("openjp3d:loadfail",
                          "Cannot open library '%s': %s",
                          path, err ? err : "(unknown error)");
    }

#define LOAD_SYM(fn_var, c_name)                                             \
    do {                                                                      \
        s_##fn_var = (fn_##fn_var##_t) lib_sym(s_lib, #c_name);             \
        if (!s_##fn_var)                                                      \
            mexErrMsgIdAndTxt("openjp3d:symfail",                            \
                              "Symbol '" #c_name "' not found in library");  \
    } while (0)

    LOAD_SYM(get_version,    opj_jp3d_get_version);
    LOAD_SYM(create_volume,  opj_jp3d_create_volume);
    LOAD_SYM(destroy_volume, opj_jp3d_destroy_volume);
    LOAD_SYM(encode,         opj_jp3d_encode);
    LOAD_SYM(decode,         opj_jp3d_decode);
    LOAD_SYM(transcode,      opj_jp3d_transcode_to_ht);

#undef LOAD_SYM

    s_jp3d_free = (fn_free_t) lib_sym(s_lib, "opj_jp3d_free");
    if (!s_jp3d_free)
        mexErrMsgIdAndTxt("openjp3d:symfail",
                          "Symbol 'opj_jp3d_free' not found in library");
}

/* =========================================================================
 * Command: get_version
 * ========================================================================= */

static void cmd_get_version(int nlhs, mxArray *plhs[],
                             int nrhs, const mxArray *prhs[])
{
    (void)nrhs; (void)prhs;
    if (!s_get_version)
        mexErrMsgIdAndTxt("openjp3d:notloaded",
                          "Library not loaded; call load_lib() first");
    const char *v = s_get_version();
    if (nlhs >= 1)
        plhs[0] = mxCreateString(v ? v : "");
}

/* =========================================================================
 * Command: encode
 *
 * prhs[1]  = flat int32 data (w*h*d*numcomps elements, C row-major)
 * prhs[2]  = w  (scalar)
 * prhs[3]  = h  (scalar)
 * prhs[4]  = d  (scalar)
 * prhs[5]  = numcomps (scalar)
 * prhs[6]  = prec (scalar)
 * prhs[7]  = sgnd (scalar)
 * prhs[8]  = params_int (int32, 13 elements)
 * prhs[9]  = target_rate (double scalar)
 * prhs[10] = callback (function handle, optional)
 *
 * plhs[0]  = uint8 row vector (codestream bytes)
 * ========================================================================= */

static void cmd_encode(int nlhs, mxArray *plhs[],
                       int nrhs, const mxArray *prhs[])
{
    if (!s_encode)
        mexErrMsgIdAndTxt("openjp3d:notloaded",
                          "Library not loaded; call load_lib() first");
    if (nrhs < 10)
        mexErrMsgIdAndTxt("openjp3d:badarg",
                          "encode requires at least 9 arguments after command");

    uint32_t w        = (uint32_t) mxGetScalar(prhs[2]);
    uint32_t h        = (uint32_t) mxGetScalar(prhs[3]);
    uint32_t d        = (uint32_t) mxGetScalar(prhs[4]);
    uint32_t numcomps = (uint32_t) mxGetScalar(prhs[5]);
    uint32_t prec     = (uint32_t) mxGetScalar(prhs[6]);
    int32_t  sgnd     = (int32_t)  mxGetScalar(prhs[7]);

    /* params_int: int32(13) */
    if (!mxIsInt32(prhs[8]) || mxGetNumberOfElements(prhs[8]) < 13)
        mexErrMsgIdAndTxt("openjp3d:badarg",
                          "encode: params_int must be int32 with 13 elements");
    const int32_t *pi = (const int32_t *) mxGetData(prhs[8]);

    double target_rate = mxGetScalar(prhs[9]);

    /* Optional callback */
    s_mx_callback = NULL;
    if (nrhs >= 11 && mxIsClass(prhs[10], "function_handle"))
        s_mx_callback = (mxArray *) prhs[10];

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
    enc.target_rate       = (float)    target_rate;
    enc.use_htj2k         = (int32_t)  pi[11];
    enc.verbose           = (int32_t)  pi[12];

    size_t n_voxels = (size_t)w * h * d;
    size_t n_total  = n_voxels * numcomps;

    if (!mxIsInt32(prhs[1]))
        mexErrMsgIdAndTxt("openjp3d:badarg",
                          "encode: data must be int32");
    if (mxGetNumberOfElements(prhs[1]) < n_total)
        mexErrMsgIdAndTxt("openjp3d:badarg",
                          "encode: data vector too short");

    ojp3d_volume_t *vol = s_create_volume(numcomps, w, h, d, prec, sgnd);
    if (!vol) {
        s_mx_callback = NULL;
        mexErrMsgIdAndTxt("openjp3d:error",
                          "opj_jp3d_create_volume() returned NULL");
    }

    const int32_t *src = (const int32_t *) mxGetData(prhs[1]);
    for (uint32_t c = 0; c < numcomps; c++) {
        int32_t *dst       = vol->comps[c].data;
        const int32_t *s   = src + (size_t)c * n_voxels;
        for (size_t i = 0; i < n_voxels; i++)
            dst[i] = s[i];
    }

    void (*cb_fn)(int, const char *, void *) = NULL;
    if (s_mx_callback)
        cb_fn = c_msg_callback;

    uint8_t *out_data = NULL;
    size_t   out_size = 0;
    int32_t  ok = s_encode(vol, &enc, &out_data, &out_size,
                           (void *)cb_fn, NULL);

    s_mx_callback = NULL;
    s_destroy_volume(vol);

    if (!ok || !out_data)
        mexErrMsgIdAndTxt("openjp3d:error",
                          "opj_jp3d_encode() failed");

    if (nlhs >= 1) {
        plhs[0] = mxCreateNumericMatrix(1, (mwSize)out_size,
                                        mxUINT8_CLASS, mxREAL);
        memcpy(mxGetData(plhs[0]), out_data, out_size);
    }
    s_jp3d_free(out_data);
}

/* =========================================================================
 * Command: decode
 *
 * prhs[1]  = uint8 codestream data
 * prhs[2]  = verbose (logical scalar)
 * prhs[3]  = callback (function handle, optional)
 *
 * plhs[0]  = struct with fields: data(int32), w, h, d, numcomps, prec, sgnd
 * ========================================================================= */

static void cmd_decode(int nlhs, mxArray *plhs[],
                       int nrhs, const mxArray *prhs[])
{
    if (!s_decode)
        mexErrMsgIdAndTxt("openjp3d:notloaded",
                          "Library not loaded; call load_lib() first");
    if (nrhs < 3)
        mexErrMsgIdAndTxt("openjp3d:badarg",
                          "decode requires at least 2 arguments after command");

    if (!mxIsUint8(prhs[1]))
        mexErrMsgIdAndTxt("openjp3d:badarg",
                          "decode: data must be a uint8 vector");

    const uint8_t *src  = (const uint8_t *) mxGetData(prhs[1]);
    size_t         size = mxGetNumberOfElements(prhs[1]);
    int            verb = (int) mxGetScalar(prhs[2]);

    /* Optional callback */
    s_mx_callback = NULL;
    if (nrhs >= 4 && mxIsClass(prhs[3], "function_handle"))
        s_mx_callback = (mxArray *) prhs[3];

    ojp3d_dec_params_t dec;
    dec.verbose = (int32_t)(verb ? 1 : 0);

    void (*cb_fn)(int, const char *, void *) = NULL;
    if (s_mx_callback)
        cb_fn = c_msg_callback;

    ojp3d_volume_t *vol = s_decode(src, size, &dec, (void *)cb_fn, NULL);
    s_mx_callback = NULL;

    if (!vol)
        mexErrMsgIdAndTxt("openjp3d:error",
                          "opj_jp3d_decode() returned NULL — decoding failed");

    uint32_t nc       = vol->numcomps;
    uint32_t w        = vol->comps[0].w;
    uint32_t h        = vol->comps[0].h;
    uint32_t d        = vol->comps[0].d;
    uint32_t prec     = vol->comps[0].prec;
    int32_t  sgnd     = vol->comps[0].sgnd;
    size_t   n_voxels = (size_t)w * h * d;
    size_t   n_total  = n_voxels * nc;

    /* Build output struct */
    const char *fields[] = { "data", "w", "h", "d",
                              "numcomps", "prec", "sgnd" };
    mxArray *st = mxCreateStructMatrix(1, 1, 7, fields);

    mxArray *mx_data = mxCreateNumericMatrix(1, (mwSize)n_total,
                                             mxINT32_CLASS, mxREAL);
    int32_t *dst = (int32_t *) mxGetData(mx_data);

    for (uint32_t c = 0; c < nc; c++) {
        const int32_t *s2 = vol->comps[c].data;
        int32_t *d2       = dst + (size_t)c * n_voxels;
        for (size_t i = 0; i < n_voxels; i++)
            d2[i] = s2[i];
    }
    s_destroy_volume(vol);

    mxSetField(st, 0, "data",     mx_data);
    mxSetField(st, 0, "w",        mxCreateDoubleScalar((double)w));
    mxSetField(st, 0, "h",        mxCreateDoubleScalar((double)h));
    mxSetField(st, 0, "d",        mxCreateDoubleScalar((double)d));
    mxSetField(st, 0, "numcomps", mxCreateDoubleScalar((double)nc));
    mxSetField(st, 0, "prec",     mxCreateDoubleScalar((double)prec));
    mxSetField(st, 0, "sgnd",     mxCreateDoubleScalar((double)sgnd));

    if (nlhs >= 1)
        plhs[0] = st;
    else
        mxDestroyArray(st);
}

/* =========================================================================
 * Command: transcode
 *
 * prhs[1]  = uint8 source codestream
 * prhs[2]  = has_params (logical scalar)
 * prhs[3]  = params_int (int32, 13 elements)
 * prhs[4]  = target_rate (double)
 * prhs[5]  = callback (function handle, optional)
 *
 * plhs[0]  = uint8 row vector (transcoded codestream)
 * ========================================================================= */

static void cmd_transcode(int nlhs, mxArray *plhs[],
                          int nrhs, const mxArray *prhs[])
{
    if (!s_transcode)
        mexErrMsgIdAndTxt("openjp3d:notloaded",
                          "Library not loaded; call load_lib() first");
    if (nrhs < 5)
        mexErrMsgIdAndTxt("openjp3d:badarg",
                          "transcode requires at least 4 arguments after command");

    if (!mxIsUint8(prhs[1]))
        mexErrMsgIdAndTxt("openjp3d:badarg",
                          "transcode: data must be a uint8 vector");

    const uint8_t *src  = (const uint8_t *) mxGetData(prhs[1]);
    size_t         size = mxGetNumberOfElements(prhs[1]);
    int            use_params = (int) mxGetScalar(prhs[2]);

    ojp3d_enc_params_t  enc;
    ojp3d_enc_params_t *enc_ptr = NULL;

    if (use_params) {
        if (!mxIsInt32(prhs[3]) || mxGetNumberOfElements(prhs[3]) < 13)
            mexErrMsgIdAndTxt("openjp3d:badarg",
                              "transcode: params_int must be int32(13)");
        const int32_t *pi = (const int32_t *) mxGetData(prhs[3]);
        double target_rate = mxGetScalar(prhs[4]);

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
        enc.target_rate       = (float)    target_rate;
        enc.use_htj2k         = 1;  /* forced for transcoding */
        enc.verbose           = (int32_t)  pi[12];
        enc_ptr = &enc;
    }

    /* Optional callback */
    s_mx_callback = NULL;
    if (nrhs >= 6 && mxIsClass(prhs[5], "function_handle"))
        s_mx_callback = (mxArray *) prhs[5];

    void (*cb_fn)(int, const char *, void *) = NULL;
    if (s_mx_callback)
        cb_fn = c_msg_callback;

    uint8_t *out_data = NULL;
    size_t   out_size = 0;
    int32_t  ok = s_transcode(src, size, enc_ptr,
                              &out_data, &out_size,
                              (void *)cb_fn, NULL);
    s_mx_callback = NULL;

    if (!ok || !out_data)
        mexErrMsgIdAndTxt("openjp3d:error",
                          "opj_jp3d_transcode_to_ht() failed");

    if (nlhs >= 1) {
        plhs[0] = mxCreateNumericMatrix(1, (mwSize)out_size,
                                        mxUINT8_CLASS, mxREAL);
        memcpy(mxGetData(plhs[0]), out_data, out_size);
    }
    s_jp3d_free(out_data);
}

/* =========================================================================
 * mexFunction — single entry point; dispatches on first string argument
 * ========================================================================= */

void mexFunction(int nlhs, mxArray *plhs[],
                 int nrhs, const mxArray *prhs[])
{
    if (nrhs < 1 || !mxIsChar(prhs[0]))
        mexErrMsgIdAndTxt("openjp3d:badarg",
                          "First argument must be a command string "
                          "('load_lib', 'get_version', 'encode', "
                          "'decode', 'transcode')");

    char cmd[64];
    if (mxGetString(prhs[0], cmd, sizeof(cmd)) != 0)
        mexErrMsgIdAndTxt("openjp3d:badarg", "Command string too long");

    if (strcmp(cmd, "load_lib") == 0)
        cmd_load_lib(nlhs, plhs, nrhs, prhs);
    else if (strcmp(cmd, "get_version") == 0)
        cmd_get_version(nlhs, plhs, nrhs, prhs);
    else if (strcmp(cmd, "encode") == 0)
        cmd_encode(nlhs, plhs, nrhs, prhs);
    else if (strcmp(cmd, "decode") == 0)
        cmd_decode(nlhs, plhs, nrhs, prhs);
    else if (strcmp(cmd, "transcode") == 0)
        cmd_transcode(nlhs, plhs, nrhs, prhs);
    else
        mexErrMsgIdAndTxt("openjp3d:badcmd",
                          "Unknown command '%s'", cmd);
}
