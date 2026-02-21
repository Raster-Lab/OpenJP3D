# Copilot Instructions — OpenJP3D

## Project Overview

OpenJP3D is a C library implementing JPEG 2000 Part 10 (JP3D) volumetric
compression (ISO/IEC 15444-10), extending the OpenJPEG 2.5.4 codebase. It
supports lossless and lossy compression of 3-D image volumes, optional Part 15
HTJ2K high-throughput block coding, and Part 9 JPIP interactive streaming for
volumetric data. Target applications include medical imaging (CT/MRI),
geospatial/satellite imagery, and microscopy.

## Language & Standard

- Write all library and tool code in **C11**.
- Use only standard C11 features; avoid compiler-specific extensions unless
  wrapped behind a portability macro.

## Naming Conventions

- Prefix all public symbols with **`opj_`** to match OpenJPEG conventions.
- Use the prefixes `opj_jp3d_`, `opj_jpip3d_`, or `opj_htj2k3d_` for
  module-specific identifiers where additional disambiguation is needed.
- Use **snake_case** for functions, variables, and type names
  (e.g., `opj_volume_t`, `opj_jp3d_encode`).
- Use **UPPER_SNAKE_CASE** for macros and constants
  (e.g., `OPJ_JP3D_USE_HTJ2K`).

## Code Style

- Indent with **4 spaces** (no tabs).
- Follow the project `clang-format` configuration when one is present.
- Run `clang-tidy` to catch common issues.
- Keep functions short and focused; prefer helper functions over deeply nested
  logic.

## Documentation

- Use **Doxygen**-style comments (`/** ... */`) for all public types, functions,
  and macros.
- Every public header (`openjp3d.h`, `openjpip3d.h`) must have complete Doxygen
  annotations so the generated reference builds without warnings.
- Write self-contained code examples in `doc/examples/` (e.g.,
  `encode_volume.c`, `decode_volume.c`).

## Directory Structure

Mirror the OpenJPEG source layout to ease future upstream merging:

```
src/lib/openjp3d/    — Core JP3D codec library
src/lib/openjpip3d/  — JPIP 3-D streaming library
src/bin/jp3d/        — CLI tools (compress, decompress, dump, transcode)
tests/               — CTest-based test suites
doc/                 — Doxygen config, user guide, architecture docs, examples
thirdparty/          — Vendored or FetchContent-managed dependencies
cmake/               — CMake helper modules
```

When adding new files, place them in the directory that matches the above layout.

## Build System

- Use **CMake** as the sole build system.
- The root `CMakeLists.txt` exposes these options:
  - `BUILD_JP3D` — Core JP3D codec (default ON)
  - `BUILD_HTJ2K_3D` — HTJ2K block coder (optional)
  - `BUILD_JPIP_3D` — JPIP volumetric streaming (optional)
  - `BUILD_CLI_TOOLS` — Command-line utilities
  - `BUILD_DOC` — Doxygen documentation
  - `BUILD_TESTING` — CTest test suite
- Support **Debug**, **Release**, and **RelWithDebInfo** configurations.
- Use `FetchContent` or vendored copies in `thirdparty/` for external
  dependencies (e.g., libtiff, libpng); keep the layout consistent with
  upstream OpenJPEG.

## API Design

- **Encoder API**: `opj_jp3d_encode()` accepts an `opj_volume_t`, compression
  parameters, and an output stream.
- **Decoder API**: `opj_jp3d_decode()` accepts a codestream and returns an
  `opj_volume_t`; support partial decoding (sub-volume, single slice, reduced
  resolution).
- Provide default-parameter helpers:
  `opj_jp3d_set_default_encoder_parameters()`,
  `opj_jp3d_set_default_decoder_parameters()`.
- **Memory management**: use pluggable allocators (`opj_malloc`, `opj_free`,
  `opj_aligned_malloc`) consistent with OpenJPEG.
- **Error handling**: callback-based error/warning/info logging via
  `opj_event_mgr_t`.

## SIMD & Performance

- Maintain a **scalar reference implementation** for every computation.
- Provide optional SIMD-optimised paths for:
  - **x86-64**: SSE4.1 / AVX2
  - **AArch64**: NEON
  - **Apple A/M-series**: NEON (+ AMX where beneficial)
- Use **runtime CPU feature detection** (`opj_cpu_features()` / CPUID) and
  automatic dispatch; never require a specific ISA at compile time.
- SIMD paths **must produce bit-identical output** to the scalar reference.
- Place benchmarks in `tools/benchmark/` with reproducible configurations.

## Threading

- Support optional **OpenMP or pthreads** parallelism at the tile level,
  following OpenJPEG's existing threading model.
