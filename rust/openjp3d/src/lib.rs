// Copyright (c) 2024-2026, OpenJP3D Contributors
// All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

//! Rust bindings for the OpenJP3D volumetric codec library.
//!
//! The native shared library is loaded at runtime via `dlopen`/`LoadLibrary`,
//! so this crate can be compiled and used without the openjp3d shared library
//! present at build time.  Set the `OPENJP3D_LIBRARY` environment variable to
//! the full path of the shared library, or call [`load_lib`] before using any
//! other function.
//!
//! # Quick start
//!
//! ```no_run
//! use openjp3d::{load_lib, encode, decode, EncodeParams};
//!
//! // Load the shared library (optional if OPENJP3D_LIBRARY is set).
//! load_lib(None).unwrap();
//!
//! // Create a synthetic 4×4×4 uint8 volume.
//! let vol: Vec<i32> = (0..4*4*4).map(|i| (i % 256) as i32).collect();
//!
//! // Encode losslessly.
//! let cs = encode(&vol, 4, 4, 4, 1, 8, false, None, None).unwrap();
//!
//! // Decode.
//! let (dec, info) = decode(&cs, None, None).unwrap();
//! assert_eq!(vol, dec);
//! println!("{}×{}×{}", info.w, info.h, info.d);
//! ```

use std::ffi::{c_char, c_void, CStr};
use std::sync::OnceLock;

// ---------------------------------------------------------------------------
// Public constants (mirror openjp3d.h)
// ---------------------------------------------------------------------------

/// Lossless 5/3 integer lifting wavelet filter.
pub const FILTER_53: i32 = 0;
/// Lossy 9/7 floating-point lifting wavelet filter.
pub const FILTER_97: i32 = 1;

/// Enable the High-Throughput block coder (HTJ2K).
pub const USE_HTJ2K: i32 = 1;

/// Unknown colour space.
pub const CS_UNKNOWN: u32 = 0;
/// sRGB colour space.
pub const CS_SRGB: u32 = 1;
/// Greyscale.
pub const CS_GRAY: u32 = 2;
/// YCbCr (YUV) colour space.
pub const CS_YUV: u32 = 3;

/// Informational message level.
pub const MSG_INFO: i32 = 0;
/// Warning message level.
pub const MSG_WARNING: i32 = 1;
/// Error message level.
pub const MSG_ERROR: i32 = 2;

// ---------------------------------------------------------------------------
// Error type
// ---------------------------------------------------------------------------

/// Error type returned by all fallible functions in this crate.
#[derive(Debug, Clone)]
pub struct Error(pub String);

impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(&self.0)
    }
}

impl std::error::Error for Error {}

fn err(msg: impl Into<String>) -> Error {
    Error(msg.into())
}

// ---------------------------------------------------------------------------
// Callback type
// ---------------------------------------------------------------------------

/// A message callback function.  `level` is one of [`MSG_INFO`],
/// [`MSG_WARNING`], or [`MSG_ERROR`].
pub type MsgCallback = Box<dyn Fn(i32, &str) + Send + 'static>;

// ---------------------------------------------------------------------------
// C-compatible structs (must match openjp3d.h exactly)
// ---------------------------------------------------------------------------

/// C representation of `opj_volume_comp_t`.
#[repr(C)]
struct CompC {
    w: u32,
    h: u32,
    d: u32,
    prec: u32,
    sgnd: i32,
    dz: f32,
    data: *mut i32,
}

/// C representation of `opj_volume_t`.
#[repr(C)]
struct VolumeC {
    numcomps: u32,
    comps: *mut CompC,
    x0: u32,
    y0: u32,
    z0: u32,
    x1: u32,
    y1: u32,
    z1: u32,
    color_space: u32,
}

/// C representation of `opj_jp3d_enc_params_t`.
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct EncParamsC {
    pub tile_width: u32,
    pub tile_height: u32,
    pub tile_depth: u32,
    pub num_resolutions_x: u32,
    pub num_resolutions_y: u32,
    pub num_resolutions_z: u32,
    pub cblk_width: u32,
    pub cblk_height: u32,
    pub cblk_depth: u32,
    pub filter: i32,
    pub num_layers: u32,
    pub target_rate: f32,
    pub use_htj2k: i32,
    pub verbose: i32,
}

