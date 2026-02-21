# Changelog

All notable changes to the OpenJP3D project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

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
