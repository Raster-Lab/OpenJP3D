// Copyright (c) 2024-2026, OpenJP3D Contributors
// All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

/*
Package openjp3d provides Go bindings for the OpenJP3D volumetric codec library.

The library is loaded at runtime via dlopen()/LoadLibrary() so the Go package
can be compiled and used without the openjp3d shared library present at build
time.  Set the OPENJP3D_LIBRARY environment variable to the full path of the
shared library, or call LoadLib before using any other functions.

Quick start:

	import "github.com/raster-lab/openjp3d"

	// Load the shared library (optional if OPENJP3D_LIBRARY is set)
	if err := openjp3d.LoadLib(""); err != nil {
	    log.Fatal(err)
	}

	// Create a synthetic 4×4×4 volume of uint8 values
	vol := make([]int32, 4*4*4)
	for i := range vol { vol[i] = int32(i) }

	// Encode
	cs, err := openjp3d.Encode(vol, 4, 4, 4, 1, 8, false, nil)
	if err != nil {
	    log.Fatal(err)
	}

	// Decode
	dec, info, err := openjp3d.Decode(cs, nil)
	if err != nil {
	    log.Fatal(err)
	}
	fmt.Println(info.W, info.H, info.D)
*/
package openjp3d

/*
#cgo linux   LDFLAGS: -ldl
#cgo darwin  LDFLAGS: -ldl

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// -------------------------------------------------------------------------
// Platform dynamic-loading shims
// -------------------------------------------------------------------------

#ifdef _WIN32
#  include <windows.h>
typedef HMODULE ojp3d_lib_handle_t;
static ojp3d_lib_handle_t ojp3d_lib_open(const char *p) {
    return LoadLibraryA(p);
}
static void *ojp3d_lib_sym(ojp3d_lib_handle_t h, const char *n) {
    return (void *)GetProcAddress(h, n);
}
static void ojp3d_lib_close(ojp3d_lib_handle_t h) { FreeLibrary(h); }
static const char *ojp3d_lib_last_error(void) {
    static char buf[256];
    DWORD e = GetLastError();
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, e, 0, buf, (DWORD)sizeof(buf), NULL);
    return buf;
}
#else
#  include <dlfcn.h>
typedef void *ojp3d_lib_handle_t;
static ojp3d_lib_handle_t ojp3d_lib_open(const char *p) {
    return dlopen(p, RTLD_LAZY | RTLD_LOCAL);
}
static void *ojp3d_lib_sym(ojp3d_lib_handle_t h, const char *n) {
    return dlsym(h, n);
}
static void ojp3d_lib_close(ojp3d_lib_handle_t h) { dlclose(h); }
static const char *ojp3d_lib_last_error(void) { return dlerror(); }
#endif

// -------------------------------------------------------------------------
// C struct mirrors — layout must match openjp3d.h exactly
// -------------------------------------------------------------------------

typedef struct {
    uint32_t  w, h, d, prec;
    int32_t   sgnd;
    float     dz;
    int32_t  *data;
} ojp3d_comp_t;

typedef struct {
    uint32_t    numcomps;
    ojp3d_comp_t *comps;
    uint32_t    x0, y0, z0;
    uint32_t    x1, y1, z1;
    uint32_t    color_space;
} ojp3d_vol_t;

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

// -------------------------------------------------------------------------
// Function pointer types
// -------------------------------------------------------------------------

typedef const char    *(*fn_get_version_t)   (void);
typedef ojp3d_vol_t  *(*fn_create_volume_t)  (uint32_t, uint32_t,
                                               uint32_t, uint32_t,
                                               uint32_t, int32_t);
typedef void          (*fn_destroy_volume_t) (ojp3d_vol_t *);
typedef void          (*fn_set_enc_params_t) (ojp3d_enc_params_t *);
typedef void          (*fn_set_dec_params_t) (ojp3d_dec_params_t *);
typedef int32_t       (*fn_encode_t)         (const ojp3d_vol_t *,
                                               const ojp3d_enc_params_t *,
                                               uint8_t **, size_t *,
                                               void *, void *);
typedef ojp3d_vol_t  *(*fn_decode_t)         (const uint8_t *, size_t,
                                               const ojp3d_dec_params_t *,
                                               void *, void *);
typedef int32_t       (*fn_transcode_t)      (const uint8_t *, size_t,
                                               const ojp3d_enc_params_t *,
                                               uint8_t **, size_t *,
                                               void *, void *);
typedef void          (*fn_free_t)           (void *);

// -------------------------------------------------------------------------
// Module-level state
// -------------------------------------------------------------------------

static ojp3d_lib_handle_t  s_ojp3d_lib;
static fn_get_version_t    s_ojp3d_get_version;
static fn_create_volume_t  s_ojp3d_create_volume;
static fn_destroy_volume_t s_ojp3d_destroy_volume;
static fn_set_enc_params_t s_ojp3d_set_default_enc;
static fn_set_dec_params_t s_ojp3d_set_default_dec;
static fn_encode_t         s_ojp3d_encode;
static fn_decode_t         s_ojp3d_decode;
static fn_transcode_t      s_ojp3d_transcode;
static fn_free_t           s_ojp3d_free;

// -------------------------------------------------------------------------
// ojp3d_load_lib: open the shared library and resolve all symbols.
// Returns 1 on success, 0 on failure (check ojp3d_load_error for details).
// -------------------------------------------------------------------------
static char s_ojp3d_load_error[512];

static int ojp3d_load_lib(const char *path)
{
    if (s_ojp3d_lib) return 1;       // already loaded
#ifndef _WIN32
    dlerror();                        // clear previous error
#endif
    s_ojp3d_lib = ojp3d_lib_open(path);
    if (!s_ojp3d_lib) {
        const char *e = ojp3d_lib_last_error();
        snprintf(s_ojp3d_load_error, sizeof(s_ojp3d_load_error),
                 "cannot open library '%s': %s", path ? path : "<default>",
                 e ? e : "unknown error");
        return 0;
    }

#define OJLOAD(name, type, sym)                                        \
    name = (type)ojp3d_lib_sym(s_ojp3d_lib, sym);                     \
    if (!name) {                                                        \
        snprintf(s_ojp3d_load_error, sizeof(s_ojp3d_load_error),      \
                 "symbol '%s' not found in library", sym);             \
        ojp3d_lib_close(s_ojp3d_lib);                                  \
        s_ojp3d_lib = (ojp3d_lib_handle_t)0;                          \
        return 0;                                                       \
    }

    OJLOAD(s_ojp3d_get_version,    fn_get_version_t,    "opj_jp3d_get_version")
    OJLOAD(s_ojp3d_create_volume,  fn_create_volume_t,  "opj_jp3d_create_volume")
    OJLOAD(s_ojp3d_destroy_volume, fn_destroy_volume_t, "opj_jp3d_destroy_volume")
    OJLOAD(s_ojp3d_set_default_enc,fn_set_enc_params_t, "opj_jp3d_set_default_encoder_parameters")
    OJLOAD(s_ojp3d_set_default_dec,fn_set_dec_params_t, "opj_jp3d_set_default_decoder_parameters")
    OJLOAD(s_ojp3d_encode,         fn_encode_t,         "opj_jp3d_encode")
    OJLOAD(s_ojp3d_decode,         fn_decode_t,         "opj_jp3d_decode")
    OJLOAD(s_ojp3d_transcode,      fn_transcode_t,      "opj_jp3d_transcode_to_ht")
    OJLOAD(s_ojp3d_free,           fn_free_t,           "opj_jp3d_free")
#undef OJLOAD

    return 1;
}

static int ojp3d_is_loaded(void) {
    return s_ojp3d_lib != (ojp3d_lib_handle_t)0;
}

static const char *ojp3d_last_load_error(void) {
    return s_ojp3d_load_error;
}

// -------------------------------------------------------------------------
// Default-parameter helpers (call into library if loaded)
// -------------------------------------------------------------------------

static void ojp3d_defaults_enc(ojp3d_enc_params_t *p)
{
    if (s_ojp3d_set_default_enc) {
        s_ojp3d_set_default_enc((void *)p);
    } else {
        memset(p, 0, sizeof(*p));
        p->num_resolutions_x = 3;
        p->num_resolutions_y = 3;
        p->num_resolutions_z = 3;
        p->cblk_width  = 64;
        p->cblk_height = 64;
        p->cblk_depth  = 64;
        p->filter      = 0;     // FILTER_53
        p->num_layers  = 1;
    }
}

static void ojp3d_defaults_dec(ojp3d_dec_params_t *p)
{
    if (s_ojp3d_set_default_dec) {
        s_ojp3d_set_default_dec((void *)p);
    } else {
        memset(p, 0, sizeof(*p));
    }
}

// -------------------------------------------------------------------------
// Callback bridge — defined here; the Go-side export is in callback.go.
// The extern declaration lets the C compiler know the symbol exists.
// -------------------------------------------------------------------------
extern void ojp3d_go_callback_bridge(int level, const char *msg);

static void ojp3d_c_callback(int level, const char *msg, void *data) {
    (void)data;
    if (!msg) msg = "";
    ojp3d_go_callback_bridge(level, msg);
}

// -------------------------------------------------------------------------
// encode_raw: wraps opj_jp3d_encode; returns 1 on success.
// -------------------------------------------------------------------------
static int ojp3d_encode_raw(
    uint32_t numcomps,
    uint32_t w, uint32_t h, uint32_t d,
    uint32_t prec, int32_t sgnd,
    const int32_t *samples,           // numcomps * w*h*d samples (row-major)
    const ojp3d_enc_params_t *params,
    uint8_t **out_data, size_t *out_size,
    int use_callback)
{
    if (!s_ojp3d_create_volume || !s_ojp3d_encode) return 0;

    ojp3d_vol_t *vol = s_ojp3d_create_volume(numcomps, w, h, d, prec, sgnd);
    if (!vol) return 0;

    size_t npx = (size_t)w * h * d;
    uint32_t c;
    for (c = 0; c < numcomps; c++) {
        memcpy(vol->comps[c].data, samples + (size_t)c * npx,
               npx * sizeof(int32_t));
    }

    void *cb  = use_callback ? (void *)ojp3d_c_callback : NULL;
    int32_t ok = s_ojp3d_encode(vol, params, out_data, out_size, cb, NULL);

    s_ojp3d_destroy_volume(vol);
    return ok;
}

// -------------------------------------------------------------------------
// decode_raw: wraps opj_jp3d_decode; fills provided fields, returns samples
// (caller must free with ojp3d_free_buf).
// -------------------------------------------------------------------------
typedef struct {
    uint32_t numcomps;
    uint32_t w, h, d, prec;
    int32_t  sgnd;
    uint32_t color_space;
} ojp3d_vol_info_t;

static int32_t *ojp3d_decode_raw(
    const uint8_t *data, size_t size,
    const ojp3d_dec_params_t *params,
    ojp3d_vol_info_t *info,
    int use_callback)
{
    if (!s_ojp3d_decode) return NULL;

    void *cb = use_callback ? (void *)ojp3d_c_callback : NULL;
    ojp3d_vol_t *vol = s_ojp3d_decode(data, size, params, cb, NULL);
    if (!vol) return NULL;

    info->numcomps   = vol->numcomps;
    info->w          = (vol->numcomps > 0) ? vol->comps[0].w : 0;
    info->h          = (vol->numcomps > 0) ? vol->comps[0].h : 0;
    info->d          = (vol->numcomps > 0) ? vol->comps[0].d : 0;
    info->prec        = (vol->numcomps > 0) ? vol->comps[0].prec : 0;
    info->sgnd        = (vol->numcomps > 0) ? vol->comps[0].sgnd : 0;
    info->color_space = vol->color_space;

    size_t npx = (size_t)info->w * info->h * info->d;
    int32_t *out = (int32_t *)malloc(vol->numcomps * npx * sizeof(int32_t));
    if (!out) {
        s_ojp3d_destroy_volume(vol);
        return NULL;
    }
    uint32_t c;
    for (c = 0; c < vol->numcomps; c++) {
        memcpy(out + (size_t)c * npx, vol->comps[c].data,
               npx * sizeof(int32_t));
    }

    s_ojp3d_destroy_volume(vol);
    return out;
}

// -------------------------------------------------------------------------
// transcode_raw: wraps opj_jp3d_transcode_to_ht.
// -------------------------------------------------------------------------
static int ojp3d_transcode_raw(
    const uint8_t *src, size_t src_size,
    const ojp3d_enc_params_t *params,
    uint8_t **out_data, size_t *out_size,
    int use_callback)
{
    if (!s_ojp3d_transcode) return 0;
    void *cb = use_callback ? (void *)ojp3d_c_callback : NULL;
    return s_ojp3d_transcode(src, src_size, params, out_data, out_size,
                              cb, NULL);
}

static void ojp3d_free_buf(void *p) {
    if (s_ojp3d_free) s_ojp3d_free(p);
    else free(p);
}

static const char *ojp3d_get_version(void) {
    if (!s_ojp3d_get_version) return "";
    return s_ojp3d_get_version();
}
*/
import "C"