/// C representation of `opj_jp3d_dec_params_t`.
#[repr(C)]
#[derive(Clone, Copy, Default)]
struct DecParamsC {
    verbose: i32,
}

/// Signature of the C message callback expected by the codec functions.
type CCallback = unsafe extern "C" fn(i32, *const c_char, *mut c_void);

// ---------------------------------------------------------------------------
// Function pointer table
// ---------------------------------------------------------------------------

struct LibFns {
    // The library handle must outlive all function pointers.
    _lib: libloading::Library,
    get_version: unsafe extern "C" fn() -> *const c_char,
    create_volume: unsafe extern "C" fn(u32, u32, u32, u32, u32, i32) -> *mut VolumeC,
    destroy_volume: unsafe extern "C" fn(*mut VolumeC),
    set_default_enc: unsafe extern "C" fn(*mut EncParamsC),
    set_default_dec: unsafe extern "C" fn(*mut DecParamsC),
    encode: unsafe extern "C" fn(
        *const VolumeC,
        *const EncParamsC,
        *mut *mut u8,
        *mut usize,
        Option<CCallback>,
        *mut c_void,
    ) -> i32,
    decode: unsafe extern "C" fn(
        *const u8,
        usize,
        *const DecParamsC,
        Option<CCallback>,
        *mut c_void,
    ) -> *mut VolumeC,
    transcode: unsafe extern "C" fn(
        *const u8,
        usize,
        *const EncParamsC,
        *mut *mut u8,
        *mut usize,
        Option<CCallback>,
        *mut c_void,
    ) -> i32,
    free: unsafe extern "C" fn(*mut c_void),
}

// SAFETY: All raw pointers inside LibFns point into the loaded shared
// library.  We never move or copy them across thread boundaries other than
// by calling into the library (which is itself thread-safe for the operations
// we perform).
unsafe impl Send for LibFns {}
unsafe impl Sync for LibFns {}

impl LibFns {
    unsafe fn load(lib: libloading::Library) -> Result<Self, Error> {
        macro_rules! sym {
            ($lib:expr, $sym:expr, $ty:ty) => {{
                let s: libloading::Symbol<$ty> = $lib
                    .get($sym)
                    .map_err(|e| err(format!("symbol '{}' not found: {}", stringify!($sym), e)))?;
                // Transmute to remove the lifetime tied to `$lib`.
                // SAFETY: the Library is stored in the same struct, so the
                // function pointer is always valid while `LibFns` is alive.
                #[allow(clippy::missing_transmute_annotations)]
                std::mem::transmute::<$ty, $ty>(*s)
            }};
        }

        let get_version = sym!(
            lib,
            b"opj_jp3d_get_version\0",
            unsafe extern "C" fn() -> *const c_char
        );
        let create_volume = sym!(
            lib,
            b"opj_jp3d_create_volume\0",
            unsafe extern "C" fn(u32, u32, u32, u32, u32, i32) -> *mut VolumeC
        );
        let destroy_volume = sym!(
            lib,
            b"opj_jp3d_destroy_volume\0",
            unsafe extern "C" fn(*mut VolumeC)
        );
        let set_default_enc = sym!(
            lib,
            b"opj_jp3d_set_default_encoder_parameters\0",
            unsafe extern "C" fn(*mut EncParamsC)
        );
        let set_default_dec = sym!(
            lib,
            b"opj_jp3d_set_default_decoder_parameters\0",
            unsafe extern "C" fn(*mut DecParamsC)
        );
        let encode = sym!(
            lib,
            b"opj_jp3d_encode\0",
            unsafe extern "C" fn(
                *const VolumeC,
                *const EncParamsC,
                *mut *mut u8,
                *mut usize,
                Option<CCallback>,
                *mut c_void,
            ) -> i32
        );
        let decode = sym!(
            lib,
            b"opj_jp3d_decode\0",
            unsafe extern "C" fn(
                *const u8,
                usize,
                *const DecParamsC,
                Option<CCallback>,
                *mut c_void,
            ) -> *mut VolumeC
        );
        let transcode = sym!(
            lib,
            b"opj_jp3d_transcode_to_ht\0",
            unsafe extern "C" fn(
                *const u8,
                usize,
                *const EncParamsC,
                *mut *mut u8,
                *mut usize,
                Option<CCallback>,
                *mut c_void,
            ) -> i32
        );
        let free = sym!(
            lib,
            b"opj_jp3d_free\0",
            unsafe extern "C" fn(*mut c_void)
        );

        Ok(LibFns {
            _lib: lib,
            get_version,
            create_volume,
            destroy_volume,
            set_default_enc,
            set_default_dec,
            encode,
            decode,
            transcode,
            free,
        })
    }
}

