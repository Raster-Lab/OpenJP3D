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
- **Phase 8E** — GUI JPIP 3-D Streaming Client ✅ *(in progress — Phase 8)*

**Current release: v1.0.0**

See [milestone.md](milestone.md) for the full implementation plan and
[CHANGELOG.md](CHANGELOG.md) for detailed change history.

## Licence

BSD-2-Clause — see [LICENSE](LICENSE) for details.