import (
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"runtime"
	"sync"
	"unsafe"
)

// ---------------------------------------------------------------------------
// Constants (mirror openjp3d.h)
// ---------------------------------------------------------------------------

const (
	// Filter53 selects the lossless 5/3 integer lifting filter.
	Filter53 = 0
	// Filter97 selects the lossy 9/7 floating-point lifting filter.
	Filter97 = 1
	// UseHTJ2K enables the High-Throughput block coder.
	UseHTJ2K = 1

	// CSUnknown — unknown colour space.
	CSUnknown = 0
	// CSSrgb — sRGB colour space.
	CSSrgb = 1
	// CSGray — greyscale.
	CSGray = 2
	// CSYUV — YUV colour space.
	CSYUV = 3

	// MsgInfo is the informational message level.
	MsgInfo = 0
	// MsgWarning is the warning message level.
	MsgWarning = 1
	// MsgError is the error message level.
	MsgError = 2
)

// ---------------------------------------------------------------------------
// EncodeParams — encoder parameter struct
// ---------------------------------------------------------------------------

// EncodeParams holds the encoder configuration for Encode and TranscodeToHT.
// Zero values mean "use library defaults".
type EncodeParams struct {
	// Tile dimensions (0 = whole image per axis).
	TileWidth  uint32
	TileHeight uint32
	TileDepth  uint32

	// DWT decomposition levels per axis.
	NumResolutionsX uint32
	NumResolutionsY uint32
	NumResolutionsZ uint32

	// Code-block dimensions.
	CblkWidth  uint32
	CblkHeight uint32
	CblkDepth  uint32

	// Filter selects the wavelet filter:  Filter53 (default) or Filter97.
	Filter int32

	// NumLayers is the number of quality layers (≥ 1).
	NumLayers uint32

	// TargetRate is the target bits per sample (0.0 = lossless).
	TargetRate float32

	// UseHTJ2K enables the HTJ2K block coder (set to UseHTJ2K = 1 to enable).
	UseHTJ2K int32

	// Verbose enables informational messages when non-zero.
	Verbose int32
}

