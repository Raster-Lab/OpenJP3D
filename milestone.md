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

## Phase 0 — Project Bootstrapping

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

## Phase 1 — Core JP3D Codec (Lossless & Lossy)

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

## Phase 2 — HTJ2K Integration (Part 15 in JP3D)

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

## Phase 3 — JPIP Extension for JP3D (Part 9)

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

## Phase 4 — SIMD Optimisation

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

## Phase 5 — Command-Line Tools

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

## Phase 6 — Documentation & Examples

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

## Phase 7 — Integration Testing & Release Preparation

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

## Dependency & Risk Summary

| Risk | Mitigation |
|------|------------|
| Legacy JP3D code is too outdated to reuse | Phase 0.7 audit determines feasibility early; fresh implementation is the fallback. |
| HTJ2K 3-D extension is non-trivial | Phase 2 scoped as optional; core JP3D codec is functional without it. |
| JPIP 3-D specification ambiguity | Design modelled on existing OpenJPIP architecture; consult ISO/IEC 15444-9:2023 text. |
| SIMD maintenance burden | Single scalar reference + SIMD variants with shared test harness ensures correctness. |
| Upstream OpenJPEG API changes | Mirror directory layout; integration tested each phase; minimal coupling to internal OpenJPEG symbols. |

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
| | **Total (sequential)** | **~29–40 weeks** |

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
