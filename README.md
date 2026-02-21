# OpenJP3D

**JPEG 2000 Part 10 (JP3D) volumetric codec**

OpenJP3D is a C library implementing JPEG 2000 Part 10 (JP3D) volumetric
compression (ISO/IEC 15444-10), built as an extension of the
[OpenJPEG](https://github.com/uclouvain/openjpeg) project (version 2.5.4).

## Features

- Lossless and lossy compression of 3-D image volumes
- Optional HTJ2K high-throughput block coding (JPEG 2000 Part 15)
- JPIP interactive streaming for volumetric data (JPEG 2000 Part 9)
- SIMD-optimised paths for x86-64 (SSE4.1/AVX2) and AArch64 (NEON)
- Command-line tools: `opj_jp3d_compress`, `opj_jp3d_decompress`,
  `opj_jp3d_dump`, `opj_jp3d_transcode`, `opj_jpip3d_server`
- **Python bindings** (`python/openjp3d/`) — pure-ctypes package with NumPy
  integration; no C compilation required:
  - `openjp3d.encode(numpy_array)` → JP3D bytes
  - `openjp3d.decode(bytes)` → NumPy array
  - `openjp3d.transcode_to_ht(bytes)` → HTJ2K JP3D bytes
  - Supports `uint8`, `int8`, `uint16`, `int16`, `int32` dtypes; single-
    and multi-component `(D,H,W)` / `(D,H,W,C)` array layouts
  - `EncodeParams` dataclass with lossless/lossy/HTJ2K configuration
  - Message callbacks for codec errors/warnings/info
  - Installable via `pip install -e python/[numpy]`
- **Julia bindings** (`julia/OpenJP3D.jl/`) — pure-`ccall` package; no C
  compilation required:
  - `OpenJP3D.encode(array)` → `Vector{UInt8}` JP3D codestream
  - `OpenJP3D.decode(data)` → Julia `Array` with correct element type
  - `OpenJP3D.transcode_to_ht(data)` → HTJ2K JP3D codestream
  - Supports `UInt8`, `Int8`, `UInt16`, `Int16`, `Int32` element types;
    single- and multi-component `(D,H,W)` / `(D,H,W,C)` array layouts
  - `EncodeParams` keyword-argument struct with lossless/lossy/HTJ2K options
  - Message callbacks for codec errors/warnings/info
  - Installable via `Pkg.develop(path="julia/OpenJP3D.jl")`
- **R bindings** (`r/openjp3d/`) — R package with a thin C wrapper using
  dynamic library loading; no pre-installed openjp3d headers required:
  - `encode(volume, prec, sgnd)` → `raw` JP3D codestream
  - `decode(data)` → integer `array` with `dtype` attribute
  - `transcode_to_ht(data)` → HTJ2K JP3D codestream
  - Supports 8, 16, and 32-bit integer arrays (signed and unsigned);
    single- and multi-component `(D,H,W)` / `(D,H,W,C)` layouts
  - `EncodeParams` S3 constructor with lossless/lossy/HTJ2K options
  - Message callbacks for codec errors/warnings/info
  - Installable via `R CMD INSTALL r/openjp3d` or `devtools::install()`
- Interactive GUI test application (`opj_jp3d_gui`) built with
  Dear ImGui + SDL2 + OpenGL 3.3 (optional, `BUILD_GUI_TOOLS=ON`):
  - File open dialog for `.jp3d`/`.j3d` codestreams and raw `.raw`/`.vol` volumes
  - 2-D slice viewer (axial/sagittal/coronal) with window/level controls and
    scroll-wheel navigation
  - 3-D ray-cast volume renderer (GPU-accelerated GLSL, configurable
    azimuth/elevation/density)
  - Metadata panel showing dimensions, bit-depth, components, DWT levels, and
    compression mode
  - Per-component statistics (min/max/mean/std-dev) and 256-bin intensity
    histogram
  - Encode panel with full encoder parameter controls (tile size, DWT levels,
    code-block size, bit-rate, filter, HTJ2K toggle, threads)
  - Decode panel with sub-volume extraction, resolution reduction, and
    single-slice mode options
  - Transcode panel for EBCOT ↔ HTJ2K transcoding
  - Progress window with elapsed time and cancellation for long-running
    operations (background threading)
  - Round-trip test wizard: one-click encode→decode→compare with PSNR/MSE
    metrics, pass/fail verdict, compression ratio, and timing
  - Diff viewer: side-by-side original vs decoded comparison with error-map
    overlay and configurable difference threshold
  - Batch test runner: queue multiple parameter configurations, results table,
    and CSV export
  - Codestream inspector: tree-view of JP3D codestream markers (SOC, SIZ3D,
    COD3D, QCD3D, SOT, SOD, EOC) with byte offsets and field details
  - JPIP 3-D connection dialog: connect to an in-process JPIP server, browse
    available datasets with dimensions and component info
  - JPIP sub-volume browser: interactive region-of-interest request with
    resolution level and quality layer controls, background fetch with timing
  - JPIP network diagnostics: session statistics including bytes transferred,
    cache hit ratio, and request/response latency
  - Enhanced log console with HH:MM:SS timestamps, severity colour-coding,
    copy-to-clipboard, and export-to-file; routes all codec messages
    (`opj_jp3d_msg_callback_t`) to the GUI log automatically
  - Preferences dialog: persistent settings (default paths, encoder presets,
    background colour, theme, threads) stored in a platform-appropriate INI
    file (`~/.config/openjp3d/gui.ini` on Linux)
  - Configurable keyboard shortcuts for 9 actions (Open, Encode, Decode,
    Next/Prev Slice, Zoom In/Out, Toggle Theme, Quit) with an interactive
    key-capture editor; bindings persisted in the preferences INI file
  - Cross-platform packaging rules: portable ZIP (Windows), DragNDrop DMG
    with `.app` bundle (macOS), TGZ tarball (Linux)
- **Go bindings** (`go/openjp3d/`) — CGo package with runtime
  `dlopen`/`LoadLibrary` loading; no link-time dependency on the shared
  library at Go build time:
  - `Encode(samples, w, h, d, numComps, prec, signed, params, cb) → []byte`
  - `Decode(data, params, cb) → ([]int32, VolumeInfo, error)`
  - `TranscodeToHT(src, params, cb) → []byte`
  - `EncodeParams` struct with lossless/lossy/HTJ2K options
  - `MsgCallback` function type for codec error/warning/info messages
  - Idempotent `LoadLib` with auto-search via `OPENJP3D_LIBRARY` env var
  - Requires Go ≥ 1.21; enabled via `BUILD_GO_BINDINGS=ON`
- **Rust bindings** (`rust/openjp3d/`) — Rust crate using `libloading`
  for runtime `dlopen`/`LoadLibrary` loading; no link-time dependency:
  - `encode(samples, w, h, d, num_comps, prec, signed, params, cb) → Vec<u8>`
  - `decode(data, params, cb) → (Vec<i32>, VolumeInfo)`
  - `transcode_to_ht(src, params, cb) → Vec<u8>`
  - `EncodeParams` and `DecodeParams` structs; `VolumeInfo` metadata
  - Callback support via `&dyn Fn(i32, &str)` closures
  - Idempotent `load_lib` with auto-search via `OPENJP3D_LIBRARY` env var
  - Enabled via `BUILD_RUST_BINDINGS=ON`

## Building

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

See [INSTALL.md](INSTALL.md) for detailed build instructions and options.

## Project Status

OpenJP3D is under active development. The following phases are complete:

- **Phase 0** — Project Bootstrapping ✅
- **Phase 1** — Core JP3D Codec (Lossless & Lossy) ✅
- **Phase 2** — HTJ2K Integration (Part 15) ✅
- **Phase 3** — JPIP Extension for JP3D (Part 9) ✅
- **Phase 4** — SIMD Optimisation ✅
- **Phase 5** — Command-Line Tools ✅
- **Phase 6** — Documentation & Examples ✅
- **Phase 7** — Integration Testing & Release ✅
- **Phase 8A** — GUI Application Framework & UI Shell ✅
- **Phase 8B** — GUI Volume Loading & Visualisation ✅
- **Phase 8C** — GUI Encoding & Decoding Controls ✅
- **Phase 8D** — GUI Round-Trip Testing & Validation ✅
- **Phase 8E** — GUI JPIP 3-D Streaming Client ✅
- **Phase 8F** — GUI Logging, Preferences & Platform Support ✅
- **Phase 9** — Python Bindings & NumPy Integration ✅
- **Phase 10** — Julia Bindings & Scientific Computing Integration ✅
- **Phase 11** — R Bindings & Scientific Computing Integration ✅
- **Phase 12** — MATLAB/Octave Bindings ✅
- **Phase 13** — Go Bindings ✅
- **Phase 14** — Rust Bindings ✅

**Current release: v1.0.0** (phases 0–7); phases 8–14 in `[Unreleased]`.

See [milestone.md](milestone.md) for the full implementation plan,
[CHANGELOG.md](CHANGELOG.md) for detailed change history,
[doc/manual-testing.md](doc/manual-testing.md) for the comprehensive manual
testing guide (107 test procedures covering every feature), and
[doc/test-automation-plan.md](doc/test-automation-plan.md) for the plan to
automate manual tests with deterministic test data.

## Licence

BSD-2-Clause — see [LICENSE](LICENSE) for details.