// DefaultEncodeParams returns an EncodeParams pre-filled with the library's
// default encoder settings.  LoadLib must be called before this function.
func DefaultEncodeParams() EncodeParams {
	var cp C.ojp3d_enc_params_t
	C.ojp3d_defaults_enc(&cp)
	return encParamsFromC(cp)
}

func encParamsToC(p EncodeParams) C.ojp3d_enc_params_t {
	return C.ojp3d_enc_params_t{
		tile_width:        C.uint32_t(p.TileWidth),
		tile_height:       C.uint32_t(p.TileHeight),
		tile_depth:        C.uint32_t(p.TileDepth),
		num_resolutions_x: C.uint32_t(p.NumResolutionsX),
		num_resolutions_y: C.uint32_t(p.NumResolutionsY),
		num_resolutions_z: C.uint32_t(p.NumResolutionsZ),
		cblk_width:        C.uint32_t(p.CblkWidth),
		cblk_height:       C.uint32_t(p.CblkHeight),
		cblk_depth:        C.uint32_t(p.CblkDepth),
		filter:            C.int32_t(p.Filter),
		num_layers:        C.uint32_t(p.NumLayers),
		target_rate:       C.float(p.TargetRate),
		use_htj2k:         C.int32_t(p.UseHTJ2K),
		verbose:           C.int32_t(p.Verbose),
	}
}

