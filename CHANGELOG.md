# Changelog

All notable changes to the OpenJP3D project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
