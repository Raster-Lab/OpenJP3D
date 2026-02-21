# OpenJP3D — Manual Testing Guide

This document provides step-by-step manual test procedures for every user-facing
feature in the OpenJP3D project. Each test has a unique identifier, clear
prerequisites, numbered steps, and explicit pass/fail criteria.

> **Convention:** A ✅ next to a test means it is expected to pass on all
> supported platforms (Linux, macOS, Windows) unless stated otherwise.

---

## Table of Contents

1. [Prerequisites & Setup](#1-prerequisites--setup)
2. [Core Library (C API)](#2-core-library-c-api)
3. [CLI Tools](#3-cli-tools)
4. [GUI Application](#4-gui-application)
5. [JPIP 3-D Streaming](#5-jpip-3-d-streaming)
6. [Python Bindings](#6-python-bindings)
7. [Julia Bindings](#7-julia-bindings)
8. [R Bindings](#8-r-bindings)
9. [MATLAB/Octave Bindings](#9-matlaboctave-bindings)
10. [Go Bindings](#10-go-bindings)
11. [Rust Bindings](#11-rust-bindings)
12. [SIMD Optimisation](#12-simd-optimisation)
13. [Build System & Packaging](#13-build-system--packaging)

---

## 1. Prerequisites & Setup

### 1.1 Build the Project

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DBUILD_CLI_TOOLS=ON \
  -DBUILD_JPIP_3D=ON
cmake --build build
```

### 1.2 Generate a Test Volume

Many tests require a raw binary volume file. Create a small 4×4×4 8-bit
unsigned volume filled with a ramp pattern:

```bash
python3 -c "
import struct, sys
data = bytes(range(64))
sys.stdout.buffer.write(data)
" > /tmp/test_4x4x4.raw
```

For a larger 64×64×16 volume (useful for lossy / SIMD tests):

```bash
python3 -c "
import struct, sys, random; random.seed(42)
sys.stdout.buffer.write(bytes(random.getrandbits(8) for _ in range(64*64*16)))
" > /tmp/test_64x64x16.raw
```

### 1.3 Environment Variables

| Variable | Purpose |
|----------|---------|
| `OPENJP3D_LIBRARY` | Path to `libopenjp3d.so` / `.dylib` / `.dll` (used by language bindings) |
| `PATH` | Must include `build/bin` (or platform equivalent) for CLI tools |

```bash
export OPENJP3D_LIBRARY=$(pwd)/build/lib/libopenjp3d.so
export PATH=$(pwd)/build/bin:$PATH
```

---

## 2. Core Library (C API)

These tests exercise the public C API directly. Use the example programs in
`doc/examples/` or write a short C driver.

### MT-API-001 — Library Version ✅

| Field | Value |
|-------|-------|
| **Description** | Verify `opj_jp3d_get_version()` returns a valid version string. |
| **Prerequisites** | Library built. |
| **Steps** | 1. Call `opj_jp3d_get_version()`. |
| **Expected Result** | Returns a non-NULL string matching the pattern `X.Y.Z` (e.g. `1.0.0`). |
| **Pass Criteria** | String is non-empty and contains two dots separating numeric segments. |

### MT-API-002 — Volume Creation and Destruction ✅

| Field | Value |
|-------|-------|
| **Description** | Create a volume, populate it, and free it without leaks. |
| **Prerequisites** | Library built; AddressSanitizer recommended. |
| **Steps** | 1. Call `opj_jp3d_create_volume(1, 8, 0, 4, 4, 4)` (1 component, 8-bit, unsigned, 4×4×4). 2. Write ramp data into `vol->comps[0].data`. 3. Call `opj_jp3d_destroy_volume(vol)`. |
| **Expected Result** | No ASan errors, no leaks. Volume fields (`w`, `h`, `d`, `numcomps`, `prec`, `sgnd`) match creation arguments. |
| **Pass Criteria** | No crash, no memory errors. |

### MT-API-003 — Lossless Round-Trip (5/3 Filter) ✅

| Field | Value |
|-------|-------|
| **Description** | Encode and decode a volume losslessly; output must be bit-exact. |
| **Prerequisites** | A 4×4×4 8-bit raw volume. |
| **Steps** | 1. Create volume from raw data. 2. Set default encoder parameters (`opj_jp3d_set_default_encoder_parameters`). 3. Leave rate = 0 and filter = `OPJ_JP3D_DWT53`. 4. Call `opj_jp3d_encode()`. 5. Call `opj_jp3d_decode()` on the resulting codestream. 6. Compare decoded sample data with original byte-for-byte. |
| **Expected Result** | All 64 samples are identical. |
| **Pass Criteria** | `memcmp()` returns 0 for the entire data buffer. |

### MT-API-004 — Lossy Encode/Decode (9/7 Filter) ✅

| Field | Value |
|-------|-------|
| **Description** | Encode at a target bit-rate and verify quality metrics. |
| **Prerequisites** | A 64×64×16 raw volume. |
| **Steps** | 1. Create volume. 2. Set parameters: `rate = 2.0`, `filter = OPJ_JP3D_DWT97`. 3. Encode, then decode. 4. Compute PSNR between original and decoded. |
| **Expected Result** | Codestream is smaller than lossless. PSNR ≥ 30 dB. |
| **Pass Criteria** | PSNR is finite and above the threshold; decoded dimensions match original. |

### MT-API-005 — HTJ2K Lossless Round-Trip ✅

| Field | Value |
|-------|-------|
| **Description** | Encode with HTJ2K block coder and verify lossless decode. |
| **Prerequisites** | A 4×4×4 raw volume. |
| **Steps** | 1. Set default encoder parameters. 2. Set `use_htj2k = 1`. 3. Encode, decode, compare. |
| **Expected Result** | Bit-exact output. |
| **Pass Criteria** | `memcmp()` returns 0. |

### MT-API-006 — Transcoding EBCOT → HTJ2K ✅

| Field | Value |
|-------|-------|
| **Description** | Transcode an existing EBCOT codestream to HTJ2K without quality loss. |
| **Prerequisites** | A lossless EBCOT-encoded JP3D codestream. |
| **Steps** | 1. Encode a volume with default (EBCOT) settings. 2. Call `opj_jp3d_transcode_to_ht()` on the codestream. 3. Decode the HTJ2K codestream. 4. Compare with original data. |
| **Expected Result** | Bit-exact output after transcode → decode. |
| **Pass Criteria** | `memcmp()` returns 0. |

### MT-API-007 — 16-bit Signed Volume ✅

| Field | Value |
|-------|-------|
| **Description** | Round-trip a 16-bit signed volume. |
| **Prerequisites** | Raw file with 16-bit signed samples (little-endian). |
| **Steps** | 1. Create volume with `prec=16, sgnd=1`. 2. Fill with values in [−32768, 32767]. 3. Lossless encode and decode. 4. Compare. |
| **Expected Result** | Bit-exact. |
| **Pass Criteria** | All samples match, including negative values. |

### MT-API-008 — Multi-Component Volume ✅

| Field | Value |
|-------|-------|
| **Description** | Round-trip a volume with multiple components (e.g. 3-component RGB). |
| **Prerequisites** | None. |
| **Steps** | 1. Create volume with `numcomps=3`, dimensions 4×4×4, 8-bit. 2. Encode losslessly. 3. Decode and compare all 3 components. |
| **Expected Result** | All 3 components are bit-exact. |
| **Pass Criteria** | `memcmp()` returns 0 for each component. |

### MT-API-009 — Error Callback ✅

| Field | Value |
|-------|-------|
| **Description** | Verify that the message callback is invoked on error. |
| **Prerequisites** | None. |
| **Steps** | 1. Register a message callback. 2. Attempt to decode an invalid codestream (e.g. random bytes). 3. Check whether the callback was invoked with an error severity. |
| **Expected Result** | Callback fires at least once with error severity. Decode returns failure. |
| **Pass Criteria** | Callback invocation count > 0; return value indicates failure. |

### MT-API-010 — Default Parameters ✅

| Field | Value |
|-------|-------|
| **Description** | Verify that default encoder/decoder parameters are sensible. |
| **Prerequisites** | None. |
| **Steps** | 1. Call `opj_jp3d_set_default_encoder_parameters()`. 2. Inspect fields. |
| **Expected Result** | `rate == 0` (lossless), `filter == OPJ_JP3D_DWT53`, `nresolution == {3,3,3}`, `use_htj2k == 0`. |
| **Pass Criteria** | All fields match the documented defaults. |

### MT-API-011 — Tiled Encoding ✅

| Field | Value |
|-------|-------|
| **Description** | Encode with explicit tile dimensions. |
| **Prerequisites** | A 64×64×16 raw volume. |
| **Steps** | 1. Set `tile_w=32, tile_h=32, tile_d=8`. 2. Encode losslessly. 3. Decode. 4. Compare. |
| **Expected Result** | Bit-exact output. |
| **Pass Criteria** | `memcmp()` returns 0. |

### MT-API-012 — Single-Slice Volume Edge Case ✅

| Field | Value |
|-------|-------|
| **Description** | Round-trip a volume with depth = 1 (degenerate 2-D case). |
| **Prerequisites** | A 4×4×1 raw file. |
| **Steps** | 1. Create volume 4×4×1. 2. Lossless encode and decode. 3. Compare. |
| **Expected Result** | Bit-exact. |
| **Pass Criteria** | No crash; output matches. |

### MT-API-013 — Memory Functions ✅

| Field | Value |
|-------|-------|
| **Description** | Verify pluggable allocators work correctly. |
| **Prerequisites** | None. |
| **Steps** | 1. Call `opj_jp3d_malloc(1024)`, write to the buffer, call `opj_jp3d_free()`. 2. Call `opj_jp3d_calloc(16, 64)`, verify zeroed, free. 3. Call `opj_jp3d_aligned_malloc(16, 256)`, verify alignment, free with `opj_jp3d_aligned_free()`. |
| **Expected Result** | No crashes, no ASan errors. Aligned pointer `% 16 == 0`. |
| **Pass Criteria** | All allocations succeed, alignment is correct, no leaks. |

---

## 3. CLI Tools

All CLI tests assume the tools are on `PATH` and a test raw volume is available.

### MT-CLI-001 — Compress Help ✅

| Field | Value |
|-------|-------|
| **Description** | Verify `opj_jp3d_compress --help` prints usage. |
| **Steps** | 1. Run `opj_jp3d_compress --help`. |
| **Expected Result** | Prints usage text listing all options and exits with code 0. |
| **Pass Criteria** | Output contains `-i`, `-o`, `-W`, `-H`, `-D` flags. |

### MT-CLI-002 — Compress Version ✅

| Field | Value |
|-------|-------|
| **Description** | Verify `opj_jp3d_compress --version` prints the library version. |
| **Steps** | 1. Run `opj_jp3d_compress --version`. |
| **Expected Result** | Prints version in `X.Y.Z` format. |
| **Pass Criteria** | Output matches the library version string. |

### MT-CLI-003 — Lossless Compress/Decompress ✅

| Field | Value |
|-------|-------|
| **Description** | Round-trip a volume through the CLI tools. |
| **Prerequisites** | `/tmp/test_4x4x4.raw`. |
| **Steps** | 1. `opj_jp3d_compress -i /tmp/test_4x4x4.raw -o /tmp/out.jp3d -W 4 -H 4 -D 4` 2. `opj_jp3d_decompress -i /tmp/out.jp3d -o /tmp/decoded.raw` 3. `diff /tmp/test_4x4x4.raw /tmp/decoded.raw` |
| **Expected Result** | `diff` reports no differences. |
| **Pass Criteria** | Exit code 0 from diff; files are identical. |

### MT-CLI-004 — Lossy Compress ✅

| Field | Value |
|-------|-------|
| **Description** | Compress at a target bit-rate. |
| **Prerequisites** | `/tmp/test_64x64x16.raw`. |
| **Steps** | 1. `opj_jp3d_compress -i /tmp/test_64x64x16.raw -o /tmp/lossy.jp3d -W 64 -H 64 -D 16 -r 2.0 -f 97` 2. Verify output file size is less than input. 3. `opj_jp3d_decompress -i /tmp/lossy.jp3d -o /tmp/lossy_dec.raw` 4. Verify decoded file has the same size as input. |
| **Expected Result** | Compressed file is smaller; decoded file has correct sample count. |
| **Pass Criteria** | Output file exists and is smaller; decoded dimensions match original. |

### MT-CLI-005 — HTJ2K Mode ✅

| Field | Value |
|-------|-------|
| **Description** | Compress with the HTJ2K block coder. |
| **Prerequisites** | `/tmp/test_4x4x4.raw`. |
| **Steps** | 1. `opj_jp3d_compress -i /tmp/test_4x4x4.raw -o /tmp/ht.jp3d -W 4 -H 4 -D 4 -H2K` 2. `opj_jp3d_decompress -i /tmp/ht.jp3d -o /tmp/ht_dec.raw` 3. `diff /tmp/test_4x4x4.raw /tmp/ht_dec.raw` |
| **Expected Result** | Bit-exact lossless round-trip. |
| **Pass Criteria** | `diff` returns no differences. |

### MT-CLI-006 — Custom Tile Dimensions ✅

| Field | Value |
|-------|-------|
| **Description** | Compress with explicit tile sizes. |
| **Prerequisites** | `/tmp/test_64x64x16.raw`. |
| **Steps** | 1. `opj_jp3d_compress -i /tmp/test_64x64x16.raw -o /tmp/tiled.jp3d -W 64 -H 64 -D 16 -t 32,32,8` 2. `opj_jp3d_decompress -i /tmp/tiled.jp3d -o /tmp/tiled_dec.raw` 3. `diff /tmp/test_64x64x16.raw /tmp/tiled_dec.raw` |
| **Expected Result** | Lossless round-trip with tiling. |
| **Pass Criteria** | Files are identical. |

### MT-CLI-007 — Custom DWT Levels ✅

| Field | Value |
|-------|-------|
| **Description** | Compress with non-default resolution levels. |
| **Prerequisites** | `/tmp/test_64x64x16.raw`. |
| **Steps** | 1. `opj_jp3d_compress -i /tmp/test_64x64x16.raw -o /tmp/levels.jp3d -W 64 -H 64 -D 16 -n 2,2,1` 2. `opj_jp3d_decompress -i /tmp/levels.jp3d -o /tmp/levels_dec.raw` 3. `diff /tmp/test_64x64x16.raw /tmp/levels_dec.raw` |
| **Expected Result** | Lossless round-trip. |
| **Pass Criteria** | Files are identical. |

### MT-CLI-008 — 16-bit Compress ✅

| Field | Value |
|-------|-------|
| **Description** | Compress a 16-bit volume. |
| **Prerequisites** | A 4×4×4 16-bit raw file (128 bytes). |
| **Steps** | 1. `opj_jp3d_compress -i /tmp/test16.raw -o /tmp/out16.jp3d -W 4 -H 4 -D 4 -p 16` 2. `opj_jp3d_decompress -i /tmp/out16.jp3d -o /tmp/dec16.raw` 3. `diff /tmp/test16.raw /tmp/dec16.raw` |
| **Expected Result** | Bit-exact. |
| **Pass Criteria** | Files are identical. |

### MT-CLI-009 — Signed Samples ✅

| Field | Value |
|-------|-------|
| **Description** | Compress signed integer samples. |
| **Prerequisites** | A 4×4×4 8-bit signed raw file. |
| **Steps** | 1. `opj_jp3d_compress -i /tmp/signed.raw -o /tmp/signed.jp3d -W 4 -H 4 -D 4 -s` 2. `opj_jp3d_decompress -i /tmp/signed.jp3d -o /tmp/signed_dec.raw` 3. `diff /tmp/signed.raw /tmp/signed_dec.raw` |
| **Expected Result** | Bit-exact round-trip preserving sign. |
| **Pass Criteria** | Files are identical. |

### MT-CLI-010 — Multi-Component Compress ✅

| Field | Value |
|-------|-------|
| **Description** | Compress a multi-component (3-channel) volume. |
| **Prerequisites** | A 4×4×4×3 raw file (192 bytes). |
| **Steps** | 1. `opj_jp3d_compress -i /tmp/rgb.raw -o /tmp/rgb.jp3d -W 4 -H 4 -D 4 -c 3` 2. `opj_jp3d_decompress -i /tmp/rgb.jp3d -o /tmp/rgb_dec.raw` 3. `diff /tmp/rgb.raw /tmp/rgb_dec.raw` |
| **Expected Result** | Bit-exact. |
| **Pass Criteria** | Files are identical. |

### MT-CLI-011 — Verbose Output ✅

| Field | Value |
|-------|-------|
| **Description** | Verify `-v` produces additional diagnostic output. |
| **Steps** | 1. Run `opj_jp3d_compress -i /tmp/test_4x4x4.raw -o /tmp/v.jp3d -W 4 -H 4 -D 4 -v` |
| **Expected Result** | Stderr/stdout shows extra information (e.g. tile count, DWT levels, timing). |
| **Pass Criteria** | Output is noticeably more verbose than without `-v`. |

### MT-CLI-012 — Dump Codestream ✅

| Field | Value |
|-------|-------|
| **Description** | Inspect a JP3D codestream with `opj_jp3d_dump`. |
| **Prerequisites** | `/tmp/out.jp3d` from MT-CLI-003. |
| **Steps** | 1. `opj_jp3d_dump -i /tmp/out.jp3d` |
| **Expected Result** | Prints marker information: SOC, SIZ3D, COD3D, QCD3D, SOT, SOD, EOC. Dimensions match 4×4×4. |
| **Pass Criteria** | Output contains recognized marker names and correct dimensions. |

### MT-CLI-013 — Transcode EBCOT → HTJ2K ✅

| Field | Value |
|-------|-------|
| **Description** | Losslessly transcode a codestream from EBCOT to HTJ2K. |
| **Prerequisites** | `/tmp/out.jp3d` (EBCOT-encoded). |
| **Steps** | 1. `opj_jp3d_transcode -i /tmp/out.jp3d -o /tmp/transcoded.jp3d` 2. `opj_jp3d_decompress -i /tmp/transcoded.jp3d -o /tmp/transcoded_dec.raw` 3. `diff /tmp/test_4x4x4.raw /tmp/transcoded_dec.raw` |
| **Expected Result** | Lossless transcode; decoded data matches original. |
| **Pass Criteria** | `diff` reports no differences. |

### MT-CLI-014 — Missing Input File ✅

| Field | Value |
|-------|-------|
| **Description** | Verify tools report an error for a nonexistent input. |
| **Steps** | 1. `opj_jp3d_compress -i /tmp/nonexistent.raw -o /tmp/x.jp3d -W 4 -H 4 -D 4` |
| **Expected Result** | Non-zero exit code; error message printed. |
| **Pass Criteria** | Exit code ≠ 0; output contains an error message referencing the file. |

### MT-CLI-015 — Missing Required Arguments ✅

| Field | Value |
|-------|-------|
| **Description** | Verify compress fails gracefully when required options are omitted. |
| **Steps** | 1. `opj_jp3d_compress -i /tmp/test_4x4x4.raw -o /tmp/x.jp3d` (missing `-W -H -D`). |
| **Expected Result** | Non-zero exit code; usage hint or error message. |
| **Pass Criteria** | Tool does not crash; exit code ≠ 0. |

### MT-CLI-016 — Decompress Help and Version ✅

| Field | Value |
|-------|-------|
| **Description** | Verify `opj_jp3d_decompress --help` and `--version`. |
| **Steps** | 1. `opj_jp3d_decompress --help` 2. `opj_jp3d_decompress --version` |
| **Expected Result** | Help text lists `-i` and `-o`; version prints `X.Y.Z`. |
| **Pass Criteria** | Both exit with code 0 and produce expected output. |

### MT-CLI-017 — Transcode Help and Version ✅

| Field | Value |
|-------|-------|
| **Description** | Verify `opj_jp3d_transcode --help` and `--version`. |
| **Steps** | 1. `opj_jp3d_transcode --help` 2. `opj_jp3d_transcode --version` |
| **Expected Result** | Help text and version displayed. |
| **Pass Criteria** | Both exit with code 0. |

### MT-CLI-018 — Dump Help and Version ✅

| Field | Value |
|-------|-------|
| **Description** | Verify `opj_jp3d_dump --help` and `--version`. |
| **Steps** | 1. `opj_jp3d_dump --help` 2. `opj_jp3d_dump --version` |
| **Expected Result** | Help text and version displayed. |
| **Pass Criteria** | Both exit with code 0. |

---

## 4. GUI Application

> **Note:** GUI tests require `BUILD_GUI_TOOLS=ON`, SDL2 development libraries
> (`libsdl2-dev`), and a display (physical or virtual via `Xvfb`). The GUI
> executable is `opj_jp3d_gui`.

### MT-GUI-001 — Application Launch ✅

| Field | Value |
|-------|-------|
| **Description** | Verify the GUI starts and shows the main window. |
| **Steps** | 1. Run `opj_jp3d_gui`. |
| **Expected Result** | A window appears with the menu bar, viewer panel, and log console. |
| **Pass Criteria** | No crash; window is visible and responsive. |

### MT-GUI-002 — Load Raw Volume ✅

| Field | Value |
|-------|-------|
| **Description** | Open a raw binary volume file. |
| **Steps** | 1. Launch GUI. 2. File → Open → select a `.raw` file. 3. Enter dimensions and bit-depth in the dialog. |
| **Expected Result** | Volume loads. Metadata panel shows correct dimensions, bit-depth, and component count. Slice viewer shows the first axial slice. |
| **Pass Criteria** | No crash; metadata matches the file. |

### MT-GUI-003 — Load JP3D Codestream ✅

| Field | Value |
|-------|-------|
| **Description** | Open a JP3D codestream file. |
| **Steps** | 1. Launch GUI. 2. File → Open → select a `.jp3d` file. |
| **Expected Result** | Codestream is decoded and volume is displayed. Metadata shows HTJ2K mode, filter type, and resolution levels. |
| **Pass Criteria** | Volume renders correctly; metadata is accurate. |

### MT-GUI-004 — Slice Viewer Navigation ✅

| Field | Value |
|-------|-------|
| **Description** | Navigate through slices in all three axes. |
| **Prerequisites** | A volume loaded (≥ 4 slices in each dimension). |
| **Steps** | 1. Select Axial view; scroll through slices. 2. Switch to Sagittal view; scroll. 3. Switch to Coronal view; scroll. |
| **Expected Result** | Slice number updates in the UI. Image changes with each slice. All three axes work. |
| **Pass Criteria** | Slice index is bounded within valid range; no blank images for valid slices. |

### MT-GUI-005 — Window/Level Adjustment ✅

| Field | Value |
|-------|-------|
| **Description** | Adjust window and level controls. |
| **Prerequisites** | A volume loaded. |
| **Steps** | 1. Modify window width slider. 2. Modify level slider. |
| **Expected Result** | Image contrast changes in real time. |
| **Pass Criteria** | Visual change is apparent; no artefacts or crashes. |

### MT-GUI-006 — Volume Statistics ✅

| Field | Value |
|-------|-------|
| **Description** | View per-component statistics. |
| **Prerequisites** | A volume loaded. |
| **Steps** | 1. Open statistics panel. |
| **Expected Result** | Displays min, max, mean, and standard deviation for each component. |
| **Pass Criteria** | Values are within the valid range for the bit-depth; no NaN/Inf. |

### MT-GUI-007 — Histogram Display ✅

| Field | Value |
|-------|-------|
| **Description** | View the 256-bin intensity histogram. |
| **Prerequisites** | A volume loaded. |
| **Steps** | 1. Open the histogram panel. |
| **Expected Result** | A histogram chart is rendered with 256 bins. |
| **Pass Criteria** | Histogram is non-empty; bins sum to the total voxel count. |

### MT-GUI-008 — 3-D Ray-Cast Renderer ✅

| Field | Value |
|-------|-------|
| **Description** | Render the volume in 3-D. |
| **Prerequisites** | A volume loaded; GPU with OpenGL 3.3 support. |
| **Steps** | 1. Switch to the 3-D view. 2. Adjust azimuth, elevation, and density sliders. |
| **Expected Result** | A 3-D ray-cast rendering is displayed. Adjusting sliders changes the view in real time. |
| **Pass Criteria** | Rendering updates smoothly; no black screen or GL errors. |

### MT-GUI-009 — Encode Panel ✅

| Field | Value |
|-------|-------|
| **Description** | Encode a loaded volume using the GUI encode panel. |
| **Prerequisites** | A raw volume loaded. |
| **Steps** | 1. Open the Encode panel. 2. Set parameters: tile size, DWT levels, filter (5/3), rate = 0. 3. Set output path. 4. Click Encode. |
| **Expected Result** | Progress bar appears. On completion, a JP3D file is written. |
| **Pass Criteria** | File exists and can be decoded; status shows "Complete". |

### MT-GUI-010 — Encode with HTJ2K Toggle ✅

| Field | Value |
|-------|-------|
| **Description** | Encode with the HTJ2K toggle enabled. |
| **Prerequisites** | A raw volume loaded. |
| **Steps** | 1. Open Encode panel. 2. Enable HTJ2K checkbox. 3. Encode. |
| **Expected Result** | Output codestream uses HTJ2K block coder (can verify via Dump). |
| **Pass Criteria** | Encode succeeds; dump confirms HTJ2K mode. |

### MT-GUI-011 — Decode Panel ✅

| Field | Value |
|-------|-------|
| **Description** | Decode a JP3D codestream using the GUI. |
| **Prerequisites** | A JP3D file. |
| **Steps** | 1. Open Decode panel. 2. Select the JP3D file. 3. Click Decode. |
| **Expected Result** | Volume loads into the viewer on completion. |
| **Pass Criteria** | Decoded volume is displayed correctly. |

### MT-GUI-012 — Decode Sub-Volume Extraction ✅

| Field | Value |
|-------|-------|
| **Description** | Decode only a sub-volume region. |
| **Prerequisites** | A JP3D file with dimensions ≥ 8×8×4. |
| **Steps** | 1. Open Decode panel. 2. Set sub-volume offset and size. 3. Click Decode. |
| **Expected Result** | Only the requested sub-volume is decoded and displayed. |
| **Pass Criteria** | Decoded dimensions match the sub-volume request. |

### MT-GUI-013 — Decode Reduced Resolution ✅

| Field | Value |
|-------|-------|
| **Description** | Decode at a reduced resolution level. |
| **Prerequisites** | A JP3D file encoded with ≥ 2 DWT levels. |
| **Steps** | 1. Open Decode panel. 2. Set reduction level to 1. 3. Decode. |
| **Expected Result** | Decoded volume has half the resolution in each axis. |
| **Pass Criteria** | Dimensions are approximately half the original. |

### MT-GUI-014 — Transcode Panel ✅

| Field | Value |
|-------|-------|
| **Description** | Transcode a codestream via the GUI. |
| **Steps** | 1. Open Transcode panel. 2. Select input EBCOT codestream. 3. Set output path. 4. Click Transcode. |
| **Expected Result** | Output HTJ2K codestream is written. |
| **Pass Criteria** | File exists; can be decoded. |

### MT-GUI-015 — Progress and Cancellation ✅

| Field | Value |
|-------|-------|
| **Description** | Verify progress overlay and cancellation for long operations. |
| **Prerequisites** | A large volume (e.g. 256×256×64). |
| **Steps** | 1. Start an encode. 2. Observe the progress window. 3. Click Cancel. |
| **Expected Result** | Progress bar advances. Cancel stops the operation gracefully. |
| **Pass Criteria** | No crash on cancel; partial output is cleaned up or left in a safe state. |

### MT-GUI-016 — Round-Trip Test Wizard ✅

| Field | Value |
|-------|-------|
| **Description** | Run the one-click encode→decode→compare wizard. |
| **Prerequisites** | A volume loaded. |
| **Steps** | 1. Open Round-Trip panel. 2. Configure parameters. 3. Click Run. |
| **Expected Result** | Wizard completes and shows PSNR, MSE, max error, compression ratio, and encode/decode timing. For lossless mode, reports "Lossless: Yes". |
| **Pass Criteria** | Metrics are computed and displayed; no errors. |

### MT-GUI-017 — Diff Viewer ✅

| Field | Value |
|-------|-------|
| **Description** | Compare original and decoded volumes visually. |
| **Prerequisites** | A completed round-trip test. |
| **Steps** | 1. Open Diff Viewer. 2. Load original and decoded volumes. 3. Adjust error threshold. |
| **Expected Result** | Side-by-side or overlay display showing per-voxel differences. |
| **Pass Criteria** | Difference map updates with threshold changes; zero diff for lossless. |

### MT-GUI-018 — Batch Test Runner ✅

| Field | Value |
|-------|-------|
| **Description** | Queue multiple parameter configurations and run them. |
| **Prerequisites** | A volume loaded. |
| **Steps** | 1. Open Batch Runner. 2. Add 3+ configurations (vary filter, rate, HTJ2K). 3. Click Run All. 4. Export results as CSV. |
| **Expected Result** | Results table populates with PSNR, ratio, pass/fail for each config. CSV file is written. |
| **Pass Criteria** | All configurations run to completion; CSV is valid. |

### MT-GUI-019 — Codestream Inspector ✅

| Field | Value |
|-------|-------|
| **Description** | Inspect codestream structure in the tree view. |
| **Prerequisites** | A JP3D file. |
| **Steps** | 1. Open Codestream Inspector. 2. Load a JP3D file. |
| **Expected Result** | Tree view shows markers: SOC, SIZ3D, COD3D, QCD3D, SOT, SOD, EOC with byte offsets and field values. |
| **Pass Criteria** | All expected markers are listed; expanding nodes shows details. |

### MT-GUI-020 — Log Console ✅

| Field | Value |
|-------|-------|
| **Description** | Verify the log console captures codec messages. |
| **Steps** | 1. Perform an encode or decode. 2. Open the log console. |
| **Expected Result** | Log entries appear with timestamps (HH:MM:SS) and severity colours. |
| **Pass Criteria** | Messages are visible; severity filter works. |

### MT-GUI-021 — Log Export ✅

| Field | Value |
|-------|-------|
| **Description** | Export log to a file and copy to clipboard. |
| **Steps** | 1. Generate log messages. 2. Click Export. 3. Click Copy to Clipboard. |
| **Expected Result** | Log file is written. Clipboard contains log text. |
| **Pass Criteria** | File is non-empty; clipboard paste matches log content. |

### MT-GUI-022 — Preferences Dialog ✅

| Field | Value |
|-------|-------|
| **Description** | Open preferences, change settings, verify persistence. |
| **Steps** | 1. Open Preferences. 2. Change theme to Light. 3. Change default thread count. 4. Close and reopen the application. |
| **Expected Result** | Settings persist across restarts (stored in INI file). |
| **Pass Criteria** | Theme and thread count are restored on restart. |

### MT-GUI-023 — Encoder Presets ✅

| Field | Value |
|-------|-------|
| **Description** | Save and load an encoder preset. |
| **Steps** | 1. Open Preferences. 2. Configure encoder preset (tile size, levels, rate). 3. Save. 4. Open Encode panel; verify preset values are applied. |
| **Expected Result** | Preset values populate the encode panel. |
| **Pass Criteria** | All preset fields match saved values. |

### MT-GUI-024 — Keyboard Shortcuts ✅

| Field | Value |
|-------|-------|
| **Description** | Verify default keyboard shortcuts and customisation. |
| **Steps** | 1. Press Ctrl+O (Open). 2. Press Ctrl+Q (Quit). 3. Open Preferences → Shortcuts. 4. Rebind "Open" to Ctrl+Shift+O. 5. Save. 6. Verify new binding works. |
| **Expected Result** | Shortcuts trigger the correct actions. Rebinding persists. |
| **Pass Criteria** | All 9 default shortcuts work; custom bindings are stored in INI. |

### MT-GUI-025 — Theme Toggle ✅

| Field | Value |
|-------|-------|
| **Description** | Toggle between dark and light themes. |
| **Steps** | 1. Use the theme toggle shortcut or Preferences → Theme. |
| **Expected Result** | UI switches between dark and light colour schemes. |
| **Pass Criteria** | All panels are readable in both themes; no rendering artefacts. |

---

## 5. JPIP 3-D Streaming

### MT-JPIP-001 — Server Help and Version ✅

| Field | Value |
|-------|-------|
| **Description** | Verify `opj_jpip3d_server --help` and `--version`. |
| **Steps** | 1. `opj_jpip3d_server --help` 2. `opj_jpip3d_server --version` |
| **Expected Result** | Help text lists `-d`, `-f` flags; version is printed. |
| **Pass Criteria** | Both exit with code 0. |

### MT-JPIP-002 — Server Startup ✅

| Field | Value |
|-------|-------|
| **Description** | Start the JPIP server with a JP3D dataset. |
| **Prerequisites** | A JP3D codestream file. |
| **Steps** | 1. `echo "" | opj_jpip3d_server -d test_dataset -f /tmp/out.jp3d` |
| **Expected Result** | Server starts, reads the dataset, and exits when stdin closes. |
| **Pass Criteria** | No crash; no error messages. |

### MT-JPIP-003 — GUI JPIP Connection Dialog ✅

| Field | Value |
|-------|-------|
| **Description** | Connect to a JPIP server from the GUI. |
| **Prerequisites** | `BUILD_GUI_TOOLS=ON`; a running JPIP server. |
| **Steps** | 1. Launch GUI. 2. Open JPIP → Connect. 3. Enter server URL. 4. Click Connect. |
| **Expected Result** | Datasets are listed with dimensions and component info. |
| **Pass Criteria** | Dataset list is populated; selecting one shows metadata. |

### MT-JPIP-004 — GUI Sub-Volume Browse ✅

| Field | Value |
|-------|-------|
| **Description** | Request a sub-volume region from the JPIP server. |
| **Prerequisites** | Connected to a JPIP server with a dataset loaded. |
| **Steps** | 1. Open Sub-Volume Browser. 2. Set ROI offset and size. 3. Set resolution level and quality layer. 4. Click Fetch. |
| **Expected Result** | Sub-volume is fetched and displayed in the viewer. Progress is shown during transfer. |
| **Pass Criteria** | Fetched data matches the requested region dimensions. |

### MT-JPIP-005 — GUI Network Diagnostics ✅

| Field | Value |
|-------|-------|
| **Description** | View JPIP session statistics. |
| **Prerequisites** | Active JPIP session with at least one completed request. |
| **Steps** | 1. Open Network Diagnostics panel. |
| **Expected Result** | Displays bytes transferred, request count, cache hit rate, and average latency. |
| **Pass Criteria** | All metrics are non-negative; bytes transferred > 0. |

---

## 6. Python Bindings

> **Prerequisites:** `pip install -e python/[numpy]` and
> `export OPENJP3D_LIBRARY=<path>`.

### MT-PY-001 — Import and Version ✅

| Field | Value |
|-------|-------|
| **Description** | Import the package and check the version. |
| **Steps** | 1. `python3 -c "import openjp3d; print(openjp3d.get_version())"` |
| **Expected Result** | Prints the library version string (e.g. `1.0.0`). |
| **Pass Criteria** | No import errors; version is non-empty. |

### MT-PY-002 — Lossless Round-Trip (uint8) ✅

| Field | Value |
|-------|-------|
| **Description** | Encode and decode a NumPy `uint8` array losslessly. |
| **Steps** | 1. `import numpy as np; import openjp3d` 2. `vol = np.random.randint(0, 256, (4,4,4), dtype=np.uint8)` 3. `data = openjp3d.encode(vol)` 4. `dec = openjp3d.decode(data)` 5. `assert np.array_equal(vol, dec)` |
| **Expected Result** | Assertion passes. |
| **Pass Criteria** | Arrays are identical. |

### MT-PY-003 — Lossless Round-Trip (int16) ✅

| Field | Value |
|-------|-------|
| **Description** | Round-trip a `int16` signed array. |
| **Steps** | 1. `vol = np.random.randint(-32768, 32767, (4,4,4), dtype=np.int16)` 2. Encode, decode, compare. |
| **Expected Result** | Bit-exact. |
| **Pass Criteria** | `np.array_equal` returns True. |

### MT-PY-004 — Multi-Component Array ✅

| Field | Value |
|-------|-------|
| **Description** | Round-trip a `(D,H,W,C)` array. |
| **Steps** | 1. `vol = np.random.randint(0, 256, (4,4,4,3), dtype=np.uint8)` 2. Encode, decode, compare. |
| **Expected Result** | All 3 components are bit-exact. |
| **Pass Criteria** | `np.array_equal` returns True. |

### MT-PY-005 — Lossy Encoding ✅

| Field | Value |
|-------|-------|
| **Description** | Encode at a target bit-rate. |
| **Steps** | 1. `params = openjp3d.EncodeParams(rate=2.0, filter_97=True)` 2. `data = openjp3d.encode(vol, params=params)` 3. Decode and compute PSNR. |
| **Expected Result** | Codestream is smaller; PSNR ≥ 30 dB. |
| **Pass Criteria** | Codestream size < lossless; PSNR is finite. |

### MT-PY-006 — HTJ2K Encoding ✅

| Field | Value |
|-------|-------|
| **Description** | Encode with HTJ2K mode. |
| **Steps** | 1. `params = openjp3d.EncodeParams(use_htj2k=True)` 2. Encode, decode, compare. |
| **Expected Result** | Lossless round-trip with HTJ2K. |
| **Pass Criteria** | `np.array_equal` returns True. |

### MT-PY-007 — Transcode to HTJ2K ✅

| Field | Value |
|-------|-------|
| **Description** | Transcode EBCOT → HTJ2K via Python. |
| **Steps** | 1. Encode without HTJ2K. 2. `ht_data = openjp3d.transcode_to_ht(data)` 3. Decode `ht_data` and compare. |
| **Expected Result** | Bit-exact after transcoding. |
| **Pass Criteria** | Arrays match. |

### MT-PY-008 — Message Callback ✅

| Field | Value |
|-------|-------|
| **Description** | Verify message callback fires during encode/decode. |
| **Steps** | 1. Define a callback that appends messages to a list. 2. Encode with the callback. 3. Verify the list is non-empty. |
| **Expected Result** | At least one message captured. |
| **Pass Criteria** | Callback was invoked; message strings are non-empty. |

### MT-PY-009 — Invalid Input Handling ✅

| Field | Value |
|-------|-------|
| **Description** | Pass invalid data to decode. |
| **Steps** | 1. `openjp3d.decode(b"not a valid codestream")` |
| **Expected Result** | Raises an exception (not a crash). |
| **Pass Criteria** | A Python exception is raised. |

### MT-PY-010 — EncodeParams Defaults ✅

| Field | Value |
|-------|-------|
| **Description** | Verify EncodeParams default values. |
| **Steps** | 1. `p = openjp3d.EncodeParams()` 2. Check `p.rate == 0`, `p.filter_97 == False`, `p.use_htj2k == False`. |
| **Expected Result** | All defaults match documentation. |
| **Pass Criteria** | Values match. |

---

## 7. Julia Bindings

> **Prerequisites:** `julia -e 'using Pkg; Pkg.develop(path="julia/OpenJP3D.jl")'`
> and `ENV["OPENJP3D_LIBRARY"]` set.

### MT-JL-001 — Import and Version ✅

| Field | Value |
|-------|-------|
| **Description** | Load the package and get the library version. |
| **Steps** | 1. `julia -e 'using OpenJP3D; println(OpenJP3D.get_version())'` |
| **Expected Result** | Prints version string. |
| **Pass Criteria** | No errors; version is non-empty. |

### MT-JL-002 — Lossless Round-Trip (UInt8) ✅

| Field | Value |
|-------|-------|
| **Description** | Encode and decode a `UInt8` array. |
| **Steps** | 1. `vol = rand(UInt8, 4, 4, 4)` 2. `data = OpenJP3D.encode(vol)` 3. `dec = OpenJP3D.decode(data)` 4. `@assert vol == dec` |
| **Expected Result** | Assertion passes. |
| **Pass Criteria** | Arrays are identical. |

### MT-JL-003 — Signed 16-bit Round-Trip ✅

| Field | Value |
|-------|-------|
| **Description** | Round-trip an `Int16` array. |
| **Steps** | 1. `vol = rand(Int16, 4, 4, 4)` 2. Encode, decode, compare. |
| **Expected Result** | Bit-exact. |
| **Pass Criteria** | `vol == dec`. |

### MT-JL-004 — Multi-Component ✅

| Field | Value |
|-------|-------|
| **Description** | Round-trip a `(D,H,W,C)` array. |
| **Steps** | 1. `vol = rand(UInt8, 4, 4, 4, 3)` 2. Encode, decode, compare. |
| **Expected Result** | All components match. |
| **Pass Criteria** | Assertion passes. |

### MT-JL-005 — HTJ2K and Transcode ✅

| Field | Value |
|-------|-------|
| **Description** | HTJ2K encode and EBCOT→HTJ2K transcode. |
| **Steps** | 1. Encode with `use_htj2k=true`. 2. Decode and compare. 3. Encode without HTJ2K; transcode; decode; compare. |
| **Expected Result** | Both paths produce bit-exact output. |
| **Pass Criteria** | All comparisons pass. |

### MT-JL-006 — Message Callback ✅

| Field | Value |
|-------|-------|
| **Description** | Verify message callback works. |
| **Steps** | 1. Pass a callback function to `encode`. 2. Check it was invoked. |
| **Expected Result** | Callback fires. |
| **Pass Criteria** | At least one message received. |

---

## 8. R Bindings

> **Prerequisites:** `R CMD INSTALL r/openjp3d` and `OPENJP3D_LIBRARY` set.

### MT-R-001 — Load and Version ✅

| Field | Value |
|-------|-------|
| **Description** | Load the package and check the version. |
| **Steps** | 1. `Rscript -e 'library(openjp3d); cat(get_version(), "\n")'` |
| **Expected Result** | Version string is printed. |
| **Pass Criteria** | No errors; version is non-empty. |

### MT-R-002 — Lossless Round-Trip ✅

| Field | Value |
|-------|-------|
| **Description** | Encode and decode an integer array. |
| **Steps** | 1. `vol <- array(as.integer(0:63), dim=c(4,4,4))` 2. `data <- encode(vol, prec=8L, sgnd=FALSE)` 3. `dec <- decode(data)` 4. `stopifnot(identical(as.integer(vol), as.integer(dec)))` |
| **Expected Result** | Assertion passes. |
| **Pass Criteria** | Arrays are identical. |

### MT-R-003 — EncodeParams ✅

| Field | Value |
|-------|-------|
| **Description** | Use EncodeParams for lossy encoding. |
| **Steps** | 1. `p <- EncodeParams(rate=2.0, filter_97=TRUE)` 2. Encode with params. 3. Decode and verify. |
| **Expected Result** | Encoding succeeds; output is lossy. |
| **Pass Criteria** | No errors; decoded dimensions match. |

### MT-R-004 — Transcode to HTJ2K ✅

| Field | Value |
|-------|-------|
| **Description** | Transcode EBCOT → HTJ2K from R. |
| **Steps** | 1. Encode losslessly. 2. `ht <- transcode_to_ht(data)` 3. Decode `ht` and compare. |
| **Expected Result** | Bit-exact. |
| **Pass Criteria** | Values match. |

### MT-R-005 — Message Callback ✅

| Field | Value |
|-------|-------|
| **Description** | Verify callback receives messages. |
| **Steps** | 1. Pass a callback function to encode. 2. Check messages. |
| **Expected Result** | At least one message captured. |
| **Pass Criteria** | Callback list is non-empty. |

---

## 9. MATLAB/Octave Bindings

> **Prerequisites:** `BUILD_MATLAB_BINDINGS=ON`; Octave or MATLAB installed.

### MT-MAT-001 — Load and Version ✅

| Field | Value |
|-------|-------|
| **Description** | Load the library and get the version. |
| **Steps** | 1. In Octave/MATLAB: `openjp3d.load_lib('<path>'); v = openjp3d.get_version(); disp(v);` |
| **Expected Result** | Version string is displayed. |
| **Pass Criteria** | No errors; version is non-empty. |

### MT-MAT-002 — Lossless Round-Trip ✅

| Field | Value |
|-------|-------|
| **Description** | Encode and decode a uint8 3-D array. |
| **Steps** | 1. `vol = uint8(randi(255, 4, 4, 4));` 2. `data = openjp3d.encode(vol);` 3. `dec = openjp3d.decode(data);` 4. `assert(isequal(vol, dec));` |
| **Expected Result** | Assertion passes. |
| **Pass Criteria** | Arrays are identical. |

### MT-MAT-003 — HTJ2K Encode ✅

| Field | Value |
|-------|-------|
| **Description** | Encode with HTJ2K mode. |
| **Steps** | 1. `p = openjp3d.EncodeParams(); p.use_htj2k = true;` 2. Encode, decode, compare. |
| **Expected Result** | Lossless round-trip. |
| **Pass Criteria** | `isequal` returns true. |

### MT-MAT-004 — Transcode ✅

| Field | Value |
|-------|-------|
| **Description** | Transcode EBCOT → HTJ2K. |
| **Steps** | 1. Encode without HTJ2K. 2. `ht = openjp3d.transcode_to_ht(data);` 3. Decode and compare. |
| **Expected Result** | Bit-exact. |
| **Pass Criteria** | Values match. |

---

## 10. Go Bindings

> **Prerequisites:** `BUILD_GO_BINDINGS=ON`; Go ≥ 1.21; `OPENJP3D_LIBRARY` set.

### MT-GO-001 — Load and Version ✅

| Field | Value |
|-------|-------|
| **Description** | Load the library and get the version. |
| **Steps** | 1. In Go test or main: `openjp3d.LoadLib("")` 2. `v, err := openjp3d.GetVersion()` 3. Print `v`. |
| **Expected Result** | Version string returned; no error. |
| **Pass Criteria** | `err == nil`; version is non-empty. |

### MT-GO-002 — Lossless Round-Trip ✅

| Field | Value |
|-------|-------|
| **Description** | Encode and decode an int32 slice. |
| **Steps** | 1. Create `[]int32` with 64 values (4×4×4). 2. `data, _ := openjp3d.Encode(samples, 4, 4, 4, 1, 8, false, nil, nil)` 3. `dec, info, _ := openjp3d.Decode(data, nil, nil)` 4. Compare `dec` with original. |
| **Expected Result** | All 64 values match. `info.Width==4, info.Height==4, info.Depth==4`. |
| **Pass Criteria** | Slices are identical. |

### MT-GO-003 — Lossy Encoding ✅

| Field | Value |
|-------|-------|
| **Description** | Encode with EncodeParams specifying a bit-rate. |
| **Steps** | 1. `params := &openjp3d.EncodeParams{Rate: 2.0, Filter97: true}` 2. Encode, decode, verify size is smaller. |
| **Expected Result** | Codestream is smaller than lossless. |
| **Pass Criteria** | `len(data) < len(losslessData)`. |

### MT-GO-004 — HTJ2K and Transcode ✅

| Field | Value |
|-------|-------|
| **Description** | HTJ2K encode and EBCOT→HTJ2K transcode. |
| **Steps** | 1. Encode with `UseHTJ2K: true`. 2. Decode and compare. 3. Encode without HTJ2K; `TranscodeToHT`; decode; compare. |
| **Expected Result** | Both paths produce exact output. |
| **Pass Criteria** | All samples match. |

### MT-GO-005 — Message Callback ✅

| Field | Value |
|-------|-------|
| **Description** | Verify MsgCallback receives messages. |
| **Steps** | 1. Pass a `MsgCallback` that appends to a slice. 2. Encode. 3. Check the slice. |
| **Expected Result** | At least one message received. |
| **Pass Criteria** | Slice length > 0. |

---

## 11. Rust Bindings

> **Prerequisites:** `BUILD_RUST_BINDINGS=ON`; Rust toolchain installed;
> `OPENJP3D_LIBRARY` set.

### MT-RS-001 — Load and Version ✅

| Field | Value |
|-------|-------|
| **Description** | Load the library and get the version. |
| **Steps** | 1. `openjp3d::load_lib(None).unwrap();` 2. `let v = openjp3d::get_version().unwrap();` 3. Assert `v` is non-empty. |
| **Expected Result** | Version string returned. |
| **Pass Criteria** | No panic; version matches `X.Y.Z` pattern. |

### MT-RS-002 — Lossless Round-Trip ✅

| Field | Value |
|-------|-------|
| **Description** | Encode and decode a `Vec<i32>`. |
| **Steps** | 1. Create samples: `(0..64).collect::<Vec<i32>>()`. 2. `let data = openjp3d::encode(&samples, 4, 4, 4, 1, 8, false, None, None).unwrap();` 3. `let (dec, info) = openjp3d::decode(&data, None, None).unwrap();` 4. Assert `samples == dec`. |
| **Expected Result** | All values match. |
| **Pass Criteria** | Assertion passes; `info.width==4`. |

### MT-RS-003 — HTJ2K and Transcode ✅

| Field | Value |
|-------|-------|
| **Description** | HTJ2K encode and EBCOT→HTJ2K transcode. |
| **Steps** | 1. Encode with `EncodeParams { use_htj2k: true, .. }`. 2. Decode, compare. 3. Encode without HTJ2K; `transcode_to_ht`; decode; compare. |
| **Expected Result** | Bit-exact. |
| **Pass Criteria** | Both assertions pass. |

### MT-RS-004 — Callback Closure ✅

| Field | Value |
|-------|-------|
| **Description** | Verify `&dyn Fn(i32, &str)` callback is invoked. |
| **Steps** | 1. Use `Arc<Mutex<Vec<String>>>` to collect messages. 2. Pass closure to `encode`. 3. Check collected messages. |
| **Expected Result** | At least one message captured. |
| **Pass Criteria** | Vec length > 0. |

### MT-RS-005 — Default Encode Params ✅

| Field | Value |
|-------|-------|
| **Description** | Verify `default_encode_params()` returns sensible defaults. |
| **Steps** | 1. `let p = openjp3d::default_encode_params().unwrap();` 2. Check `p.rate == 0.0`, `p.use_htj2k == false`. |
| **Expected Result** | Defaults match documentation. |
| **Pass Criteria** | All fields match. |

---

## 12. SIMD Optimisation

> **Prerequisites:** Build with `CMAKE_BUILD_TYPE=Release` (enables SIMD auto-dispatch).

### MT-SIMD-001 — Scalar vs SSE4.1 Consistency (x86-64) ✅

| Field | Value |
|-------|-------|
| **Description** | Verify SSE4.1 DWT produces bit-identical output to scalar. |
| **Steps** | 1. Encode a volume with SIMD enabled. 2. Encode the same volume with SIMD disabled (set `OPJ_JP3D_DISABLE_SIMD=1` if available, or build without SSE). 3. Compare codestreams byte-for-byte. |
| **Expected Result** | Codestreams are identical. |
| **Pass Criteria** | `memcmp()` returns 0. |

### MT-SIMD-002 — Scalar vs AVX2 Consistency (x86-64) ✅

| Field | Value |
|-------|-------|
| **Description** | Same as MT-SIMD-001 but for AVX2. |
| **Steps** | Same pattern as MT-SIMD-001 using AVX2. |
| **Expected Result** | Bit-identical codestreams. |
| **Pass Criteria** | `memcmp()` returns 0. |

### MT-SIMD-003 — Scalar vs NEON Consistency (AArch64) ✅

| Field | Value |
|-------|-------|
| **Description** | Verify NEON DWT produces bit-identical output to scalar. |
| **Steps** | Same pattern as MT-SIMD-001 on AArch64 hardware or QEMU. |
| **Expected Result** | Bit-identical. |
| **Pass Criteria** | `memcmp()` returns 0. |

### MT-SIMD-004 — Runtime CPU Feature Detection ✅

| Field | Value |
|-------|-------|
| **Description** | Verify `opj_cpu_features()` detects available ISA extensions. |
| **Steps** | 1. Call `opj_cpu_features()`. 2. On an x86-64 machine, check that at least SSE4.1 is reported. |
| **Expected Result** | Feature flags are non-zero on supported hardware. |
| **Pass Criteria** | Return value includes expected feature bits. |

---

## 13. Build System & Packaging

### MT-BUILD-001 — Default Build ✅

| Field | Value |
|-------|-------|
| **Description** | Build with default options. |
| **Steps** | 1. `cmake -B build` 2. `cmake --build build` |
| **Expected Result** | Build succeeds with no errors. `libopenjp3d` is produced. |
| **Pass Criteria** | Exit code 0; library file exists. |

### MT-BUILD-002 — Debug Build with Testing ✅

| Field | Value |
|-------|-------|
| **Description** | Full debug build with tests. |
| **Steps** | 1. `cmake -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DBUILD_CLI_TOOLS=ON -DBUILD_JPIP_3D=ON` 2. `cmake --build build` 3. `ctest --test-dir build` |
| **Expected Result** | All CTest targets pass. |
| **Pass Criteria** | `ctest` reports 0 failures. |

### MT-BUILD-003 — Optional Features Off ✅

| Field | Value |
|-------|-------|
| **Description** | Build with optional features disabled. |
| **Steps** | 1. `cmake -B build -DBUILD_HTJ2K_3D=OFF -DBUILD_JPIP_3D=OFF -DBUILD_CLI_TOOLS=OFF -DBUILD_TESTING=OFF` 2. `cmake --build build` |
| **Expected Result** | Build succeeds. Only `libopenjp3d` is produced (no CLI, no JPIP, no HTJ2K). |
| **Pass Criteria** | Exit code 0; no CLI binaries in build output. |

### MT-BUILD-004 — pkg-config File ✅

| Field | Value |
|-------|-------|
| **Description** | Verify the generated `.pc` file is valid. |
| **Steps** | 1. Install the library. 2. `pkg-config --modversion openjp3d` 3. `pkg-config --libs openjp3d` |
| **Expected Result** | Version matches; libs include `-lopenjp3d`. |
| **Pass Criteria** | Both commands succeed. |

### MT-BUILD-005 — CMake find_package ✅

| Field | Value |
|-------|-------|
| **Description** | Verify `find_package(OpenJP3D)` works from an external project. |
| **Steps** | 1. Install the library. 2. Create a minimal CMake project that does `find_package(OpenJP3D REQUIRED)`. 3. Configure and build. |
| **Expected Result** | CMake finds the package. Build succeeds. |
| **Pass Criteria** | No CMake errors; test program links and runs. |

### MT-BUILD-006 — CPack Source Tarball ✅

| Field | Value |
|-------|-------|
| **Description** | Generate a source tarball. |
| **Steps** | 1. `cd build && cpack --config CPackSourceConfig.cmake` |
| **Expected Result** | `.tar.gz` and/or `.tar.xz` archives are created. |
| **Pass Criteria** | Archive exists; extracting it yields the source tree. |

### MT-BUILD-007 — Cross-Platform CI Matrix ✅

| Field | Value |
|-------|-------|
| **Description** | Verify CI passes on all target platforms. |
| **Steps** | 1. Push a commit or open a PR. 2. Check GitHub Actions results. |
| **Expected Result** | Builds and tests pass on Linux (GCC, Clang), macOS (Apple Clang), and Windows (MSVC). |
| **Pass Criteria** | All matrix entries are green. |

---

## Test Summary Matrix

| Category | Test Count | IDs |
|----------|-----------|-----|
| Core Library (C API) | 13 | MT-API-001 – MT-API-013 |
| CLI Tools | 18 | MT-CLI-001 – MT-CLI-018 |
| GUI Application | 25 | MT-GUI-001 – MT-GUI-025 |
| JPIP Streaming | 5 | MT-JPIP-001 – MT-JPIP-005 |
| Python Bindings | 10 | MT-PY-001 – MT-PY-010 |
| Julia Bindings | 6 | MT-JL-001 – MT-JL-006 |
| R Bindings | 5 | MT-R-001 – MT-R-005 |
| MATLAB/Octave Bindings | 4 | MT-MAT-001 – MT-MAT-004 |
| Go Bindings | 5 | MT-GO-001 – MT-GO-005 |
| Rust Bindings | 5 | MT-RS-001 – MT-RS-005 |
| SIMD Optimisation | 4 | MT-SIMD-001 – MT-SIMD-004 |
| Build System & Packaging | 7 | MT-BUILD-001 – MT-BUILD-007 |
| **Total** | **107** | |