func encParamsFromC(cp C.ojp3d_enc_params_t) EncodeParams {
	return EncodeParams{
		TileWidth:       uint32(cp.tile_width),
		TileHeight:      uint32(cp.tile_height),
		TileDepth:       uint32(cp.tile_depth),
		NumResolutionsX: uint32(cp.num_resolutions_x),
		NumResolutionsY: uint32(cp.num_resolutions_y),
		NumResolutionsZ: uint32(cp.num_resolutions_z),
		CblkWidth:       uint32(cp.cblk_width),
		CblkHeight:      uint32(cp.cblk_height),
		CblkDepth:       uint32(cp.cblk_depth),
		Filter:          int32(cp.filter),
		NumLayers:       uint32(cp.num_layers),
		TargetRate:      float32(cp.target_rate),
		UseHTJ2K:        int32(cp.use_htj2k),
		Verbose:         int32(cp.verbose),
	}
}

// ---------------------------------------------------------------------------
// VolumeInfo — metadata returned by Decode
// ---------------------------------------------------------------------------

// VolumeInfo holds the metadata of a decoded volume.
type VolumeInfo struct {
	NumComps   uint32
	W, H, D    uint32
	Prec       uint32
	Signed     bool
	ColorSpace uint32
}

// ---------------------------------------------------------------------------
// Library loading
// ---------------------------------------------------------------------------