- All shared state must be properly synchronised; avoid global mutable state.

## Testing

- Use **CTest** for all automated tests.
- Target **≥ 50 test cases** for the core codec, covering edge cases such as
  1-slice volumes, maximum tile dimensions, and all supported bit-depths.
- Every round-trip test (encode → decode) must verify **bit-exact lossless**
  output for reversible mode.
- Lossy-mode tests must verify output at target bit-rates with measurable PSNR.
- End-to-end CLI tests (encode → decode → diff) should be included for every
  command-line tool.
- Run tests under **AddressSanitizer (ASan)**, **UndefinedBehaviorSanitizer
  (UBSan)**, and **MemorySanitizer (MSan)** in CI.

## Security

- Fuzz decoder paths with **AFL++ / libFuzzer** as part of release preparation.
- Address **all sanitizer warnings** before tagging a release.
- Treat all external input (codestreams, file-format boxes, JPIP requests) as
  untrusted; validate lengths, offsets, and counts before use.

## CI / CD

- GitHub Actions workflows covering:
  - **Linux** (GCC, Clang)
  - **macOS** (Apple Clang)
  - **Windows** (MSVC)
  - Matrix builds for **x86-64** and **AArch64** (cross-compilation or QEMU)
- CI must run CTest, `clang-format` checks, `clang-tidy`, and Doxygen builds.
- SIMD tests must run on both x86-64 and AArch64 targets in CI.

## Licensing

- All source files are licensed under **BSD-2-Clause**, matching OpenJPEG.
- Include the licence header in every source and header file.
- When referencing external algorithms (e.g., OpenJPH for HTJ2K), document the
  origin and licence in the relevant source file and in `doc/`.

## Modularity

- **JP3D**, **HTJ2K-3D**, and **JPIP-3D** are independent build targets that
  can be enabled or disabled individually.
- The core JP3D codec must be fully functional without HTJ2K or JPIP.
- Keep inter-module coupling minimal; communicate through the public API.

## Upstream Compatibility

- Mirror the OpenJPEG `src/` directory layout so that future OpenJPEG releases
  can be merged with minimal conflict.
- Minimise coupling to internal OpenJPEG symbols; prefer the public API.
- Verify that the OpenJP3D source tree can coexist alongside OpenJPEG 2.5.4
  without build conflicts.

## Commit & PR Practices

- Use **semantic versioning** (e.g., v1.0.0).
- Maintain a `CHANGELOG.md` with entries for every phase and release.
- Keep PRs focused: one logical change per pull request.
- Ensure `cmake --build .` succeeds on all CI targets before merging.

## Documentation Maintenance

Whenever a phase, feature, or milestone task is completed, **all** of the
following documents must be updated before the PR is merged:

### 1. `milestone.md`

- Add a **✅ Complete** badge next to the phase heading
  (e.g., `## Phase 1 — Core JP3D Codec (Lossless & Lossy) ✅ Complete`).
- Do **not** remove or rewrite existing task tables; the plan is the historical
  record.

### 2. `CHANGELOG.md`

- Add an entry under `## [Unreleased]` → `### Added` (or the appropriate
  section: `Changed`, `Fixed`, `Removed`).
- Use the format already established: a **bold phase title** followed by
  bullet points for each deliverable, referencing the task numbers from
  `milestone.md` (e.g., `**5.1 \`opj_jp3d_compress\`**`).
- When tagging a release, move `[Unreleased]` entries into a versioned section
  (e.g., `## [1.0.0] - 2026-MM-DD`).

### 3. `README.md`

- Keep the **Features** list current — add new capabilities as they land.
- Update the **Project Status** section to name the most recently completed
  phase and the phase currently in progress.
- Add or update any **Quick-start / CLI usage** examples when new tools are
  introduced.

### 4. Other documents (as applicable)

| Document | When to update |
|----------|---------------|
| `INSTALL.md` | New build options or dependencies are added. |
| `AUTHORS.md` | New contributors join the project. |
| `CODING_STYLE.md` | Conventions change or new rules are adopted. |
| `doc/` guides & examples | API surface changes, new features need examples. |
| Man pages (`src/bin/jp3d/*.1`) | CLI flags or behaviour change. |

### Checklist template

Use this checklist in every feature-completion PR to verify documentation is
up to date:

```markdown
- [ ] `milestone.md` — phase/task marked ✅ Complete
- [ ] `CHANGELOG.md` — entry added under `[Unreleased]`
- [ ] `README.md` — Features / Project Status / examples updated
- [ ] `INSTALL.md` — updated if build options changed
- [ ] Doxygen headers — new/changed public API documented
- [ ] Man pages — updated if CLI tools changed
```
