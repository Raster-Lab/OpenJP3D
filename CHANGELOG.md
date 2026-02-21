# Changelog

All notable changes to the OpenJP3D project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- **Test automation plan** (`doc/test-automation-plan.md`): Phased plan (A–E)
  to automate all 107 manual test procedures from `doc/manual-testing.md`.
  Includes gap analysis, test data catalog (10 deterministic datasets), CI
  workflow extensions, headless GUI smoke testing approach (Xvfb), and a full
  traceability matrix mapping every manual test ID to its automated equivalent.

- **Comprehensive manual testing guide** (`doc/manual-testing.md`): 107 manual
  test procedures covering every user-facing feature — Core C API (13 tests),
  CLI tools (18), GUI application (25), JPIP streaming (5), Python bindings
  (10), Julia bindings (6), R bindings (5), MATLAB/Octave bindings (4), Go
  bindings (5), Rust bindings (5), SIMD optimisation (4), and build system &
  packaging (7).

- **Phase 14: Rust Bindings**
  - **14.1 Rust crate scaffold** (`rust/openjp3d/`): `Cargo.toml` with
    `name = "openjp3d"`, edition 2021, `libloading = "0.8"` dependency.
    BSD-2-Clause licence header on all source files.
  - **14.2 Runtime loader** (`rust/openjp3d/src/lib.rs`):
    `libloading::Library` stored in a global `OnceLock`; `#[repr(C)]`
    struct mirrors for `opj_volume_comp_t`, `opj_volume_t`,
    `opj_jp3d_enc_params_t`, and `opj_jp3d_dec_params_t`.  All function
    pointers resolved at runtime via `lib.get()`; no link-time dependency
    on the shared library.  Library search order: `OPENJP3D_LIBRARY` env
    var → `lib/` next to the executable → OS default loader.
  - **14.3 Callback bridge**: thread-local `TLS_CB` slot stores a fat
    pointer to the caller's `&dyn Fn(i32, &str)` closure for the duration
    of each synchronous C call.  `unsafe extern "C" fn tls_callback_bridge`
    reads the slot and dispatches to the Rust closure.  `run_with_callback`
    helper manages slot lifetime safely.
  - **14.4 High-level Rust API**: `load_lib`, `is_loaded`, `get_version`,
    `encode`, `decode`, `transcode_to_ht`, `default_encode_params`.
    `EncodeParams` and `DecodeParams` structs; `VolumeInfo` metadata.
    All fallible functions return `Result<_, Error>`.
  - **14.5 CMake integration** (`rust/CMakeLists.txt`):
    `BUILD_RUST_BINDINGS` option; `test_rust_bindings` CTest target
    running `cargo test` with `OPENJP3D_LIBRARY` injected.
    Requires `cargo` and `BUILD_SHARED_LIBS=ON`.
  - **14.6 Test suite** (`rust/openjp3d/tests/integration_tests.rs`):
    40 tests covering library loading, version format, constants,
    `EncodeParams` defaults, lossless round-trips for all five supported
    precisions (uint8/int8/uint16/int16/int32), multi-component
    (3- and 4-channel) volumes, single-slice edge case, non-square
    dimensions, large (16×16×16) volumes, tiled encoding, HTJ2K lossless,
    lossy 9/7, `transcode_to_ht`, SOC marker check, data-layout
    correctness, message callback, `VolumeInfo` metadata, and
    error-handling cases.  Tests skip gracefully when the shared library
    is not available.
  - **14.7 Documentation** (`rust/openjp3d/README.md`): installation,
    library search order, quick-start examples (lossless, lossy, HTJ2K,
    transcode, multi-component, callback), and full API reference table.

- **Phase 13: Go Bindings**
  - **13.1 Go package scaffold** (`go/openjp3d/`): module
    `github.com/raster-lab/openjp3d` declared in `go.mod`.
    BSD-2-Clause licence header on all source files.
  - **13.2 CGo runtime loader** (`go/openjp3d/openjp3d.go`):
    CGo preamble embeds cross-platform `dlopen`/`LoadLibrary` shims
    and C struct mirrors matching `openjp3d.h`.  All entry points
    resolved at runtime; no link-time dependency on the shared library.
    Library search order: `OPENJP3D_LIBRARY` env var → `lib/` next to
    the executable → OS default loader.
  - **13.3 Callback bridge** (`go/openjp3d/callback.go`): exported Go
    function `ojp3d_go_callback_bridge` receives C-level messages and
    routes them to the current `MsgCallback` via a mutex-protected
    global slot.
  - **13.4 High-level Go API**: `LoadLib`, `IsLoaded`, `GetVersion`,
    `Encode`, `Decode`, `TranscodeToHT`, `DefaultEncodeParams`.
    `EncodeParams` and `DecodeParams` structs; `VolumeInfo` metadata.
  - **13.5 CMake integration** (`go/CMakeLists.txt`):
    `BUILD_GO_BINDINGS` option; `test_go_bindings` CTest target
    running `go test -v ./...` with `OPENJP3D_LIBRARY` injected.
    Requires Go ≥ 1.21 and `BUILD_SHARED_LIBS=ON`.
  - **13.6 Test suite** (`go/openjp3d/openjp3d_test.go`): 32 tests
    covering library loading, version format, constants, `EncodeParams`
    defaults, lossless round-trips for all five supported precisions
    (uint8/int8/uint16/int16/int32), multi-component (3- and 4-channel)
    volumes, single-slice edge case, non-square dimensions, large
    (16×16×16) volumes, tiled encoding, HTJ2K lossless, lossy 9/7,
    `TranscodeToHT`, SOC marker check, data-layout correctness, message
    callback, `VolumeInfo` metadata, and error-handling cases.
  - **13.7 Documentation** (`go/openjp3d/README.md`): installation,
    library search order, quick-start examples, and full API reference.