var (
	loadOnce  sync.Once
	loadErr   error
)

// LoadLib loads the openjp3d shared library.  If path is empty, the function
// searches in order:
//  1. OPENJP3D_LIBRARY environment variable.
//  2. A lib/ subdirectory relative to the current executable.
//  3. OS default library search path ("libopenjp3d" / "openjp3d").
//
// LoadLib is idempotent: calling it multiple times has no effect after the
// first successful load.
func LoadLib(path string) error {
	if path == "" {
		path = resolveLibPath()
	}
	loadOnce.Do(func() {
		cs := C.CString(path)
		defer C.free(unsafe.Pointer(cs))
		if C.ojp3d_load_lib(cs) == 0 {
			loadErr = fmt.Errorf("openjp3d: %s",
				C.GoString(C.ojp3d_last_load_error()))
		}
	})
	return loadErr
}

// IsLoaded reports whether the native library has been loaded successfully.
func IsLoaded() bool {
	return C.ojp3d_is_loaded() != 0
}

// resolveLibPath determines the best candidate path for the shared library.
func resolveLibPath() string {
	if v := os.Getenv("OPENJP3D_LIBRARY"); v != "" {
		return v
	}

	// Try lib/ next to executable.
	exe, err := os.Executable()
	if err == nil {
		var names []string
		switch runtime.GOOS {
		case "windows":
			names = []string{"openjp3d.dll"}
		case "darwin":
			names = []string{"libopenjp3d.dylib", "libopenjp3d.so"}
		default:
			names = []string{"libopenjp3d.so", "libopenjp3d.so.1"}
		}
		dir := filepath.Join(filepath.Dir(exe), "lib")
		for _, name := range names {
			if _, err := os.Stat(filepath.Join(dir, name)); err == nil {
				return filepath.Join(dir, name)
			}
		}
	}

	// Bare name — let the OS linker find it.
	switch runtime.GOOS {
	case "windows":
		return "openjp3d.dll"
	case "darwin":
		return "libopenjp3d.dylib"
	default:
		return "libopenjp3d.so"
	}
}