// ---------------------------------------------------------------------------
// Global library state
// ---------------------------------------------------------------------------

static LIB: OnceLock<Result<LibFns, Error>> = OnceLock::new();

fn get_lib() -> Result<&'static LibFns, Error> {
    match LIB.get() {
        Some(Ok(fns)) => Ok(fns),
        Some(Err(e)) => Err(e.clone()),
        None => Err(err("openjp3d: library not loaded; call load_lib first")),
    }
}

// ---------------------------------------------------------------------------
// Library search helpers
// ---------------------------------------------------------------------------

fn resolve_lib_path(path: Option<&str>) -> String {
    if let Some(p) = path {
        return p.to_owned();
    }
    if let Ok(v) = std::env::var("OPENJP3D_LIBRARY") {
        if !v.is_empty() {
            return v;
        }
    }

    // Try lib/ next to the executable.
    if let Ok(exe) = std::env::current_exe() {
        let dir = exe.parent().unwrap_or(std::path::Path::new(".")).join("lib");
        let names: &[&str] = if cfg!(target_os = "windows") {
            &["openjp3d.dll"]
        } else if cfg!(target_os = "macos") {
            &["libopenjp3d.dylib", "libopenjp3d.so"]
        } else {
            &["libopenjp3d.so", "libopenjp3d.so.1"]
        };
        for name in names {
            let candidate = dir.join(name);
            if candidate.exists() {
                return candidate.to_string_lossy().into_owned();
            }
        }
    }

    // Bare name — let the OS linker find it.
    if cfg!(target_os = "windows") {
        "openjp3d.dll".to_owned()
    } else if cfg!(target_os = "macos") {
        "libopenjp3d.dylib".to_owned()
    } else {
        "libopenjp3d.so".to_owned()
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Load the openjp3d shared library.
///
/// If `path` is `None`, the function searches in order:
/// 1. `OPENJP3D_LIBRARY` environment variable.
/// 2. A `lib/` subdirectory relative to the current executable.
/// 3. OS default library search path.
///
/// This function is idempotent: calling it multiple times has no effect after
/// the first successful load.
pub fn load_lib(path: Option<&str>) -> Result<(), Error> {
    let resolved = resolve_lib_path(path);
    let result = LIB.get_or_init(|| {
        let lib = unsafe {
            libloading::Library::new(&resolved)
                .map_err(|e| err(format!("cannot open library '{}': {}", resolved, e)))?
        };
        unsafe { LibFns::load(lib) }
    });
    match result {
        Ok(_) => Ok(()),
        Err(e) => Err(e.clone()),
    }
}

/// Report whether the native library has been loaded successfully.
pub fn is_loaded() -> bool {
    matches!(LIB.get(), Some(Ok(_)))
}

/// Return the native library version string (e.g. `"1.0.0"`).
pub fn get_version() -> Result<String, Error> {
    let fns = get_lib()?;
    let ver = unsafe { (fns.get_version)() };
    if ver.is_null() {
        return Ok(String::new());
    }
    Ok(unsafe { CStr::from_ptr(ver) }
        .to_string_lossy()
        .into_owned())
}

// ---------------------------------------------------------------------------
// EncodeParams
// ---------------------------------------------------------------------------

/// Encoder parameters for [`encode`] and [`transcode_to_ht`].
///
/// Zero-valued fields use library defaults when passed to the C codec.
#[derive(Debug, Clone, PartialEq)]
pub struct EncodeParams {
    /// Tile width (0 = whole image).
    pub tile_width: u32,
    /// Tile height (0 = whole image).
    pub tile_height: u32,
    /// Tile depth (0 = whole image).
    pub tile_depth: u32,
    /// DWT decomposition levels along X.
    pub num_resolutions_x: u32,
    /// DWT decomposition levels along Y.
    pub num_resolutions_y: u32,
    /// DWT decomposition levels along Z.
    pub num_resolutions_z: u32,
    /// Code-block width.
    pub cblk_width: u32,
    /// Code-block height.
    pub cblk_height: u32,
    /// Code-block depth.
    pub cblk_depth: u32,
    /// Wavelet filter: [`FILTER_53`] (lossless) or [`FILTER_97`] (lossy).
    pub filter: i32,
    /// Number of quality layers (≥ 1).
    pub num_layers: u32,
    /// Target bits per sample (0.0 = lossless).
    pub target_rate: f32,
    /// Set to [`USE_HTJ2K`] to enable the High-Throughput block coder.
    pub use_htj2k: i32,
    /// Non-zero enables informational messages.
    pub verbose: i32,
}

impl EncodeParams {
    fn to_c(&self) -> EncParamsC {
        EncParamsC {
            tile_width: self.tile_width,
            tile_height: self.tile_height,
            tile_depth: self.tile_depth,
            num_resolutions_x: self.num_resolutions_x,
            num_resolutions_y: self.num_resolutions_y,
            num_resolutions_z: self.num_resolutions_z,
            cblk_width: self.cblk_width,
            cblk_height: self.cblk_height,
            cblk_depth: self.cblk_depth,
            filter: self.filter,
            num_layers: self.num_layers,
            target_rate: self.target_rate,
            use_htj2k: self.use_htj2k,
            verbose: self.verbose,
        }
    }

    fn from_c(c: &EncParamsC) -> Self {
        EncodeParams {
            tile_width: c.tile_width,
            tile_height: c.tile_height,
            tile_depth: c.tile_depth,
            num_resolutions_x: c.num_resolutions_x,
            num_resolutions_y: c.num_resolutions_y,
            num_resolutions_z: c.num_resolutions_z,
            cblk_width: c.cblk_width,
            cblk_height: c.cblk_height,
            cblk_depth: c.cblk_depth,
            filter: c.filter,
            num_layers: c.num_layers,
            target_rate: c.target_rate,
            use_htj2k: c.use_htj2k,
            verbose: c.verbose,
        }
    }
}

/// Return the library's default encoder parameters.
///
/// [`load_lib`] must have been called (or `OPENJP3D_LIBRARY` set) before
/// calling this function.
pub fn default_encode_params() -> Result<EncodeParams, Error> {
    let fns = get_lib()?;
    let mut c = EncParamsC::default();
    unsafe { (fns.set_default_enc)(&mut c) };
    Ok(EncodeParams::from_c(&c))
}

// ---------------------------------------------------------------------------
// DecodeParams
// ---------------------------------------------------------------------------

/// Decoder parameters for [`decode`].
#[derive(Debug, Clone, Default, PartialEq)]
pub struct DecodeParams {
    /// Non-zero enables informational messages.
    pub verbose: i32,
}

// ---------------------------------------------------------------------------
// VolumeInfo
// ---------------------------------------------------------------------------

/// Metadata returned by [`decode`].
#[derive(Debug, Clone, PartialEq)]
pub struct VolumeInfo {
    /// Number of components (channels).
    pub num_comps: u32,
    /// Width (X samples per component).
    pub w: u32,
    /// Height (Y samples per component).
    pub h: u32,
    /// Depth (Z samples per component).
    pub d: u32,
    /// Bit depth per sample.
    pub prec: u32,
    /// `true` if samples are signed.
    pub signed: bool,
    /// Colour space constant (`CS_*`).
    pub color_space: u32,
}

// ---------------------------------------------------------------------------
// Callback helpers
//
// Callbacks are passed to synchronous C functions.  We store a raw pointer to
// the caller-provided closure in a thread-local slot for the duration of the
// C call, then clear it before returning.  This is sound because:
//
//   1. The C functions (encode / decode / transcode) are synchronous — they
//      do not retain the callback pointer after they return.
//   2. We clear the thread-local slot before returning from run_with_callback.
//   3. Callbacks are only ever invoked on the same thread that set the slot.
// ---------------------------------------------------------------------------

std::thread_local! {
    // We store the fat pointer as *const (dyn … + 'static) to satisfy the
    // thread-local 'static requirement.  The actual lifetime is enforced by
    // the invariant above (pointer cleared before function returns).
    static TLS_CB: std::cell::RefCell<Option<*const (dyn Fn(i32, &str) + 'static)>> =
        const { std::cell::RefCell::new(None) };
}

/// `extern "C"` bridge invoked by the native codec.
unsafe extern "C" fn tls_callback_bridge(level: i32, msg: *const c_char, _data: *mut c_void) {
    let s = if msg.is_null() {
        ""
    } else {
        match CStr::from_ptr(msg).to_str() {
            Ok(s) => s,
            Err(_) => return,
        }
    };
    TLS_CB.with(|slot| {
        if let Some(ptr) = *slot.borrow() {
            // SAFETY: the pointer is valid for the duration of the C call.
            let cb: &(dyn Fn(i32, &str) + 'static) = unsafe { &*ptr };
            cb(level, s);
        }
    });
}

/// Run `f` with `cb` installed as the thread-local callback.
fn run_with_callback<F, T>(cb: Option<&dyn Fn(i32, &str)>, f: F) -> T
where
    F: FnOnce(Option<CCallback>) -> T,
{
    if let Some(cb_ref) = cb {
        // SAFETY: we transmute away the non-'static lifetime so we can store
        // the fat pointer in the thread-local.  The pointer is valid for the
        // entire duration of `f` (a synchronous C call), and we clear the
        // slot before returning — so the pointer is never dereferenced after
        // `cb_ref` is dropped.
        let ptr: *const (dyn Fn(i32, &str) + 'static) = unsafe {
            std::mem::transmute::<
                *const dyn Fn(i32, &str),
                *const (dyn Fn(i32, &str) + 'static),
            >(cb_ref as *const _)
        };
        TLS_CB.with(|slot| {
            *slot.borrow_mut() = Some(ptr);
        });
        let result = f(Some(tls_callback_bridge));
        TLS_CB.with(|slot| {
            *slot.borrow_mut() = None;
        });
        result
    } else {
        f(None)
    }
}

// ---------------------------------------------------------------------------
// Encode
// ---------------------------------------------------------------------------

/// Encode a 3-D volume to a JP3D codestream.
///
/// `samples` must contain `num_comps * w * h * d` `i32` values in
/// component-major, depth-major, height-major, width-minor order
/// (i.e. `samples[comp][z][y][x]`).
///
/// `prec` is the bit depth (e.g. 8, 16, 32).  `signed` indicates whether
/// sample values are signed.
///
/// `params` may be `None` to use library defaults.  `cb` may be `None`.
pub fn encode(
    samples: &[i32],
    w: u32,
    h: u32,
    d: u32,
    num_comps: u32,
    prec: u32,
    signed: bool,
    params: Option<&EncodeParams>,
    cb: Option<&dyn Fn(i32, &str)>,
) -> Result<Vec<u8>, Error> {
    if num_comps == 0 || w == 0 || h == 0 || d == 0 {
        return Err(err("openjp3d::encode: dimensions must be > 0"));
    }
    let need = num_comps as usize * w as usize * h as usize * d as usize;
    if samples.len() < need {
        return Err(err(format!(
            "openjp3d::encode: samples too short: need {need}, got {}",
            samples.len()
        )));
    }

    let fns = get_lib()?;

    let cp = match params {
        Some(p) => p.to_c(),
        None => {
            let mut c = EncParamsC::default();
            unsafe { (fns.set_default_enc)(&mut c) };
            c
        }
    };

    let sgnd: i32 = if signed { 1 } else { 0 };

    // Create volume.
    let vol = unsafe { (fns.create_volume)(num_comps, w, h, d, prec, sgnd) };
    if vol.is_null() {
        return Err(err("openjp3d::encode: opj_jp3d_create_volume returned NULL"));
    }

    // Fill component data.
    let npx = (w * h * d) as usize;
    unsafe {
        let comps = std::slice::from_raw_parts((*vol).comps, num_comps as usize);
        for (c, comp) in comps.iter().enumerate() {
            let src = &samples[c * npx..(c + 1) * npx];
            std::ptr::copy_nonoverlapping(src.as_ptr(), comp.data, npx);
        }
    }

    // Encode.
    let mut out_data: *mut u8 = std::ptr::null_mut();
    let mut out_size: usize = 0;

    let ok = run_with_callback(cb, |c_cb| unsafe {
        (fns.encode)(vol, &cp, &mut out_data, &mut out_size, c_cb, std::ptr::null_mut())
    });

    unsafe { (fns.destroy_volume)(vol) };

    if ok == 0 || out_data.is_null() {
        return Err(err("openjp3d::encode: encoding failed"));
    }

    // Copy C-allocated buffer into a Rust Vec.
    let bytes = unsafe { std::slice::from_raw_parts(out_data, out_size).to_vec() };
    unsafe { (fns.free)(out_data as *mut c_void) };
    Ok(bytes)
}

// ---------------------------------------------------------------------------
// Decode
// ---------------------------------------------------------------------------

/// Decode a JP3D codestream.
///
/// Returns the sample data in component-major, depth-major, row-major order
/// together with volume metadata.
///
/// `params` may be `None` to use library defaults.  `cb` may be `None`.
pub fn decode(
    data: &[u8],
    params: Option<&DecodeParams>,
    cb: Option<&dyn Fn(i32, &str)>,
) -> Result<(Vec<i32>, VolumeInfo), Error> {
    if data.is_empty() {
        return Err(err("openjp3d::decode: empty codestream"));
    }

    let fns = get_lib()?;

    let cp = match params {
        Some(p) => DecParamsC { verbose: p.verbose },
        None => {
            let mut c = DecParamsC::default();
            unsafe { (fns.set_default_dec)(&mut c) };
            c
        }
    };

    let vol = run_with_callback(cb, |c_cb| unsafe {
        (fns.decode)(data.as_ptr(), data.len(), &cp, c_cb, std::ptr::null_mut())
    });

    if vol.is_null() {
        return Err(err("openjp3d::decode: decoding failed"));
    }

    let info = unsafe {
        let num_comps = (*vol).numcomps;
        let (w, h, d, prec, sgnd, cs) = if num_comps > 0 {
            let comp = &*(*vol).comps;
            (comp.w, comp.h, comp.d, comp.prec, comp.sgnd, (*vol).color_space)
        } else {
            (0, 0, 0, 0, 0, (*vol).color_space)
        };
        VolumeInfo {
            num_comps,
            w,
            h,
            d,
            prec,
            signed: sgnd != 0,
            color_space: cs,
        }
    };

    let npx = (info.w * info.h * info.d) as usize;
    let total = info.num_comps as usize * npx;
    let mut samples = Vec::with_capacity(total);

    unsafe {
        let comps = std::slice::from_raw_parts((*vol).comps, info.num_comps as usize);
        for comp in comps {
            let src = std::slice::from_raw_parts(comp.data, npx);
            samples.extend_from_slice(src);
        }
        (fns.destroy_volume)(vol);
    }

    Ok((samples, info))
}

// ---------------------------------------------------------------------------
// TranscodeToHT
// ---------------------------------------------------------------------------

/// Losslessly transcode a JP3D codestream to HTJ2K block coding.
///
/// `params` may be `None` to use library defaults (`use_htj2k` is forced to 1).
/// `cb` may be `None`.
pub fn transcode_to_ht(
    src: &[u8],
    params: Option<&EncodeParams>,
    cb: Option<&dyn Fn(i32, &str)>,
) -> Result<Vec<u8>, Error> {
    if src.is_empty() {
        return Err(err(
            "openjp3d::transcode_to_ht: empty source codestream",
        ));
    }

    let fns = get_lib()?;

    let mut cp = match params {
        Some(p) => p.to_c(),
        None => {
            let mut c = EncParamsC::default();
            unsafe { (fns.set_default_enc)(&mut c) };
            c
        }
    };
    cp.use_htj2k = 1;

    let mut out_data: *mut u8 = std::ptr::null_mut();
    let mut out_size: usize = 0;

    let ok = run_with_callback(cb, |c_cb| unsafe {
        (fns.transcode)(
            src.as_ptr(),
            src.len(),
            &cp,
            &mut out_data,
            &mut out_size,
            c_cb,
            std::ptr::null_mut(),
        )
    });

    if ok == 0 || out_data.is_null() {
        return Err(err("openjp3d::transcode_to_ht: transcode failed"));
    }

    let bytes = unsafe { std::slice::from_raw_parts(out_data, out_size).to_vec() };
    unsafe { (fns.free)(out_data as *mut c_void) };
    Ok(bytes)
}