- **Phase 12: MATLAB/Octave Bindings**
  - **12.1 MEX C wrapper** (`matlab/openjp3d/src/openjp3d_mex.c`):
    single `mexFunction` entry point dispatching on a command string
    (`load_lib`, `get_version`, `encode`, `decode`, `transcode`).
    Cross-platform runtime loading via `dlopen`/`LoadLibrary`; no
    link-time dependency on the OpenJP3D shared library.  Optional
    MATLAB function-handle message callback via `mexCallMATLAB`.
  - **12.2 `EncodeParams` classdef** (`matlab/openjp3d/EncodeParams.m`):
    MATLAB value class mirroring `opj_jp3d_enc_params_t` with sensible
    lossless defaults and a `to_c()` method that packs fields into the
    `int32(13)` + `double` layout expected by the MEX wrapper.
  - **12.3 High-level package API** (`matlab/openjp3d/+openjp3d/`):
    - `encode(volume [,params [,options]])` — accepts `[D H W]` or
      `[D H W C]` numeric arrays; infers `prec`/`sgnd` from MATLAB
      class; performs column-major → row-major permutation; returns
      `uint8` JP3D codestream.
    - `[volume, dtype] = decode(data [,opts])` — accepts `uint8` vector;
      reconstructs `int32` array with correct axis order; returns dtype
      string (`'uint8'`, `'int8'`, `'uint16'`, `'int16'`, `'int32'`).
    - `transcode_to_ht(data [,params [,opts]])` — wraps
      `opj_jp3d_transcode_to_ht()`; forces `use_htj2k=1`.
    - `get_version()`, `load_lib([path])` — library version and
      runtime loader with env-var / relative-path search order.
  - **12.4 CMake integration** (`matlab/CMakeLists.txt`):
    `BUILD_MATLAB_BINDINGS` option; CTest targets `build_matlab_mex`
    (via `mkoctfile --mex`) and `test_matlab_bindings` (via
    `octave --no-gui`); falls back to MATLAB `mex`/`-batch` when
    Octave is not found.
  - **12.5 Test suite** (`tests/test_matlab.m`): ≥ 40 tests covering
    version, `EncodeParams`, lossless round-trips for all precisions,
    multi-component and single-slice volumes, tiled encoding, HTJ2K,
    lossy encoding, transcode, data layout, callbacks, dtype inference,
    and error handling.  Skips gracefully when the shared library is
    unavailable.
  - **12.6 Documentation** (`matlab/openjp3d/README.md`): installation,
    library search order, quick-start examples, and full API reference.

- **Phase 11: R Bindings & Scientific Computing Integration**
  - **11.1 R package scaffold**: `r/openjp3d/` package with `DESCRIPTION`,
    `NAMESPACE`, and `LICENSE`.  Installable via
    `R CMD INSTALL r/openjp3d` or `devtools::install("r/openjp3d")`.
    BSD-2-Clause licence header on all R and C source files.
  - **11.2 Thin C wrapper** (`r/openjp3d/src/openjp3d_wrap.c`):
    cross-platform loader using `dlopen`/`LoadLibrary` to open the
    openjp3d shared library at runtime (not at compile/link time).
    Provides five `.Call()` entry points registered via `R_init_openjp3d`.
    Search order: `OPENJP3D_LIBRARY` env var → `lib/` subdirectory next to
    the installed package → OS default loader.  `src/Makevars` links `-ldl`
    on Linux/macOS; `src/Makevars.win` uses `LoadLibrary` (auto-linked).
  - **11.3 High-level R API** (`r/openjp3d/R/openjp3d.R`):
    - `encode(volume, params, prec, sgnd, on_message)` — accepts `(D,H,W)`
      or `(D,H,W,C)` integer arrays; returns `raw` JP3D codestream.
    - `decode(data, verbose, on_message)` — accepts `raw` vector; returns
      integer array with `dtype` attribute (`"uint8"`, `"int8"`, `"uint16"`,
      `"int16"`, or `"int32"`).
    - `transcode_to_ht(data, params, on_message)` — wraps
      `opj_jp3d_transcode_to_ht()`.
    - `get_version()` — returns the native library version string.
    - `EncodeParams(...)` — S3 constructor mirroring
      `opj_jp3d_enc_params_t` with R-friendly defaults.
    - Data-layout helpers convert R column-major ↔ C row-major via
      `aperm(arr, c(3,2,1))`.
    - Module-level constants: `FILTER_53`, `FILTER_97`, `USE_HTJ2K`,
      `CS_UNKNOWN`, `CS_SRGB`, `CS_GRAY`, `CS_YUV`, `MSG_INFO`,
      `MSG_WARNING`, `MSG_ERROR`.
  - **11.4 Package init** (`r/openjp3d/R/zzz.R`): `.onLoad()` resolves the
    shared library at package load time and calls `ojp3d_load_lib()`.
  - **11.5 CMake integration** (`r/CMakeLists.txt`):
    `BUILD_R_BINDINGS` option added to root `CMakeLists.txt` (default OFF).
    When ON, registers a `test_r_bindings` CTest target that installs the
    package into `${CMAKE_BINARY_DIR}/r_library` and runs
    `tests/test_r.R` with `OPENJP3D_LIBRARY` set via generator expressions.
    Requires `BUILD_SHARED_LIBS=ON`; skips gracefully when R is not found.
  - **11.6 R test suite** (`tests/test_r.R`): 43 test cases (base R only,
    no external test framework required) covering version string format,
    all exported constants, `EncodeParams` construction, lossless
    round-trips for all five supported precisions, multi-component volumes,
    single-slice edge case, non-square dimensions, 16×16×16 large volume,
    tiled encoding, SOC marker prefix check, HTJ2K round-trip, lossy 9/7
    encoding, `transcode_to_ht` with/without params, corner-voxel and
    gradient data-layout correctness, message callback, `dtype` attribute,
    and error-handling cases.
  - **11.7 Package README** (`r/openjp3d/README.md`): installation steps,
    library search-order documentation, quick-start examples (lossless,
    lossy, HTJ2K, transcode, multi-component, callback), and full API
    reference table.
  - New files: `r/openjp3d/DESCRIPTION`, `r/openjp3d/NAMESPACE`,
    `r/openjp3d/LICENSE`, `r/openjp3d/R/openjp3d.R`, `r/openjp3d/R/zzz.R`,
    `r/openjp3d/src/openjp3d_wrap.c`, `r/openjp3d/src/Makevars`,
    `r/openjp3d/src/Makevars.win`, `r/openjp3d/README.md`,
    `r/CMakeLists.txt`, `tests/test_r.R`.
    `BUILD_R_BINDINGS` CMake option added to root `CMakeLists.txt`.