func requireLib() error {
	if !IsLoaded() {
		if err := LoadLib(""); err != nil {
			return err
		}
		if !IsLoaded() {
			return errors.New("openjp3d: library not loaded; call LoadLib first")
		}
	}
	return nil
}

// ---------------------------------------------------------------------------
// Callback support
// ---------------------------------------------------------------------------

// MsgCallback is a function that receives codec messages.
// level is one of MsgInfo, MsgWarning, or MsgError.
type MsgCallback func(level int, message string)

var (
	cbMu      sync.Mutex
	activeCallback MsgCallback
)

// ---------------------------------------------------------------------------
// GetVersion returns the version string of the native library.
// ---------------------------------------------------------------------------

// GetVersion returns the native library version string (e.g. "1.0.0").
func GetVersion() (string, error) {
	if err := requireLib(); err != nil {
		return "", err
	}
	return C.GoString(C.ojp3d_get_version()), nil
}

// ---------------------------------------------------------------------------
// Encode encodes a 3-D volume to a JP3D codestream.
//
// samples must contain numComps * w * h * d int32 values stored in
// component-major, depth-major, height-major, width-minor order
// (i.e. samples[comp][z][y][x]).
//
// prec is the bit depth (e.g. 8, 16, 32); signed indicates whether the
// sample values are signed.
//
// params may be nil to use library defaults.
//
// cb may be nil to suppress messages.
// ---------------------------------------------------------------------------
func Encode(
	samples []int32,
	w, h, d, numComps, prec uint32,
	signed bool,
	params *EncodeParams,
	cb MsgCallback,
) ([]byte, error) {
	if err := requireLib(); err != nil {
		return nil, err
	}
	need := int(numComps) * int(w) * int(h) * int(d)
	if len(samples) < need {
		return nil, fmt.Errorf("openjp3d.Encode: samples too short: need %d, got %d",
			need, len(samples))
	}
	if numComps == 0 || w == 0 || h == 0 || d == 0 {
		return nil, errors.New("openjp3d.Encode: dimensions must be > 0")
	}

	// Prepare C encoder params.
	var cp C.ojp3d_enc_params_t
	if params != nil {
		cp = encParamsToC(*params)
	} else {
		C.ojp3d_defaults_enc(&cp)
	}

	sgnd := C.int32_t(0)
	if signed {
		sgnd = 1
	}

	// Install callback.
	useCB := C.int(0)
	if cb != nil {
		cbMu.Lock()
		activeCallback = cb
		cbMu.Unlock()
		useCB = 1
	}
	defer func() {
		if cb != nil {
			cbMu.Lock()
			activeCallback = nil
			cbMu.Unlock()
		}
	}()

	var outData *C.uint8_t
	var outSize C.size_t

	rc := C.ojp3d_encode_raw(
		C.uint32_t(numComps),
		C.uint32_t(w), C.uint32_t(h), C.uint32_t(d),
		C.uint32_t(prec), sgnd,
		(*C.int32_t)(unsafe.Pointer(&samples[0])),
		&cp,
		&outData, &outSize,
		useCB,
	)
	if rc == 0 {
		return nil, errors.New("openjp3d.Encode: encoding failed")
	}

	// Copy C-allocated buffer into Go-managed memory.
	goBytes := C.GoBytes(unsafe.Pointer(outData), C.int(outSize))
	C.ojp3d_free_buf(unsafe.Pointer(outData))
	return goBytes, nil
}

