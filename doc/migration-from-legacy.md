# Migration from Legacy JP3D

This guide helps users of the deprecated OpenJPEG ≤ 2.4.0 JP3D implementation
migrate to OpenJP3D.

## Background

OpenJPEG versions ≤ 2.4.0 included an experimental JP3D implementation in
`src/lib/openjp3d/`. This code was removed in OpenJPEG 2.5.0 (May 2022) as
it was unmaintained and did not meet the project's quality standards.

OpenJP3D is a complete reimplementation of JP3D built on modern C11 practices,
with comprehensive test coverage, SIMD optimisation, and additional features
(HTJ2K, JPIP 3-D).

## Key Differences

### API Changes

| Legacy (OpenJPEG ≤ 2.4.0) | OpenJP3D | Notes |
|---------------------------|----------|-------|
| `opj_volume_create()` | `opj_jp3d_create_volume()` | Renamed for consistency |
| `opj_volume_destroy()` | `opj_jp3d_destroy_volume()` | Renamed for consistency |
| `opj_encode()` | `opj_jp3d_encode()` | New signature with output buffer and callback |
| `opj_decode()` | `opj_jp3d_decode()` | New signature with input buffer and callback |
| `opj_set_default_encoder_parameters()` | `opj_jp3d_set_default_encoder_parameters()` | Module-specific prefix |
| `opj_set_default_decoder_parameters()` | `opj_jp3d_set_default_decoder_parameters()` | Module-specific prefix |
| N/A | `opj_jp3d_transcode_to_ht()` | New: EBCOT → HTJ2K transcoding |

### Data Structures

**Legacy `opj_volume_t`:**

The legacy structure used separate arrays for each component and a different
memory layout. OpenJP3D uses a consistent structure:

```c
/* OpenJP3D volume */
opj_volume_t *vol = opj_jp3d_create_volume(
    1,        /* numcomps */
    256, 256, /* width, height */
    64,       /* depth */
    8,        /* bit depth */
    0         /* unsigned */
);

/* Access samples: data[z * w * h + y * w + x] */
vol->comps[0].data[0] = 42;
```

### Encoder Parameters

**Legacy approach:**
```c
/* Legacy — incomplete, undocumented fields */
opj_cparameters_t params;
opj_set_default_encoder_parameters(&params);
```

**OpenJP3D approach:**
```c
opj_jp3d_enc_params_t params;
opj_jp3d_set_default_encoder_parameters(&params);

/* All fields are documented and have safe defaults */
params.filter = OPJ_JP3D_FILTER_53;   /* Lossless 5/3 */
params.num_resolutions_x = 3;
params.num_resolutions_y = 3;
params.num_resolutions_z = 3;
params.target_rate = 0.0f;             /* 0 = lossless */
params.use_htj2k = 0;                 /* EBCOT (default) */
```

### Error Handling

The legacy code used global error state. OpenJP3D uses callback-based
error reporting:

```c
void my_handler(opj_jp3d_msg_level_t level,
                const char *msg, void *data) {
    if (level == OPJ_JP3D_MSG_ERROR)
        fprintf(stderr, "ERROR: %s\n", msg);
}

/* Pass the callback to encode/decode */
opj_jp3d_encode(vol, &params, &out, &out_size,
                my_handler, NULL);
```

### Memory Management

OpenJP3D provides its own allocation wrappers:

```c
void *buf = opj_jp3d_malloc(1024);
opj_jp3d_free(buf);
```

Codestream buffers returned by `opj_jp3d_encode()` must be freed with
`opj_jp3d_free()`. Volumes must be freed with `opj_jp3d_destroy_volume()`.

### Build System

**Legacy:** The JP3D codec was built as part of the OpenJPEG source tree
with limited configuration options.

**OpenJP3D:** Standalone CMake project with modular build options:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_CLI_TOOLS=ON \
      -DBUILD_JPIP_3D=ON
cmake --build build
```

See [INSTALL.md](../INSTALL.md) for all build options.

## New Features

OpenJP3D adds capabilities not present in the legacy implementation:

| Feature | Description |
|---------|-------------|
| **HTJ2K block coder** | High-throughput block coding (Part 15) for faster encode/decode |
| **JPIP 3-D streaming** | Interactive volumetric data access (Part 9) |
| **SIMD optimisation** | SSE4.1, AVX2, and NEON accelerated DWT |
| **Multi-threading** | Optional OpenMP parallelism at the tile level |
| **CLI tools** | `opj_jp3d_compress`, `opj_jp3d_decompress`, `opj_jp3d_dump`, `opj_jp3d_transcode`, `opj_jpip3d_server` |
| **Comprehensive tests** | CTest suite with sanitizer support |

## Codestream Compatibility

OpenJP3D generates codestreams conforming to ISO/IEC 15444-10. Codestreams
produced by the legacy OpenJPEG JP3D implementation are **not** directly
compatible — the legacy code used a non-standard codestream format with
incomplete marker segment support.

To migrate existing data, re-encode the raw volumes using OpenJP3D.

## Migration Checklist

1. Replace legacy headers with `#include "openjp3d.h"`.
2. Update function calls to use the `opj_jp3d_` prefix (see table above).
3. Replace `opj_cparameters_t` with `opj_jp3d_enc_params_t`.
4. Add an error callback or pass `NULL` for default behaviour.
5. Update memory management to use `opj_jp3d_malloc` / `opj_jp3d_free`.
6. Update build system to link against `openjp3d` (and `openjpip3d` if using
   JPIP).
7. Re-encode any existing codestreams with the OpenJP3D encoder.
