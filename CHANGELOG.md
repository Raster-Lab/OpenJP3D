# Changelog

All notable changes to the OpenJP3D project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

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