// ---------------------------------------------------------------------------
// Decode decodes a JP3D codestream.
//
// Returns the sample data (component-major, depth-major, row-major order),
// volume metadata, and any error.
//
// params may be nil to use library defaults.  cb may be nil.
// ---------------------------------------------------------------------------
func Decode(
	data []byte,
	params *DecodeParams,
	cb MsgCallback,
) (samples []int32, info VolumeInfo, err error) {
	if err = requireLib(); err != nil {
		return
	}
	if len(data) == 0 {
		err = errors.New("openjp3d.Decode: empty codestream")
		return
	}

	var cp C.ojp3d_dec_params_t
	if params != nil {
		cp.verbose = C.int32_t(params.Verbose)
	} else {
		C.ojp3d_defaults_dec(&cp)
	}

	useCB := C.int(0)
	if cb != nil {
		cbMu.Lock()
		activeCallback = cb
		cbMu.Unlock()
		useCB = 1
	}
	defer func() {
		if cb != nil {
			cbMu.Lock()
			activeCallback = nil
			cbMu.Unlock()
		}
	}()

	var volInfo C.ojp3d_vol_info_t
	raw := C.ojp3d_decode_raw(
		(*C.uint8_t)(unsafe.Pointer(&data[0])),
		C.size_t(len(data)),
		&cp,
		&volInfo,
		useCB,
	)
	if raw == nil {
		err = errors.New("openjp3d.Decode: decoding failed")
		return
	}
	defer C.free(unsafe.Pointer(raw))

	info = VolumeInfo{
		NumComps:   uint32(volInfo.numcomps),
		W:          uint32(volInfo.w),
		H:          uint32(volInfo.h),
		D:          uint32(volInfo.d),
		Prec:       uint32(volInfo.prec),
		Signed:     volInfo.sgnd != 0,
		ColorSpace: uint32(volInfo.color_space),
	}

	n := int(info.NumComps) * int(info.W) * int(info.H) * int(info.D)
	samples = make([]int32, n)
	if n > 0 {
		src := unsafe.Slice((*int32)(unsafe.Pointer(raw)), n)
		copy(samples, src)
	}
	return
}

// ---------------------------------------------------------------------------
// TranscodeToHT transcodes a JP3D codestream to HTJ2K block coding.
//
// params may be nil to use library defaults (use_htj2k is forced to 1).
// cb may be nil.
// ---------------------------------------------------------------------------
func TranscodeToHT(
	src []byte,
	params *EncodeParams,
	cb MsgCallback,
) ([]byte, error) {
	if err := requireLib(); err != nil {
		return nil, err
	}
	if len(src) == 0 {
		return nil, errors.New("openjp3d.TranscodeToHT: empty source codestream")
	}

	var cp C.ojp3d_enc_params_t
	if params != nil {
		cp = encParamsToC(*params)
	} else {
		C.ojp3d_defaults_enc(&cp)
	}
	cp.use_htj2k = 1

	useCB := C.int(0)
	if cb != nil {
		cbMu.Lock()
		activeCallback = cb
		cbMu.Unlock()
		useCB = 1
	}
	defer func() {
		if cb != nil {
			cbMu.Lock()
			activeCallback = nil
			cbMu.Unlock()
		}
	}()

	var outData *C.uint8_t
	var outSize C.size_t

	rc := C.ojp3d_transcode_raw(
		(*C.uint8_t)(unsafe.Pointer(&src[0])),
		C.size_t(len(src)),
		&cp,
		&outData, &outSize,
		useCB,
	)
	if rc == 0 {
		return nil, errors.New("openjp3d.TranscodeToHT: transcode failed")
	}

	goBytes := C.GoBytes(unsafe.Pointer(outData), C.int(outSize))
	C.ojp3d_free_buf(unsafe.Pointer(outData))
	return goBytes, nil
}

// ---------------------------------------------------------------------------
// DecodeParams — decoder parameter struct
// ---------------------------------------------------------------------------

// DecodeParams holds optional decoder settings.
type DecodeParams struct {
	// Verbose enables informational messages when non-zero.
	Verbose int32
}