- **Phase 10: Julia Bindings & Scientific Computing Integration**
  - **10.1 Julia package scaffold**: `julia/OpenJP3D.jl/` package with
    `Project.toml` (Julia ≥ 1.6).  Installable via
    `Pkg.develop(path="julia/OpenJP3D.jl")`.  Requires no C compilation —
    pure `ccall`.  BSD-2-Clause licence header on all Julia source files.
  - **10.2 Shared-library loader** (`lib_path()`): cross-platform loader
    supporting Linux (`.so`), macOS (`.dylib`), and Windows (`.dll`).
    Search order: `OPENJP3D_LIBRARY` environment variable → package `lib/`
    subdirectory → `Libdl.find_library("libopenjp3d")` → bare name.
    Cached — library path resolved once per process.
  - **10.3 ccall bindings** (C structs in `OpenJP3D.jl`): Julia `struct`
    types mirroring `opj_volume_comp_t` (`VolumeComp`), `opj_volume_t`
    (`Volume`), `opj_jp3d_enc_params_t` (`EncParamsC`), and
    `opj_jp3d_dec_params_t` (`DecParamsC`).  Mutable structs used for
    pass-by-reference parameters.  All six public API functions bound via
    `ccall`: `opj_jp3d_get_version`, `opj_jp3d_create_volume`,
    `opj_jp3d_destroy_volume`, `opj_jp3d_encode`, `opj_jp3d_decode`,
    `opj_jp3d_transcode_to_ht`, and `opj_jp3d_free`.
  - **10.4 High-level Julia API** (`julia/OpenJP3D.jl/src/OpenJP3D.jl`):
    - `encode(volume [, params]; on_message)` — accepts `(D,H,W)` or
      `(D,H,W,C)` Julia arrays (element types: `UInt8`, `Int8`, `UInt16`,
      `Int16`, `Int32`); returns raw JP3D `Vector{UInt8}`.
    - `decode(data; verbose, on_message)` — accepts `Vector{UInt8}`; returns
      a Julia array with element type chosen from the encoded bit-depth and
      signedness; single-component volumes as `(D,H,W)`, multi-component as
      `(D,H,W,C)`.
    - `transcode_to_ht(data; params, on_message)` — wraps
      `opj_jp3d_transcode_to_ht()`; forces `use_htj2k=USE_HTJ2K`.
    - `get_version()` — returns the native library version string.
    - `EncodeParams` keyword-argument struct mirrors `opj_jp3d_enc_params_t`
      with Julia-friendly defaults and a `_to_c()` conversion helper.
    - Module-level constants: `FILTER_53`, `FILTER_97`, `USE_HTJ2K`,
      `CS_UNKNOWN`, `CS_SRGB`, `CS_GRAY`, `CS_YUV`, `MSG_INFO`,
      `MSG_WARNING`, `MSG_ERROR`.
    - Data-layout helpers (`_to_row_major`, `_from_row_major`) using
      `permutedims` to bridge Julia column-major ↔ C row-major storage.
    - Message callback via a module-level `@cfunction` with a thread-local
      slot; `on_message(level::Int, msg::String)` API.
  - **10.5 CMake integration** (`julia/CMakeLists.txt`):
    `BUILD_JULIA_BINDINGS` option added to root `CMakeLists.txt`
    (default OFF).  When ON, registers a `test_julia_bindings` CTest
    target running `julia --project=julia/OpenJP3D.jl test/runtests.jl`
    with `OPENJP3D_LIBRARY` set via generator expressions.  Requires
    Julia ≥ 1.6 and `BUILD_SHARED_LIBS=ON`; skips gracefully if Julia is
    not found or the version requirement is not met.
  - **10.6 Julia test suite** (`julia/OpenJP3D.jl/test/runtests.jl`):
    100+ test cases covering version string format, all exported constants,
    `EncodeParams` construction and `_to_c()`, lossless round-trips for all
    five supported element types, multi-component (3- and 4-channel)
    volumes, single-slice edge case, non-square dimensions, large volumes
    (16×16×16), tiled encoding, SOC marker prefix check, HTJ2K round-trip,
    lossy 9/7 encoding, verbose decode, message callback, `transcode_to_ht`
    with and without params, data layout correctness (corner voxels,
    gradient pattern), and error handling (invalid codestream, 2-D/5-D
    input, `Float32`/`Float64` element types).  Tests auto-skip when the
    shared library is not available.
  - **10.7 Package README** (`julia/OpenJP3D.jl/README.md`): installation
    steps, library search-order documentation, quick-start examples
    (lossless, lossy, HTJ2K, transcode, callback), and full API reference.
  - New files: `julia/OpenJP3D.jl/src/OpenJP3D.jl`,
    `julia/OpenJP3D.jl/Project.toml`, `julia/OpenJP3D.jl/test/runtests.jl`,
    `julia/OpenJP3D.jl/README.md`, `julia/CMakeLists.txt`.
    `BUILD_JULIA_BINDINGS` CMake option added to root `CMakeLists.txt`.

