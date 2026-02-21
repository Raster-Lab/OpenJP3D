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

**Current release: v1.0.0**

See [milestone.md](milestone.md) for the full implementation plan and
[CHANGELOG.md](CHANGELOG.md) for detailed change history.

## Licence

BSD-2-Clause — see [LICENSE](LICENSE) for details.