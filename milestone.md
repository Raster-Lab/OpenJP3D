# OpenJP3D — Implementation Milestone Plan

## Project Overview

OpenJP3D is a C library implementing **JPEG 2000 Part 10 (JP3D)** volumetric compression
(ISO/IEC 15444-10) as an extension of the [OpenJPEG](https://github.com/uclouvain/openjpeg) project
(version 2.5.4). The library delivers lossless and lossy compression of 3-D image volumes with
optional **Part 15 HTJ2K** high-throughput block coding and extends the existing **Part 9 JPIP**
interactive protocol to support volumetric data.

### Design Principles

| Principle | Detail |
|-----------|--------|
| **Compatibility** | Code style, build system (CMake), documentation, and public API conventions match OpenJPEG 2.5.4. |
| **Upstream Sync** | Source layout mirrors `src/lib/openjp2/` so future OpenJPEG releases can be merged with minimal conflict. |
| **Performance** | SIMD-optimised paths for x86-64 (SSE4.1/AVX2), AArch64 (NEON), and Apple A/M-series (NEON + AMX where beneficial). |
| **Modularity** | JP3D, HTJ2K, and JPIP-3D are independent build targets that can be enabled or disabled individually. |

---

## Phase 0 — Project Bootstrapping ✅ Complete

**Goal:** Establish repository structure, build infrastructure, CI, and coding standards.

### Deliverables

| # | Task | Details |
|---|------|---------|
| 0.1 | Repository layout | Create directory structure mirroring OpenJPEG conventions. Key directories: `src/lib/openjp3d/`, `src/bin/jp3d/`, `src/lib/openjpip3d/`, `tests/`, `doc/`, `thirdparty/`, `cmake/`. |
| 0.2 | CMake build system | Root `CMakeLists.txt` with options: `BUILD_JP3D`, `BUILD_HTJ2K_3D`, `BUILD_JPIP_3D`, `BUILD_CLI_TOOLS`, `BUILD_DOC`, `BUILD_TESTING`. Support Debug / Release / RelWithDebInfo. |
| 0.3 | Coding standards | Document in `CODING_STYLE.md`: C11, OpenJPEG naming conventions (`opj_` prefix), Doxygen-style comments, 4-space indentation, `clang-format` config. |
| 0.4 | CI / CD pipeline | GitHub Actions workflows for Linux (GCC, Clang), macOS (Apple Clang), Windows (MSVC). Matrix builds for x86-64 and AArch64. CTest integration for automated test execution. |
| 0.5 | Dependency management | Vendor or `FetchContent` for required third-party code (e.g., libtiff, libpng for test I/O). Keep `thirdparty/` layout consistent with upstream OpenJPEG. |
| 0.6 | Licence & documentation skeleton | BSD-2-Clause licence (matching OpenJPEG). Scaffold `README.md`, `INSTALL.md`, `AUTHORS.md`, `CHANGELOG.md`, Doxygen config in `doc/`. |
| 0.7 | Legacy code audit | Check out OpenJPEG ≤ 2.4.0 JP3D sources (`src/lib/openjp3d/`). Evaluate re-usability vs. writing fresh code. Document findings in `doc/legacy-audit.md`. |

### Exit Criteria

- `cmake --build .` succeeds on all CI targets (empty library, no functionality yet).
- Doxygen generates documentation without warnings.
- `clang-format` and `clang-tidy` pass on skeleton sources.

---

## Phase 1 — Core JP3D Codec (Lossless & Lossy) ✅ Complete

**Goal:** Implement the JP3D encoder and decoder supporting both lossless and lossy modes.

### 1A — Data Structures & I/O

| # | Task | Details |
|---|------|---------|
| 1A.1 | Volume data model | `opj_volume_t` structure: 3-D tile grid, component info, bit-depth, signedness, inter-slice spacing metadata. Mirrors `opj_image_t` with an added Z-axis. |
| 1A.2 | Codestream syntax | Implement JP3D marker segments: SOC, SIZ3D (extended SIZ for Z dimension), COD3D, COC3D, QCD3D, QCC3D, RGN3D, POC3D, SOT, SOD, EOC. |
| 1A.3 | File format (JP3D box) | JP3D file format boxes extending JP2: JP3D Signature, Header, Volume Header, Colour Specification with volumetric metadata. |
| 1A.4 | Raw volume I/O | Read/write raw binary volumes (`.raw`, `.vol`) with configurable dimensions, bit-depth, byte-order. |
| 1A.5 | TIFF stack I/O | Read/write multi-page TIFF as volume slices (via libtiff). |

### 1B — 3-D Wavelet Transform

| # | Task | Details |
|---|------|---------|
| 1B.1 | Separable 3-D DWT | Lifting-based separable discrete wavelet transform along X, Y, Z. Support 5/3 (reversible/lossless) and 9/7 (irreversible/lossy) filter banks. |
| 1B.2 | Multi-resolution decomposition | Configurable decomposition levels per axis (dx, dy, dz). Support anisotropic decomposition for datasets where Z resolution differs from X/Y. |
| 1B.3 | Tile-based processing | Apply DWT within 3-D tiles. Support tile sizes configurable independently per axis. |
| 1B.4 | Boundary handling | Symmetric extension at volume and tile boundaries, consistent with JPEG 2000 Part 1 conventions. |

### 1C — Entropy Coding (EBCOT 3-D)

| # | Task | Details |
|---|------|---------|
| 1C.1 | 3-D code-blocks | Extend EBCOT to 3-D code-blocks (configurable up to 64×64×64). Context modelling adapted for the Z-axis neighbourhood. |
| 1C.2 | Tier-1 coding | Bit-plane coding with 3-D context windows. Significance propagation, magnitude refinement, and cleanup passes extended to three dimensions. |
| 1C.3 | Tier-2 coding | Packet header coding, layer formation, and packet sequencing with Z-dimension precinct support. |
| 1C.4 | Rate control | Post-Compression Rate-Distortion (PCRD) optimisation across 3-D code-blocks for lossy mode. Truncation-point selection algorithm. |

### 1D — Public API

| # | Task | Details |
|---|------|---------|
| 1D.1 | Encoder API | `opj_jp3d_encode()` — accepts `opj_volume_t`, compression parameters, output stream. |
| 1D.2 | Decoder API | `opj_jp3d_decode()` — accepts codestream, returns `opj_volume_t`. Supports partial decoding (sub-volume, single slice, reduced resolution). |
| 1D.3 | Parameter helpers | `opj_jp3d_set_default_encoder_parameters()`, `opj_jp3d_set_default_decoder_parameters()`. |
| 1D.4 | Memory management | Pluggable allocator (`opj_malloc`, `opj_free`, `opj_aligned_malloc`) consistent with OpenJPEG conventions. |
| 1D.5 | Error handling | Callback-based error/warning/info logging via `opj_event_mgr_t`. |

### Exit Criteria

- Round-trip (encode → decode) of synthetic 3-D volumes with bit-exact lossless output.
- Lossy compression at target bit-rates with measurable PSNR.
- CTest suite with ≥ 50 test cases covering edge cases (1-slice volume, maximum tile dimensions, all supported bit-depths).

---

## Phase 2 — HTJ2K Integration (Part 15 in JP3D) ✅ Complete

**Goal:** Provide an optional high-throughput block coder for JP3D, modelled on JPEG 2000 Part 15 (HTJ2K / ISO/IEC 15444-15).

### Deliverables

| # | Task | Details |
|---|------|---------|
| 2.1 | HT block coder — 3-D | Implement the FBCOT (Fast Block Coder with Optimised Truncation) algorithm extended to 3-D code-blocks. Replaces Tier-1 EBCOT when enabled. |
| 2.2 | Cleanup pass replacement | Replace the EBCOT cleanup pass with the HT cleanup pass (VLC + MagSgn coding), adapted for 3-D context. |
| 2.3 | API flag | `OPJ_JP3D_USE_HTJ2K` parameter flag. When set, encoder uses HT block coder; decoder auto-detects from marker. |
| 2.4 | Transcoding | Utility function `opj_jp3d_transcode_to_ht()` — lossless transcoding from legacy EBCOT 3-D codestream to HT codestream and vice versa. |
| 2.5 | Reference: OpenJPH | Evaluate [OpenJPH](https://github.com/aous72/OpenJPH) (open-source HTJ2K) as a reference for the block-coder implementation. Document any borrowed algorithms and their licences. |

### Exit Criteria

- Encode/decode round-trip with HTJ2K mode produces correct output.
- Throughput benchmarks show ≥ 5× speedup over EBCOT 3-D at moderate bit-rates.
- Lossless transcoding between EBCOT 3-D and HT 3-D codestreams is bit-exact.

---

## Phase 3 — JPIP Extension for JP3D (Part 9) ✅ Complete

**Goal:** Extend OpenJPEG's JPIP (Part 9) server and client infrastructure to support interactive streaming of JP3D volumetric datasets.

### Deliverables

| # | Task | Details |
|---|------|---------|
| 3.1 | JPIP 3-D request model | Extend JPIP window-of-interest requests with Z-axis parameters: `fsiz3d`, `roff3d`, `rsiz3d` for sub-volume access. |
| 3.2 | JPT/JPP-stream 3-D | Tile-part (JPT) and precinct-part (JPP) streaming extended for 3-D tile and precinct indices. |
| 3.3 | Cache model extension | Server-side cache model tracks delivered 3-D precincts/tiles per client session. |
| 3.4 | Server (`opj_jp3d_server`) | FastCGI-based JPIP server handling JP3D datasets. Built on the existing `opj_server` architecture from OpenJPIP. |
| 3.5 | Client library | C client library `libopenjpip3d` providing functions to open sessions, request volumetric regions, and receive decoded sub-volumes. |
| 3.6 | Metadata embedding | Support XML metadata boxes (medical DICOM tags, geospatial coordinates) in JPIP responses. |

### Exit Criteria

- Client can request and receive arbitrary sub-volumes from a JP3D dataset served over HTTP.
- Server correctly manages multi-client sessions with independent cache models.
- Integration tests with synthetic and real-world volumetric datasets.

---

## Phase 4 — SIMD Optimisation ✅ Complete

**Goal:** Accelerate critical paths using SIMD intrinsics on all target architectures.

### Deliverables

| # | Task | Details |
|---|------|---------|
| 4.1 | Profiling baseline | Profile Phase 1–3 code to identify hotspots (DWT, entropy coding, colour transform, memory copy). |
| 4.2 | x86-64 — SSE4.1 / AVX2 | Vectorised 3-D DWT lifting steps, context formation, and MagSgn coding for x86-64. Runtime dispatch via CPUID. |
| 4.3 | AArch64 — NEON | Equivalent NEON implementations. Validated on Raspberry Pi 4/5 and Ampere Altra. |
| 4.4 | Apple A/M series | NEON paths verified on Apple Silicon (M1–M4). Explore Apple AMX for large matrix operations in multi-component transforms. Compile with `-march=armv8.4-a+crypto` or newer as appropriate. |
| 4.5 | Runtime dispatch | `opj_cpu_features()` utility detecting ISA extensions at runtime. Automatic selection of best available code path. |
| 4.6 | Multi-threading | Optional OpenMP or pthreads parallelism for tile-level encoding/decoding (following OpenJPEG's existing threading model). |
| 4.7 | Benchmarks | Reproducible benchmark suite (`tools/benchmark/`) comparing: scalar vs. SIMD, single-thread vs. multi-thread, EBCOT vs. HTJ2K, across all architectures. |

### Exit Criteria

- ≥ 2× DWT speedup and ≥ 3× HTJ2K Tier-1 speedup on SIMD-capable hardware vs. scalar baseline.
- All SIMD paths produce bit-identical output to scalar reference implementation.
- CI runs SIMD tests on x86-64 and AArch64 (via cross-compilation or QEMU).

---

## Phase 5 — Command-Line Tools ✅ Complete

**Goal:** Provide CLI tools for encoding, decoding, inspection, and benchmarking of JP3D volumes.

### Deliverables

| # | Task | Details |
|---|------|---------|
| 5.1 | `opj_jp3d_compress` | Encode raw/TIFF volumes to JP3D codestream. Options: tile size, decomposition levels, bit-rate, lossless/lossy, HTJ2K mode, number of threads. |
| 5.2 | `opj_jp3d_decompress` | Decode JP3D codestream to raw/TIFF. Options: sub-volume extraction, reduced resolution, single-slice output. |
| 5.3 | `opj_jp3d_dump` | Inspect and print JP3D codestream structure (markers, tile info, packet lengths). Similar to OpenJPEG's `opj_dump`. |
| 5.4 | `opj_jp3d_transcode` | Transcode between EBCOT and HTJ2K block coding within a JP3D codestream without full decode. |
| 5.5 | `opj_jpip3d_server` | Standalone JPIP server for JP3D datasets (wraps Phase 3 library). |
| 5.6 | Man pages | Troff man pages for each tool, installed to `share/man/man1/`. |

### Exit Criteria

- All tools build and run on Linux, macOS, and Windows.
- `--help` output and man pages are complete and consistent.
- End-to-end CLI tests (encode → decode → diff) pass on CI.

---

## Phase 6 — Documentation & Examples ✅ Complete

**Goal:** Comprehensive documentation and code examples, consistent with OpenJPEG style.

### Deliverables

| # | Task | Details |
|---|------|---------|
| 6.1 | API reference | Full Doxygen documentation for `openjp3d.h`, `openjpip3d.h`, all public types and functions. |
| 6.2 | User guide | Markdown guide in `doc/`: building from source, basic usage, advanced options, architecture overview. |
| 6.3 | Code examples | `doc/examples/` directory with self-contained C programs: `encode_volume.c`, `decode_volume.c`, `partial_decode.c`, `jpip3d_client.c`, `htj2k_encode.c`. |
| 6.4 | Architecture document | `doc/architecture.md` describing internal module structure, data flow, and extension points. |
| 6.5 | Migration guide | `doc/migration-from-legacy.md` for users of the deprecated OpenJPEG ≤ 2.4.0 JP3D code. |
| 6.6 | CONTRIBUTING guide | `CONTRIBUTING.md` with build instructions, test procedures, coding conventions, and PR workflow. |

### Exit Criteria

- Doxygen builds without warnings.
- All example programs compile and run successfully.
- Documentation reviewed for accuracy and completeness.

---

## Phase 7 — Integration Testing & Release Preparation ✅ Complete

**Goal:** Validate the full system, ensure upstream compatibility, and prepare for release.

### Deliverables

| # | Task | Details |
|---|------|---------|
| 7.1 | Conformance testing | Validate against JP3D conformance codestreams (if available from ISO/ITU-T test suite). |
| 7.2 | Real-world datasets | Test with medical imaging data (CT/MRI volumes from public datasets such as [The Cancer Imaging Archive](https://www.cancerimagingarchive.net/)), satellite/geospatial stacks, and microscopy data. |
| 7.3 | Upstream compatibility | Verify that the OpenJP3D source tree can be placed alongside OpenJPEG 2.5.4 `src/` without build conflicts. Test merging with a hypothetical OpenJPEG 2.6.x release. |
| 7.4 | Security review | Fuzz testing with AFL++ / libFuzzer on decoder paths. Address all sanitizer warnings (ASan, UBSan, MSan). |
| 7.5 | Performance report | Publish benchmark results across architectures, compression modes, and dataset types. |
| 7.6 | Release packaging | Source tarball, CMake install targets (`make install`), pkg-config `.pc` file, versioned shared library (`libopenjp3d.so.1.0.0`). |
| 7.7 | Tagging & changelog | Semantic versioning (v1.0.0). Complete `CHANGELOG.md` entries for all phases. |

### Exit Criteria

- All tests pass on Linux, macOS, Windows across x86-64 and AArch64.
- No critical or high-severity security issues from fuzzing.
- Release artefacts install correctly and downstream test programs link and run.

---

## Phase 8 — Interactive GUI Test Application ✅ Complete

### 8A — Application Framework & UI Shell ✅ Complete

| # | Task | Details |
|---|------|---------|
| 8A.1 | GUI toolkit selection | Evaluate and select a cross-platform C/C++ GUI toolkit (e.g., Qt 6, Dear ImGui + SDL2/GLFW, GTK 4, or wxWidgets). Prefer a lightweight option that keeps the dependency footprint small. Document the choice and rationale in `doc/gui-toolkit-rationale.md`. |
| 8A.2 | Application scaffold | Create the GUI application target `opj_jp3d_gui` under `src/bin/jp3d/gui/`. Add a `BUILD_GUI_TOOLS` CMake option (default OFF). Wire into the root `CMakeLists.txt`. |
| 8A.3 | Main window layout | Implement the main window with: menu bar, toolbar, file-browser panel, volume-info panel, slice/volume viewport, and a log/console panel. Support resizable and dockable panels. |
| 8A.4 | Theme & accessibility | Light and dark themes. Ensure sufficient contrast and keyboard-navigable controls for accessibility compliance. |

### 8B — Volume Loading & Visualisation ✅ Complete

| # | Task | Details |
|---|------|---------|
| 8B.1 | File open dialog | Open raw volumes (`.raw`, `.vol`), multi-page TIFF stacks, and JP3D codestreams (`.jp3d`, `.j3d`). Auto-detect format or prompt for parameters (dimensions, bit-depth, byte-order) for raw files. |
| 8B.2 | Slice viewer | 2-D slice viewer for axial, sagittal, and coronal planes. Slider or scroll-wheel navigation along the selected axis. Support window/level (brightness/contrast) adjustment. |
| 8B.3 | 3-D volume rendering | Optional 3-D volume rendering viewport using GPU-accelerated ray-casting (OpenGL 3.3+ or Vulkan). Configurable transfer function and opacity mapping. |
| 8B.4 | Metadata display | Display volume metadata (dimensions, bit-depth, number of components, tile grid, decomposition levels, compression mode) in the info panel. |
| 8B.5 | Histogram & statistics | Compute and display voxel intensity histogram, min/max/mean/std-dev statistics for the loaded volume or a user-selected region of interest. |

### 8C — Encoding & Decoding Controls ✅ Complete

| # | Task | Details |
|---|------|---------|
| 8C.1 | Encode panel | GUI controls for all encoder parameters: tile size (X/Y/Z), decomposition levels, code-block size, target bit-rate, lossless vs. lossy, HTJ2K mode toggle, number of threads. "Encode" button triggers encoding and displays progress. |
| 8C.2 | Decode panel | GUI controls for decoder options: sub-volume extraction (offset + size), reduced resolution level, single-slice mode. "Decode" button triggers decoding and loads the result into the viewer. |
| 8C.3 | Transcode panel | Interface for EBCOT ↔ HTJ2K transcoding. Select input codestream, choose target mode, and execute. |
| 8C.4 | Progress & cancellation | Progress bar with percentage and elapsed time for encode/decode/transcode operations. Support cancellation of long-running operations. |

### 8D — Round-Trip Testing & Validation ✅ Complete

| # | Task | Details |
|---|------|---------|
| 8D.1 | Round-trip test wizard | One-click round-trip test: encode → decode → compare. Reports pass/fail for lossless (bit-exact) and displays PSNR/MSE for lossy. |
| 8D.2 | Diff viewer | Side-by-side or overlay viewer comparing original and decoded volumes. Highlight differing voxels with a configurable threshold. Error-map visualisation (absolute difference per voxel). |
| 8D.3 | Batch test runner | Queue multiple encode/decode/round-trip jobs with varying parameters. Display results in a table (parameter set, pass/fail, PSNR, compression ratio, encode time, decode time). Export results to CSV. |
| 8D.4 | Codestream inspector | Visual representation of the JP3D codestream structure (markers, tile parts, packets) similar to `opj_jp3d_dump` output but presented in a tree-view widget. |

### 8E — JPIP 3-D Streaming Client ✅ Complete

| # | Task | Details |
|---|------|---------|
| 8E.1 | JPIP connection dialog | Connect to a JPIP server by URL. Display available datasets and session info. |
| 8E.2 | Interactive sub-volume browsing | Navigate volumetric data served via JPIP. Request and display sub-volumes interactively. Show cache/download progress per region. |
| 8E.3 | Network diagnostics | Display JPIP session statistics: bytes transferred, cache hit ratio, request/response latency, number of active sessions. |

### 8F — Logging, Preferences & Platform Support ✅ Complete

| # | Task | Details |
|---|------|---------|
| 8F.1 | Log console | Embedded log panel capturing all `opj_event_mgr_t` callbacks (errors, warnings, info). Support filtering by severity, copy-to-clipboard, and export to file. |
| 8F.2 | Preferences dialog | Persistent settings: default file paths, encoding presets, viewer defaults (interpolation, background colour), theme selection, thread count. Stored in a platform-appropriate config file (e.g., `~/.config/openjp3d/gui.ini`). |
| 8F.3 | Cross-platform packaging | Build and package the GUI application for Linux (AppImage or Flatpak), macOS (`.app` bundle), and Windows (NSIS/WiX installer or portable `.zip`). |
| 8F.4 | Keyboard shortcuts | Configurable keyboard shortcuts for common actions (open, encode, decode, next/previous slice, zoom, pan). |

### Exit Criteria

- GUI application builds and runs on Linux, macOS, and Windows.
- All codec operations (encode, decode, transcode) are accessible via the GUI with parameter controls.
- Round-trip validation (lossless bit-exact, lossy PSNR) passes through the GUI.
- Slice viewer correctly displays volumes across all supported bit-depths and component counts.
- JPIP client can connect to `opj_jpip3d_server` and browse volumetric data interactively.
- Application is stable under normal use — no crashes or memory leaks detected by sanitizers.

---

## Phase 10 — Julia Bindings & Scientific Computing Integration ✅ Complete

**Goal:** Expose the OpenJP3D codec to the Julia scientific computing ecosystem
via pure-`ccall`-based bindings, enabling encoding and decoding of 3-D volumes
directly from native Julia arrays without requiring any compilation step.

### Deliverables

| # | Task | Details |
|---|------|---------|
| 10.1 | Julia package scaffold | `julia/OpenJP3D.jl/` package with `Project.toml`.  Installable via `Pkg.develop(path="julia/OpenJP3D.jl")`.  BSD-2-Clause licence header on all Julia files.  Requires Julia ≥ 1.6 and no C compilation. |
| 10.2 | Shared-library loader (`lib_path()`) | Cross-platform loader: checks `OPENJP3D_LIBRARY` env var, then the package `lib/` subdirectory, then `Libdl.find_library()`.  Works on Linux (`.so`), macOS (`.dylib`), and Windows (`.dll`).  Cached: library path resolved once per process. |
| 10.3 | ccall bindings (C structs) | Julia `struct` types mirroring all public C structs (`VolumeComp`, `Volume`, `EncParamsC`, `DecParamsC`).  Layout verified against `openjp3d.h`.  Mutable structs for pass-by-reference (`EncParamsC`, `DecParamsC`). |
| 10.4 | High-level Julia API (`OpenJP3D.jl`) | `encode(volume [, params]; on_message)` — accepts `(D,H,W)` or `(D,H,W,C)` Julia arrays; returns `Vector{UInt8}`.  `decode(data; verbose, on_message)` — accepts `Vector{UInt8}`; returns Julia array with correct element type.  `transcode_to_ht(data; params, on_message)` — wraps `opj_jp3d_transcode_to_ht()`.  `get_version()` — returns native library version.  `EncodeParams` keyword-argument struct mirrors `opj_jp3d_enc_params_t`.  Data-layout helpers handle Julia column-major ↔ C row-major conversion via `permutedims`. |
| 10.5 | CMake integration (`julia/CMakeLists.txt`) | `BUILD_JULIA_BINDINGS` CMake option (default OFF).  When ON, registers a `test_julia_bindings` CTest target that runs `julia --project=julia/OpenJP3D.jl test/runtests.jl` with `OPENJP3D_LIBRARY` set automatically.  Checks for Julia ≥ 1.6 and `BUILD_SHARED_LIBS=ON`; skips gracefully otherwise. |
| 10.6 | Julia test suite (`julia/OpenJP3D.jl/test/runtests.jl`) | 100+ test cases covering: version string format, all exported constants, `EncodeParams` construction and `_to_c()`, lossless round-trips for all five supported element types (`UInt8`, `Int8`, `UInt16`, `Int16`, `Int32`), multi-component 3- and 4-channel volumes, single-slice edge case, non-square dimensions, large volumes, tiled encoding, SOC marker check, HTJ2K round-trip, lossy 9/7 encoding, `transcode_to_ht` with and without params, data layout correctness (corner voxels, gradient pattern), message callback, and error handling (invalid codestream, 2-D/5-D input, `Float32`/`Float64` dtype).  Tests auto-skip when the shared library is not available. |
| 10.7 | Package README (`julia/OpenJP3D.jl/README.md`) | Installation instructions, library search-order documentation, quick-start examples (lossless, lossy, HTJ2K, transcode, callback), and full API reference table. |

### Exit Criteria

- `Pkg.develop(path="julia/OpenJP3D.jl")` succeeds without compilation.
- `julia --project=julia/OpenJP3D.jl julia/OpenJP3D.jl/test/runtests.jl` passes with the shared library built.
- `OpenJP3D.encode()` / `OpenJP3D.decode()` produce lossless round-trips for all supported element types.
- `OpenJP3D.transcode_to_ht()` produces a decodable codestream.
- Data layout (Julia column-major ↔ C row-major) is correctly handled; spatial voxel order is preserved.
- Package installs cleanly on Linux, macOS, and Windows.

---

## Phase 9 — Python Bindings & NumPy Integration ✅ Complete

**Goal:** Expose the OpenJP3D codec to the scientific Python ecosystem via pure-Python
ctypes-based bindings, enabling encoding and decoding of 3-D volumes directly from NumPy
arrays without requiring a compiled Python extension.

### Deliverables

| # | Task | Details |
|---|------|---------|
| 9.1 | Python package scaffold | `python/openjp3d/` package with `pyproject.toml` and `setup.py`. Installable via `pip install -e .`. BSD-2-Clause licence header on all Python files. |
| 9.2 | Shared-library loader (`_lib.py`) | Cross-platform loader: checks `OPENJP3D_LIBRARY` env var, then the package directory, then `ctypes.util.find_library()`. Works on Linux (`.so`), macOS (`.dylib`), and Windows (`.dll`). Singleton: loads once per process. |
| 9.3 | ctypes bindings (`_bindings.py`) | Mirrors all public C structures (`opj_volume_comp_t`, `opj_volume_t`, `opj_jp3d_enc_params_t`, `opj_jp3d_dec_params_t`) as `ctypes.Structure` subclasses. Annotates all public API functions with `argtypes` / `restype`. Defines `MsgCallbackType` for the message callback. |
| 9.4 | High-level NumPy API (`__init__.py`) | `encode(volume, params, *, on_message)` — accepts `(D,H,W)` or `(D,H,W,C)` NumPy arrays; returns JP3D bytes. `decode(data, *, verbose, on_message)` — accepts bytes; returns NumPy array with correct dtype. `transcode_to_ht(data, params)` — wraps `opj_jp3d_transcode_to_ht()`. `get_version()` — returns native library version. `EncodeParams` dataclass mirrors `opj_jp3d_enc_params_t`. |
| 9.5 | CMake integration (`python/CMakeLists.txt`) | `BUILD_PYTHON_BINDINGS` CMake option (default OFF). When ON, registers a `test_python_bindings` CTest target that runs `pytest tests/test_python.py` with `OPENJP3D_LIBRARY` and `PYTHONPATH` set automatically. Requires `BUILD_SHARED_LIBS=ON`. |
| 9.6 | Python test suite (`tests/test_python.py`) | 40+ pytest test cases covering: version string, constants, `EncodeParams` construction and `_to_c()`, lossless round-trips for all supported dtypes (`uint8`, `int8`, `uint16`, `int16`, `int32`), multi-component volumes, single-slice edge case, non-square dimensions, SOC marker check, HTJ2K round-trip, lossy encoding, tiled encoding, message callback, `transcode_to_ht`, and error handling (invalid data, wrong ndim, unsupported dtype). |
| 9.7 | Package README (`python/README.md`) | Installation instructions, quick-start examples (lossless, lossy, HTJ2K, transcode), library search-order documentation. |

### Exit Criteria

- `pip install -e python/` succeeds without compilation.
- `pytest tests/test_python.py` passes with the shared library built (`BUILD_SHARED_LIBS=ON`).
- `openjp3d.encode()` / `openjp3d.decode()` produce lossless round-trips for all supported dtypes.
- `openjp3d.transcode_to_ht()` produces a decodable codestream.
- No memory leaks (library-side allocations freed via `opj_jp3d_free()`).
- Package installs cleanly on Linux, macOS, and Windows.

---

## Phase 11 — R Bindings & Scientific Computing Integration ✅ Complete

**Goal:** Expose the OpenJP3D codec to the R statistical computing ecosystem
via bindings that use a thin C wrapper to dynamically load the pre-built
shared library, enabling encoding and decoding of 3-D volumes directly from
R integer arrays.

### Deliverables

| # | Task | Details |
|---|------|---------|
| 11.1 | R package scaffold | `r/openjp3d/` package with `DESCRIPTION`, `NAMESPACE`, and `LICENSE`. Installable via `R CMD INSTALL r/openjp3d` or `devtools::install("r/openjp3d")`. BSD-2-Clause licence header on all R and C source files. |
| 11.2 | Thin C wrapper (`src/openjp3d_wrap.c`) | Cross-platform loader using `dlopen`/`LoadLibrary` to open the openjp3d shared library at runtime (not at compile time). Search order: `OPENJP3D_LIBRARY` env var → `lib/` next to the installed package → OS default loader. Registers `.Call()` entry points via `R_init_openjp3d`. |
| 11.3 | High-level R API (`R/openjp3d.R`) | `encode(volume, params, prec, sgnd, on_message)` — accepts `(D,H,W)` or `(D,H,W,C)` integer arrays; returns `raw` JP3D codestream. `decode(data, verbose, on_message)` — accepts `raw` vector; returns integer array with `dtype` attribute. `transcode_to_ht(data, params, on_message)` — wraps `opj_jp3d_transcode_to_ht()`. `get_version()` — returns native library version. `EncodeParams(...)` S3 constructor. Data-layout helpers convert R column-major ↔ C row-major via `aperm()`. |
| 11.4 | Package init (`R/zzz.R`) | `.onLoad()` resolves the shared library at package load time and calls `ojp3d_load_lib()`. |
| 11.5 | CMake integration (`r/CMakeLists.txt`) | `BUILD_R_BINDINGS` CMake option (default OFF). When ON, registers a `test_r_bindings` CTest target that installs the package into a build-tree library directory and runs `tests/test_r.R` with `OPENJP3D_LIBRARY` set automatically. Requires `BUILD_SHARED_LIBS=ON`; skips gracefully when R is not found. |
| 11.6 | R test suite (`tests/test_r.R`) | 43 test cases (base R, no external test framework required) covering: version string format, all exported constants, `EncodeParams` construction and `_to_c()`, lossless round-trips for all five supported precisions (8/16/32-bit signed and unsigned), multi-component (3- and 4-channel) volumes, single-slice edge case, non-square dimensions, large volumes (16×16×16), tiled encoding, SOC marker prefix check, HTJ2K round-trip, lossy 9/7 encoding, `transcode_to_ht` with and without params, data layout correctness (corner voxels, gradient pattern), message callback, `dtype` attribute on decoded arrays, and error handling (invalid codestream, 2-D input, non-raw data). |
| 11.7 | Package README (`r/openjp3d/README.md`) | Installation instructions, library search-order documentation, quick-start examples (lossless, lossy, HTJ2K, transcode, multi-component, callback), and full API reference table. |

### Exit Criteria

- `R CMD INSTALL r/openjp3d` succeeds on Linux, macOS, and Windows.
- `Rscript tests/test_r.R` passes with the shared library built (`BUILD_SHARED_LIBS=ON`).
- `openjp3d::encode()` / `openjp3d::decode()` produce lossless round-trips for all supported precisions.
- `openjp3d::transcode_to_ht()` produces a decodable codestream.
- No memory leaks (library-side allocations freed via `opj_jp3d_free()`).
- Package installs cleanly on Linux, macOS, and Windows.

---

## Phase 12 — MATLAB/Octave Bindings ✅ Complete

**Goal:** Provide MATLAB and Octave bindings for the OpenJP3D codec, following the same
runtime-loading pattern as the R bindings (Phase 11).

### Deliverables

| # | Task | Details |
|---|------|---------|
| 12.1 | MEX C wrapper | `matlab/openjp3d/src/openjp3d_mex.c`: single `mexFunction` entry point dispatching on a command string. Runtime `dlopen`/`LoadLibrary` loading; no link-time dependency on the shared library. Five commands: `load_lib`, `get_version`, `encode`, `decode`, `transcode`. Optional MATLAB function-handle callback via `mexCallMATLAB`. |
| 12.2 | `EncodeParams` class | `matlab/openjp3d/EncodeParams.m`: MATLAB `classdef` mirroring the C encoder parameter struct with sensible defaults and a `to_c()` method. |
| 12.3 | High-level API package | `matlab/openjp3d/+openjp3d/` namespace package: `encode`, `decode`, `transcode_to_ht`, `get_version`, `load_lib`. Handles column-major ↔ row-major permutation, class inference, and name-value option parsing. |
| 12.4 | CMake integration | `matlab/CMakeLists.txt`: `BUILD_MATLAB_BINDINGS` option; CTest targets `build_matlab_mex` (via `mkoctfile --mex`) and `test_matlab_bindings` (via `octave --no-gui`). Falls back to MATLAB `mex` if Octave is not found. |
| 12.5 | Test suite | `tests/test_matlab.m`: ≥ 40 tests covering version, constants, `EncodeParams`, lossless round-trips for all precisions, multi-component volumes, single-slice edge case, non-square dimensions, tiled encoding, HTJ2K, lossy encoding, transcode, data layout, callbacks, dtype inference, and error handling. Skips gracefully when the shared library is unavailable. |
| 12.6 | Documentation | `matlab/openjp3d/README.md`: installation, library search order, quick-start examples, and full API reference. |

### Exit Criteria

- `mkoctfile --mex matlab/openjp3d/src/openjp3d_mex.c` succeeds on Linux, macOS, and Windows.
- `octave --no-gui tests/test_matlab.m` passes with the shared library available via `OPENJP3D_LIBRARY`.
- `openjp3d.encode()` / `openjp3d.decode()` produce lossless round-trips for all supported precisions.
- `openjp3d.transcode_to_ht()` produces a decodable codestream.
- No memory leaks (all MEX-allocated arrays destroyed; library allocations freed via `opj_jp3d_free()`).
- Works with both Octave ≥ 6.0 and MATLAB R2019b+.

---

## Phase 13 — Go Bindings ✅ Complete

**Goal:** Provide Go bindings for the OpenJP3D codec using CGo with runtime
`dlopen`/`LoadLibrary` loading, following the same pattern as the R and MATLAB
bindings (Phases 11–12).

### Deliverables

| # | Task | Details |
|---|------|---------|
| 13.1 | Go package scaffold | `go/openjp3d/` package with `go.mod` declaring module `github.com/raster-lab/openjp3d`. BSD-2-Clause licence header on all Go and C source files. |
| 13.2 | CGo runtime loader (`openjp3d.go`) | CGo preamble embeds platform-specific dynamic-loading shims (`dlopen`/`LoadLibrary`) and C struct mirrors for `opj_volume_t`, `opj_jp3d_enc_params_t`, and `opj_jp3d_dec_params_t`. All C function pointers are resolved at runtime; no link-time dependency on the shared library. |
| 13.3 | Callback bridge (`callback.go`) | `//export ojp3d_go_callback_bridge` Go function receives C-side messages and routes them to the current Go `MsgCallback`. Uses a mutex-protected global slot consistent with CGo restrictions. |
| 13.4 | High-level Go API | `LoadLib(path string) error` — auto-search or explicit path; idempotent. `IsLoaded() bool`. `GetVersion() (string, error)`. `Encode(samples []int32, w, h, d, numComps, prec uint32, signed bool, params *EncodeParams, cb MsgCallback) ([]byte, error)`. `Decode(data []byte, params *DecodeParams, cb MsgCallback) ([]int32, VolumeInfo, error)`. `TranscodeToHT(src []byte, params *EncodeParams, cb MsgCallback) ([]byte, error)`. `DefaultEncodeParams() EncodeParams`. |
| 13.5 | CMake integration (`go/CMakeLists.txt`) | `BUILD_GO_BINDINGS` CMake option (default OFF). Verifies Go ≥ 1.21 and `BUILD_SHARED_LIBS=ON`; registers `test_go_bindings` CTest target running `go test -v ./...` with `OPENJP3D_LIBRARY` set automatically. |
| 13.6 | Go test suite (`go/openjp3d/openjp3d_test.go`) | 32 test cases covering: library loading (idempotent), version string format, all exported constants, `EncodeParams` defaults and manual fields, lossless round-trips for all five supported precisions (uint8, int8, uint16, int16, int32), multi-component (3- and 4-channel) volumes, single-slice edge case, non-square dimensions, large (16×16×16) volume, tiled encoding, HTJ2K lossless, lossy 9/7 encoding, `TranscodeToHT` with and without params, SOC marker check, data-layout correctness, message callback, `VolumeInfo` fields, error handling (empty and invalid codestreams, zero dimensions, too-few samples, empty transcode source), and all-zeros / all-max volumes. Tests skip gracefully when the shared library is not available. |
| 13.7 | Documentation (`go/openjp3d/README.md`) | Installation instructions, library search-order documentation, quick-start examples (lossless, lossy, HTJ2K, transcode, multi-component, callback), and full API reference table. |

### Exit Criteria

- `go build ./...` succeeds on Linux, macOS, and Windows.
- `OPENJP3D_LIBRARY=... go test -v ./...` passes with the shared library built (`BUILD_SHARED_LIBS=ON`).
- `openjp3d.Encode()` / `openjp3d.Decode()` produce lossless round-trips for all supported precisions.
- `openjp3d.TranscodeToHT()` produces a decodable codestream.
- No memory leaks (C-allocated buffers freed via `opj_jp3d_free()`; decoded sample arrays freed via `free()`).

---

## Dependency & Risk Summary

| Risk | Mitigation |
|------|------------|
| Legacy JP3D code is too outdated to reuse | Phase 0.7 audit determines feasibility early; fresh implementation is the fallback. |
| HTJ2K 3-D extension is non-trivial | Phase 2 scoped as optional; core JP3D codec is functional without it. |
| JPIP 3-D specification ambiguity | Design modelled on existing OpenJPIP architecture; consult ISO/IEC 15444-9:2023 text. |
| SIMD maintenance burden | Single scalar reference + SIMD variants with shared test harness ensures correctness. |
| Upstream OpenJPEG API changes | Mirror directory layout; integration tested each phase; minimal coupling to internal OpenJPEG symbols. |
| GUI toolkit dependency size | Select a lightweight toolkit (e.g., Dear ImGui + SDL2); `BUILD_GUI_TOOLS` is OFF by default so the core library remains dependency-free. |
| Cross-platform GPU rendering | Require only OpenGL 3.3 (widely supported); provide a software-fallback 2-D slice viewer when GPU rendering is unavailable. |
| Python bindings ABI mismatch | ctypes bindings are validated against the exact C structure layout at test time; `BUILD_SHARED_LIBS=ON` required; `OPENJP3D_LIBRARY` env var allows flexible library location. |

---

## Timeline Overview

| Phase | Description | Estimated Duration |
|-------|-------------|-------------------|
| 0 | Project Bootstrapping | 2 weeks |
| 1 | Core JP3D Codec | 8–10 weeks |
| 2 | HTJ2K Integration | 4–6 weeks |
| 3 | JPIP Extension | 4–6 weeks |
| 4 | SIMD Optimisation | 4–6 weeks |
| 5 | Command-Line Tools | 2–3 weeks |
| 6 | Documentation & Examples | 2–3 weeks |
| 7 | Integration Testing & Release | 3–4 weeks |
| 8 | Interactive GUI Test Application | 6–8 weeks |
| 9 | Python Bindings & NumPy Integration | 1–2 weeks |
| 10 | Julia Bindings & Scientific Computing Integration | 1–2 weeks |
| 11 | R Bindings & Scientific Computing Integration | 1–2 weeks |
| 12 | MATLAB/Octave Bindings | 1–2 weeks |
| 13 | Go Bindings | 1–2 weeks |
| | **Total (sequential)** | **~40–58 weeks** |

> Phases 2, 3, and 4 can be partially parallelised after Phase 1 is complete,
> potentially reducing total wall-clock time by 6–10 weeks.

---

## References

- ISO/IEC 15444-1 — JPEG 2000 Part 1: Core coding system
- ISO/IEC 15444-2 — JPEG 2000 Part 2: Extensions
- ISO/IEC 15444-9 — JPEG 2000 Part 9: JPIP (Interactive Protocol)
- ISO/IEC 15444-10 — JPEG 2000 Part 10: Extensions for 3-D data (JP3D)
- ISO/IEC 15444-15 — JPEG 2000 Part 15: High-Throughput JPEG 2000 (HTJ2K)
- [OpenJPEG — GitHub](https://github.com/uclouvain/openjpeg)
- [OpenJPH — GitHub](https://github.com/aous72/OpenJPH)
- [OpenJPEG 2.5.0 Release Notes (JP3D removal)](https://www.openjpeg.org/2022/05/13/OpenJPEG-2.5.0-released)