- **Phase 9: Python Bindings & NumPy Integration**
  - **9.1 Python package scaffold**: `python/openjp3d/` package with
    `pyproject.toml` (PEP 621) and `setup.py`.  Installable via
    `pip install -e python/[numpy]`.  Requires no C compilation — pure
    ctypes.  BSD-2-Clause licence header on all Python source files.
  - **9.2 Shared-library loader** (`python/openjp3d/_lib.py`):
    cross-platform loader supporting Linux (`.so`), macOS (`.dylib`), and
    Windows (`.dll`).  Search order: `OPENJP3D_LIBRARY` environment
    variable → package directory → `ctypes.util.find_library("openjp3d")`.
    Singleton — loads once per process via module-level cache.
  - **9.3 ctypes bindings** (`python/openjp3d/_bindings.py`): mirrors
    `opj_volume_comp_t`, `opj_volume_t`, `opj_jp3d_enc_params_t`, and
    `opj_jp3d_dec_params_t` as `ctypes.Structure` subclasses with all
    fields typed.  Full `argtypes` / `restype` annotations on all six
    public API functions: `opj_jp3d_get_version`, `opj_jp3d_create_volume`,
    `opj_jp3d_destroy_volume`, `opj_jp3d_encode`, `opj_jp3d_decode`, and
    `opj_jp3d_transcode_to_ht`.  `MsgCallbackType` wraps the
    `opj_jp3d_msg_callback_t` function pointer.
  - **9.4 High-level NumPy API** (`python/openjp3d/__init__.py`):
    - `encode(volume, params, *, on_message)` — accepts `(D,H,W)` or
      `(D,H,W,C)` NumPy arrays (dtypes: `uint8`, `int8`, `uint16`,
      `int16`, `int32`); returns raw JP3D bytes.
    - `decode(data, *, verbose, on_message)` — accepts bytes; returns a
      NumPy array with dtype chosen from the encoded bit-depth/signedness;
      single-component volumes returned as `(D,H,W)`, multi-component as
      `(D,H,W,C)`.
    - `transcode_to_ht(data, params, *, on_message)` — wraps
      `opj_jp3d_transcode_to_ht()`; forces `use_htj2k=USE_HTJ2K`.
    - `get_version()` — returns the native library version string.
    - `EncodeParams` dataclass mirrors `opj_jp3d_enc_params_t` with
      Pythonic defaults and a `_to_c()` conversion method.
    - Module-level constants: `FILTER_53`, `FILTER_97`, `USE_HTJ2K`,
      `CS_UNKNOWN`, `CS_SRGB`, `CS_GRAY`, `CS_YUV`.
  - **9.5 CMake integration** (`python/CMakeLists.txt`):
    `BUILD_PYTHON_BINDINGS` option added to root `CMakeLists.txt`
    (default OFF).  When ON, registers a `test_python_bindings` CTest
    target running `pytest tests/test_python.py` with `OPENJP3D_LIBRARY`
    and `PYTHONPATH` set via generator expressions.  Requires
    `BUILD_SHARED_LIBS=ON`; skips gracefully if Python 3, pytest, or
    NumPy are not found.
  - **9.6 Python test suite** (`tests/test_python.py`): 40+ pytest test
    cases covering version string format, constants, `EncodeParams`
    construction and `_to_c()`, lossless round-trips for all five
    supported dtypes, multi-component (3- and 4-channel) volumes, single-
    slice edge case, non-square dimensions, SOC marker prefix check,
    HTJ2K round-trip, lossy 9/7 encoding, tiled encoding, verbose mode,
    message callback, `transcode_to_ht` with and without params, and
    error handling (invalid codestream, 2-D/5-D input, `float32` dtype).
    Tests auto-skip when the shared library is not available.
  - **9.7 Package README** (`python/README.md`): installation steps,
    quick-start examples (lossless, lossy, HTJ2K, transcode), and library
    search-order documentation.
  - New files: `python/openjp3d/__init__.py`, `python/openjp3d/_bindings.py`,
    `python/openjp3d/_lib.py`, `python/pyproject.toml`, `python/setup.py`,
    `python/README.md`, `python/CMakeLists.txt`, `tests/test_python.py`.


  - **8F.1 Enhanced log console**: Upgraded the log panel from a basic
    ring buffer to a full `GuiLogState` system.  Each entry carries a
    wall-clock timestamp (HH:MM:SS) and severity colour-coding.  New
    toolbar buttons: **Copy** (copies all visible entries to the system
    clipboard) and **Export…** (writes visible entries to a user-specified
    text file via an inline export dialog).  The panel directly implements
    `opj_jp3d_msg_callback_t` via `gui_log_codec_callback()` so that codec
    errors, warnings, and info messages are automatically routed to the GUI
    log from any encode/decode operation.  Thread-safe: the underlying
    entries vector is mutex-protected for calls from background threads.
  - **8F.2 Preferences dialog**: `Tools > Preferences` opens a persistent
    settings dialog grouped into collapsible sections — *Appearance* (theme
    toggle, viewport background colour), *File Paths* (default open and
    output directories), *Performance* (thread count slider), and *Default
    Encoder Preset* (tile size, DWT levels, target rate, filter, HTJ2K,
    threads).  Settings are persisted in a platform-appropriate INI file:
    `~/.config/openjp3d/gui.ini` (Linux / XDG), `~/Library/Preferences/
    openjp3d-gui.ini` (macOS), or `%APPDATA%\openjp3d\gui.ini` (Windows).
    Preferences are loaded on startup and saved on dialog **Save** or
    application exit.  The current config file path is shown in the
    *Configuration File* section for easy location.
  - **8F.3 Cross-platform packaging**: CMake install rules for
    `opj_jp3d_gui` added to the GUI `CMakeLists.txt` using
    `GNUInstallDirs`.  CPack packaging configured per platform: portable
    **ZIP** archive on Windows, **DragNDrop DMG** with macOS `.app` bundle
    on macOS (`MACOSX_BUNDLE` target properties set), and **TGZ** tarball
    on Linux (suitable for repackaging as AppImage or Flatpak).
  - **8F.4 Configurable keyboard shortcuts**: `Tools > Keyboard Shortcuts`
    opens a per-action shortcut editor.  Nine actions are configurable:
    *Open Volume* (default Ctrl+O), *Encode* (E), *Decode* (D),
    *Next Slice* (→), *Previous Slice* (←), *Zoom In* (=), *Zoom Out* (−),
    *Toggle Theme* (Ctrl+T), and *Quit* (Ctrl+Q).  Click any binding button
    to enter capture mode (highlighted in amber); press the desired key
    combination (Ctrl/Shift/Alt + any named key) to rebind; press Escape to
    cancel.  **Reset to Defaults** restores all built-in bindings.  Bindings
    are persisted in the `[shortcuts]` section of the same INI file as
    preferences and reloaded automatically on startup.  The hardcoded
    `io.KeyCtrl` shortcut checks in `opj_jp3d_gui.cpp` have been replaced
    by calls to `gui_shortcuts_check()` throughout the main loop.
  - New source files: `src/bin/jp3d/gui/gui_log_prefs.h`,
    `src/bin/jp3d/gui/gui_log_prefs.cpp`.  `CMakeLists.txt` updated to
    compile them as part of the `opj_jp3d_gui` target and add
    install/CPack rules (8F.3).

- **Phase 8E: JPIP 3-D Streaming Client**
  - **8E.1 JPIP connection dialog**: `Tools > JPIP Connection` (or toolbar
    JPIP button) opens the connection dialog.  Enter a JP3D codestream file
    path to load it into an in-process JPIP server and open a client session.
    Displays session ID, number of datasets, and a selectable list of available
    datasets with dimensions and component count.  Connect/disconnect controls
    with error reporting.
  - **8E.2 Interactive sub-volume browsing**: `Tools > JPIP Browser` opens
    the sub-volume browser panel.  Configure region-of-interest (X/Y/Z offset
    and size), resolution level, and quality layers.  "Fetch Sub-Volume"
    submits a JPIP request in a background thread, decodes the response, and
    displays the result with fetch timing.  "Load into Viewer" transfers the
    fetched sub-volume into the main viewer for slice/3-D visualisation.
    Region controls are clamped to dataset extents.
  - **8E.3 Network diagnostics**: `Tools > JPIP Diagnostics` displays
    cumulative session statistics: bytes transferred (human-readable KB/MB),
    total requests sent, cache hits, cache hit ratio (%), last request
    latency (ms), and average latency (ms).  Reset button clears statistics.
  - New source files: `src/bin/jp3d/gui/gui_jpip.h`,
    `src/bin/jp3d/gui/gui_jpip.cpp`.  `CMakeLists.txt` updated to compile
    them as part of the `opj_jp3d_gui` target and link against `openjpip3d`.

- **Phase 8D: Round-Trip Testing & Validation**
  - **8D.1 Round-trip test wizard**: One-click encode→decode→compare via
    `Tools > Round-Trip Test` or toolbar.  Configurable filter (5/3, 9/7),
    target bit-rate, HTJ2K toggle, and DWT levels.  Background thread runs
    `opj_jp3d_encode()` then `opj_jp3d_decode()` and reports pass/fail for
    lossless (bit-exact) with PSNR, MSE, max absolute error, compression
    ratio, encode time, and decode time.
  - **8D.2 Diff viewer**: Side-by-side comparison of original and decoded
    volumes with an error-map overlay.  Axis selector (axial/sagittal/coronal),
    slice navigation, and configurable difference threshold.  Error map
    highlights differing voxels in red intensity proportional to absolute
    difference.  Accessible from the round-trip results via "Open Diff Viewer".
  - **8D.3 Batch test runner**: Queue multiple encode/decode/round-trip
    configurations with varying parameters (filter, rate, HTJ2K, DWT levels,
    tile size).  "Add Defaults" pre-populates lossless, lossy, and HTJ2K
    configurations.  Results displayed in a sortable table (pass/fail, PSNR,
    MSE, compression ratio, encode/decode times).  "Export CSV" writes results
    to `batch_results.csv`.
  - **8D.4 Codestream inspector**: Tree-view widget showing JP3D codestream
    structure — SOC, SIZ3D (volume size, tile layout, per-component metadata),
    COD3D (filter, HTJ2K flag, DWT levels, code-block size, layers), QCD3D
    (target rate), SOT (tile-part index, data length), SOD, and EOC markers
    with byte offsets.  Parses any `.jp3d`/`.j3d` file on demand.
  - New source files: `src/bin/jp3d/gui/gui_roundtrip.h`,
    `src/bin/jp3d/gui/gui_roundtrip.cpp`.  `CMakeLists.txt` updated to
    compile them as part of the `opj_jp3d_gui` target.

- **Phase 8C: Encoding & Decoding Controls**
  - **8C.1 Encode panel**: `Tools > Encode` (or toolbar) opens the Encode
    panel with full encoder parameter controls: tile size (X/Y/Z),
    decomposition levels (0–8), code-block size, target bit-rate (0 = lossless),
    lossless 5/3 vs. lossy 9/7 filter selection, HTJ2K mode toggle, and thread
    count.  "Encode" button triggers background encoding of the loaded volume
    via `opj_jp3d_encode()` and writes the result to the specified output path.
  - **8C.2 Decode panel**: `Tools > Decode` opens the Decode panel with
    decoder options: input JP3D codestream path, optional sub-volume extraction
    (offset + size), reduced resolution level, and single-slice mode.
    "Decode" button runs `opj_jp3d_decode()` in a background thread and
    auto-loads the result into the viewer on completion.
  - **8C.3 Transcode panel**: `Tools > Transcode` opens the Transcode panel
    for EBCOT ↔ HTJ2K transcoding.  Select an input JP3D codestream, choose
    target mode (HTJ2K or EBCOT), specify an output path, and execute.
    HTJ2K transcoding uses `opj_jp3d_transcode_to_ht()`; EBCOT transcoding
    performs a full decode + re-encode cycle.
  - **8C.4 Progress & cancellation**: Progress window shows an animated
    progress bar, elapsed time (minutes:seconds), and a Cancel button for
    all long-running encode/decode/transcode operations.  Operations run in
    a background `std::thread` to keep the GUI responsive.  On completion the
    window shows success/failure status with error details.
  - New source files: `src/bin/jp3d/gui/gui_codec.h`,
    `src/bin/jp3d/gui/gui_codec.cpp`.  `CMakeLists.txt` updated to compile
    them as part of the `opj_jp3d_gui` target.

- **Phase 8B: Volume Loading & Visualisation**
  - **8B.1 File open dialog**: `Open Volume` dialog (File menu, toolbar, Ctrl+O)
    accepts JP3D codestreams (`.jp3d`, `.j3d`) decoded via `opj_jp3d_decode()`
    and raw binary volumes (`.raw`, `.vol`) loaded via `opj_raw_io_read()`.
    Raw-file parameters (width, height, depth, precision, signed flag,
    component count) are entered inline in the dialog; format is auto-detected
    from the file extension.
  - **8B.2 Slice viewer**: 2-D slice viewport rendering axial (Z), sagittal (X),
    or coronal (Y) planes.  The current axis and slice index are controlled by
    radio-buttons, a slider, and scroll-wheel.  Each frame the active slice is
    extracted from the loaded `opj_volume_t`, window/level–normalised, and
    uploaded as an RGBA8 OpenGL texture displayed via `ImGui::Image()`.
    Aspect-ratio–preserving fit is applied automatically.
  - **8B.3 3-D volume rendering**: GPU-accelerated ray-cast renderer toggled
    from the viewport toolbar.  Renders component 0 into an FBO colour
    attachment via a full-screen GLSL shader that marches 128 steps through a
    3-D `GL_R8` volume texture.  Configurable azimuth, elevation, and density
    controls; the 3-D texture is lazily rebuilt whenever the volume changes.
  - **8B.4 Metadata display**: Volume Info panel now shows real data — voxel
    dimensions (W × H × D), bit-depth, component count, voxel dz spacing,
    DWT resolution levels, filter type (5/3 lossless vs 9/7 lossy), and HTJ2K
    flag (populated for JP3D codestreams).
  - **8B.5 Histogram & statistics**: `gui_volume_compute_stats()` computes
    per-component min, max, mean, and standard deviation in a single pass.
    Results are shown in the Volume Info panel together with a 256-bin
    normalised intensity histogram rendered via `ImGui::PlotHistogram()`.
    Window/level sliders are pre-seeded from the computed min/max and update
    the slice texture in real time.
  - New source files: `src/bin/jp3d/gui/gui_volume.h`,
    `src/bin/jp3d/gui/gui_volume.cpp`.  `CMakeLists.txt` updated to compile
    them as part of the `opj_jp3d_gui` target.

- **Phase 8A: Application Framework & UI Shell**
  - **8A.1 GUI toolkit selection**: Evaluated Qt 6, GTK 4, wxWidgets, and
    Dear ImGui + SDL2. Selected Dear ImGui (v1.91.8) + SDL2 + OpenGL 3.3
    for minimal dependency footprint and immediate-mode rendering. Rationale
    documented in `doc/gui-toolkit-rationale.md`.
  - **8A.2 Application scaffold**: Created `opj_jp3d_gui` target under
    `src/bin/jp3d/gui/`. Added `BUILD_GUI_TOOLS` CMake option (default OFF)
    with FetchContent for Dear ImGui (docking branch) and SDL2. C++ enabled
    only when `BUILD_GUI_TOOLS=ON`; core library build is unaffected.
  - **8A.3 Main window layout**: Dockable panel layout with menu bar
    (File/View/Tools/Help), toolbar (Open, Encode, Decode, Transcode,
    Round-Trip, Theme toggle), file-browser panel, volume-info panel,
    slice/volume viewport, and log/console panel. Panels are resizable
    and dockable via Dear ImGui's built-in docking system.
  - **8A.4 Theme & accessibility**: Dark and light themes with WCAG 2.1 AA
    contrast compliance. Keyboard navigation enabled (ImGuiConfigFlags_NavEnableKeyboard).
    Theme toggle via View menu, toolbar button, or Ctrl+T shortcut.

## [1.0.0] - 2026-02-21

### Added

- Phase 7: Integration Testing & Release Preparation.
  - **7.1 Conformance testing**: `tests/test_conformance.c` — 26 tests
    covering edge-case JP3D codestreams: single-slice volumes (d=1),
    single-row (h=1), single-column (w=1), 1×1×1 minimal volumes, full
    8-bit value coverage, 16-bit and 32-bit precision, signed 8-bit and
    16-bit samples, multi-component (3 and 4 components), tiled encoding,
    non-power-of-2 dimensions, asymmetric dimensions, large volumes
    (64×64×64), HTJ2K mode round-trips, and lossy mode with PSNR
    verification.
  - **7.2 Real-world datasets**: `tests/test_integration.c` — 20 tests
    simulating medical imaging (CT/MRI), satellite/geospatial stacks,
    microscopy data, multi-tile encoding, HTJ2K round-trips, lossy
    compression with PSNR checks, multi-resolution decomposition, dense
    data, and constant-value volumes.
  - **7.3 Upstream compatibility**: `tests/test_compat.c` — 34 tests
    verifying version strings, version macros, API function callability,
    boolean/filter/colour-space/HTJ2K constants, default parameter values,
    and encode→decode API stability.
  - **7.4 Security review**: `tools/fuzz/fuzz_decode.c` — libFuzzer/AFL++
    harness for decoder paths; `BUILD_FUZZ` CMake option; build
    instructions for sanitizer-enabled fuzzing.
  - **7.5 Performance report**: `doc/performance-report.md` documenting
    benchmark methodology, reference results across architectures and
    compression modes, and reproduction instructions.
  - **7.6 Release packaging**: CMake install targets for libraries, headers,
    and CLI binaries via `GNUInstallDirs`; `cmake/openjp3d.pc.in` pkg-config
    template; `cmake/OpenJP3DConfig.cmake.in` for `find_package(OpenJP3D)`;
    CPack source tarball configuration (TGZ/TXZ); versioned shared library
    support (`libopenjp3d.so.1.0.0`).
  - **7.7 Tagging & changelog**: Version bumped to 1.0.0 in `CMakeLists.txt`
    and `openjp3d.h`; complete `CHANGELOG.md` entries for all phases.
  - `BUILD_FUZZ` CMake option added (default OFF, Clang-only).
  - `INSTALL.md` updated with new build options and install instructions.

- Phase 6: Documentation & Examples.
  - **6.1 API reference**: Comprehensive Doxygen annotations on all public
    types, functions, and macros in `openjp3d.h` and `openjpip3d.h`.
  - **6.2 User guide**: `doc/user-guide.md` — building from source, C library
    API usage, CLI tool usage, advanced options (HTJ2K, lossy, tiling,
    multi-component, JPIP streaming).
  - **6.3 Code examples**: Five self-contained C programs in `doc/examples/`:
    `encode_volume.c`, `decode_volume.c`, `partial_decode.c`,
    `jpip3d_client.c`, `htj2k_encode.c`. All compile and run successfully.
    Buildable via `BUILD_DOC_EXAMPLES` CMake option.
  - **6.4 Architecture document**: `doc/architecture.md` describing internal
    module structure, data flow (encoding/decoding/JPIP), and extension points.
  - **6.5 Migration guide**: `doc/migration-from-legacy.md` for users of the
    deprecated OpenJPEG ≤ 2.4.0 JP3D code, with API mapping table, code
    examples, and migration checklist.
  - **6.6 CONTRIBUTING guide**: `CONTRIBUTING.md` with build instructions,
    test procedures, sanitizer builds, coding conventions, directory structure,
    and PR workflow.
  - `BUILD_DOC_EXAMPLES` CMake option added (default OFF).
  - `INSTALL.md` updated with current build options.

- Phase 5: Command-Line Tools.
  - **5.1 `opj_jp3d_compress`**: CLI tool to encode a raw binary volume into
    a JP3D codestream.  Options: tile size (`-t`), decomposition levels (`-n`),
    target bit-rate (`-r`), lossless/lossy filter selection (`-f 53|97`),
    HTJ2K mode (`-H2K`), bit depth (`-p`), signed samples (`-s`), number of
    components (`-c`), and verbose output (`-v`).
  - **5.2 `opj_jp3d_decompress`**: CLI tool to decode a JP3D codestream into
    a raw binary volume file.  Supports verbose output and the full JP3D
    feature set including HTJ2K auto-detection.
  - **5.3 `opj_jp3d_dump`**: CLI tool to inspect a JP3D codestream and print
    marker segments (SOC, SIZ3D, COD3D, QCD3D, SOT, SOD, EOC), volume
    dimensions, tile layout, filter type, code-block sizes, and HTJ2K flag.
  - **5.4 `opj_jp3d_transcode`**: CLI tool to transcode a JP3D codestream from
    EBCOT to HTJ2K block coding without full decode/re-encode.
  - **5.5 `opj_jpip3d_server`**: Standalone JPIP server CLI for JP3D datasets.
    Reads JPIP 3-D query strings from stdin, processes them via the Phase 3
    server library, and writes responses to stdout.
  - **5.6 Man pages**: Troff man pages for all five tools, installable to
    `share/man/man1/`.
  - `BUILD_CLI_TOOLS` CMake option now defaults to ON.
  - End-to-end CLI tests (`tests/test_cli.c`): 12 test cases covering
    round-trip compress→decompress (8-bit, 16-bit, HTJ2K, signed), dump
    marker verification, EBCOT→HTJ2K transcode round-trip, `--help`/`--version`
    exit codes, missing-argument error handling, verbose mode, volume size
    in dump output, and custom decomposition levels.

- Phase 4: SIMD Optimisation.
  - **4.1 Profiling baseline**: critical hotspots identified as the 3-D DWT
    (separable 5/3 lifting along X/Y/Z) and entropy coding; the X-direction
    row transform (contiguous memory) is the primary SIMD target.
  - **4.2 x86-64 SSE4.1 / AVX2** (`opj_dwt3d_sse41.c`, `opj_dwt3d_avx2.c`):
    vectorised forward and inverse 5/3 integer lifting steps.
    SSE4.1 processes 4 `int32` samples per SIMD word; AVX2 processes 8.
    De-interleaving uses shuffle+unpack (SSE4.1) or `permutevar8x32` (AVX2).
    Scatter-back via `_mm_unpacklo/hi_epi32` stores.  Both paths are compiled
    with per-file `-msse4.1` / `-mavx2` flags.
  - **4.3 AArch64 NEON** (`opj_dwt3d_neon.c`): equivalent NEON implementation
    using `vld2q_s32` / `vst2q_s32` for zero-cost de-interleave/re-interleave.
  - **4.4 Apple A/M-series**: the NEON path is compatible with Apple Silicon
    (M1–M4); no extra flags are required since NEON is mandatory on AArch64.
  - **4.5 Runtime dispatch** (`opj_cpu.h`, `opj_cpu.c`): `opj_cpu_features()`
    detects SSE2, SSE4.1, AVX2 (via CPUID) and NEON (compile-time for
    AArch64) at runtime; result is memoised.  `opj_cpu_features_str()` returns
    a human-readable feature string.  `opj_dwt3d.c` uses static-cached
    dispatch functions (`dwt53_fwd_1d_dispatch`, `dwt53_inv_1d_dispatch`)
    that select AVX2 → SSE4.1 → NEON → scalar in priority order.
  - **4.6 Multi-threading**: optional OpenMP parallelism (enabled with
    `-DENABLE_OPENMP=ON`) on the X-direction row loops in `opj_dwt3d_fwd`
    and `opj_dwt3d_inv`; each thread uses a private scratch buffer.
  - **4.7 Benchmarks** (`tools/benchmark/bench_dwt3d.c`): reproducible DWT
    throughput benchmark reporting MVoxels/sec for forward and inverse 5/3
    and 9/7 transforms across configurable volume sizes and decomposition
    levels.  Enabled with `-DBUILD_BENCHMARKS=ON`.
  - SIMD correctness tests (`tests/test_simd.c`): 89 test cases covering
    CPU feature detection, SSE4.1/AVX2/NEON 1-D DWT correctness vs scalar
    reference (19 row lengths × 2 directions × up to 3 ISA paths), and
    3-D DWT dispatch round-trips on 10 volume configurations.
  - `opj_dwt3d_simd.h`: internal header declaring the SIMD 1-D DWT functions
    with `OPJ_JP3D_HAVE_*` guards for clean conditional compilation.
  - `CMakeLists.txt`: added `ENABLE_OPENMP` and `BUILD_BENCHMARKS` options.

- Phase 3: JPIP Extension for JP3D (Part 9).
  - **3.1 JPIP 3-D request model**: `opj_jpip3d_request_t` with `fsiz3d`, `roff3d`, `rsiz3d`
    Z-axis parameters for sub-volume window-of-interest access.
  - **3.2 JPT/JPP-stream 3-D**: tile-part and precinct-part streaming for 3-D indices
    via `opj_jpip3d_jpt_write_tile_part()` and `opj_jpip3d_jpp_write_precinct()`.
  - **3.3 Cache model**: server-side cache tracking delivered 3-D precincts/tiles
    per session via `opj_jpip3d_cache_t`.
  - **3.4 Server**: `opj_jpip3d_server_t` handling JP3D datasets, region extraction,
    and per-session cache management.
  - **3.5 Client library**: `opj_jpip3d_session_t` for opening sessions, requesting
    volumetric regions, and receiving decoded sub-volumes via the in-process server API.
  - **3.6 Metadata embedding**: `opj_jpip3d_metadata_t` wrapping XML strings,
    serialization/deserialization as JP2/JP3D XML boxes.
  - Integration tests: request parsing, cache model, metadata boxes, JPT/JPP stream
    headers, server+client round-trips, multi-session independence.

- Phase 2: HTJ2K high-throughput block coder integration.
  - **2.1 HT block coder — 3-D** (`opj_ht3d.c` / `opj_ht3d.h`): FBCOT-derived
    Fast Block Coder with Optimised Truncation adapted for 3-D code-blocks.
    Uses MEL (Minimum Entropy Length) entropy coding for the significance
    stream and Exp-Golomb / MagSgn variable-length coding for coefficient
    magnitudes and signs, following the structure of ISO/IEC 15444-15 extended
    to three dimensions.
  - **2.2 Cleanup-pass replacement**: the HT block coder performs a single
    forward significance + magnitude pass (MEL + MagSgn), replacing the
    multi-pass EBCOT Tier-1 coder when enabled.
  - **2.3 API flag**: `OPJ_JP3D_USE_HTJ2K` constant and `use_htj2k` field in
    `opj_jp3d_enc_params_t`. When set, the encoder uses the HT block coder for
    every code-block; the decoder auto-detects the coding mode from the COD3D
    marker in the codestream.
  - **2.4 Transcoding**: `opj_jp3d_transcode_to_ht()` — losslessly transcode
    an existing EBCOT or HT JP3D codestream to the HT block coder, preserving
    tile structure, DWT parameters, and filter selection.
  - CTest suite expanded with 39 HTJ2K tests covering: HT block-level
    encode→decode round-trips (all-zero, sparse, alternating, signed, 8-bit,
    16-bit, large magnitude, 1×1×1 through 8×8×8), full-codec HT round-trips
    (multi-component, multi-tile), and EBCOT→HT transcoding.

- Phase 1: Core JP3D codec implementation.
  - **1A Data Structures & I/O**: `opj_volume_t`, `opj_volume_comp_t`,
    JP3D codestream marker segments (SOC, SIZ3D, COD3D, QCD3D, SOT, SOD,
    EOC), and raw binary volume I/O for 8/16/32-bit signed/unsigned volumes.
  - **1B 3-D Wavelet Transform**: separable 3-D DWT with 5/3 integer
    lifting (lossless) and 9/7 float lifting (lossy), configurable
    decomposition levels per axis, and symmetric boundary extension.
  - **1C Entropy Coding**: EBCOT 3-D tier-1 bit-plane coder with
    significance state machine and sign coding; tier-2 packet formation
    and layer coding; basic post-compression rate control for lossy mode.
  - **1D Public API**: `opj_jp3d_create_volume`, `opj_jp3d_destroy_volume`,
    `opj_jp3d_encode`, `opj_jp3d_decode`, default-parameter helpers,
    memory-management wrappers, and callback-based error/warning/info
    event manager.
  - CTest suite expanded to 70 test cases (volume lifecycle, DWT
    round-trips, encode→decode lossless round-trips, parameter
    validation, raw I/O).
- Phase 0: Project bootstrapping — repository structure, CMake build system,
  CI/CD pipeline, coding standards, documentation skeleton.
