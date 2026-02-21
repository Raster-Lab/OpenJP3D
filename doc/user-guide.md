# OpenJP3D User Guide

## Introduction

OpenJP3D is a C library implementing JPEG 2000 Part 10 (JP3D) volumetric
compression (ISO/IEC 15444-10). It provides lossless and lossy compression
of 3-D image volumes, with optional HTJ2K high-throughput block coding and
JPIP interactive streaming.

Target applications include medical imaging (CT/MRI), geospatial/satellite
imagery, and microscopy.

## Building from Source

### Prerequisites

- CMake ≥ 3.14
- C11-compatible compiler (GCC ≥ 7, Clang ≥ 6, MSVC ≥ 2019)
- (Optional) Doxygen ≥ 1.9 for API documentation

### Quick Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_JP3D` | ON | Core JP3D codec library |
| `BUILD_HTJ2K_3D` | OFF | HTJ2K 3-D block coder |
| `BUILD_JPIP_3D` | ON | JPIP 3-D streaming library |
| `BUILD_CLI_TOOLS` | ON | Command-line utilities |
| `BUILD_DOC` | OFF | Doxygen API documentation |
| `BUILD_TESTING` | ON | CTest test suite |
| `BUILD_SHARED_LIBS` | OFF | Build shared libraries |
| `ENABLE_OPENMP` | OFF | OpenMP multi-threading |
| `BUILD_BENCHMARKS` | OFF | Performance benchmark suite |

### Full-Featured Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_CLI_TOOLS=ON \
      -DBUILD_JPIP_3D=ON   \
      -DENABLE_OPENMP=ON
cmake --build build
```

### Installation

```bash
cmake --install build --prefix /usr/local
```

## Basic Usage — C Library API

### Encoding a Volume

```c
#include "openjp3d.h"

/* Create a 256×256×64 greyscale volume, 8-bit unsigned */
opj_volume_t *vol = opj_jp3d_create_volume(1, 256, 256, 64, 8, 0);

/* Fill sample data: data[z * w * h + y * w + x] */
for (uint32_t z = 0; z < 64; z++)
    for (uint32_t y = 0; y < 256; y++)
        for (uint32_t x = 0; x < 256; x++)
            vol->comps[0].data[z * 256 * 256 + y * 256 + x] = (int32_t)(x ^ y ^ z);

/* Set encoder parameters */
opj_jp3d_enc_params_t params;
opj_jp3d_set_default_encoder_parameters(&params);
params.filter = OPJ_JP3D_FILTER_53;  /* Lossless */

/* Encode */
uint8_t *codestream = NULL;
size_t   cs_size    = 0;
opj_jp3d_encode(vol, &params, &codestream, &cs_size, NULL, NULL);

/* Use codestream... */

opj_jp3d_free(codestream);
opj_jp3d_destroy_volume(vol);
```

### Decoding a Codestream

```c
#include "openjp3d.h"

/* Decode from buffer */
opj_volume_t *vol = opj_jp3d_decode(codestream, cs_size, NULL, NULL, NULL);

/* Access decoded samples */
int32_t sample = vol->comps[0].data[z * vol->comps[0].w * vol->comps[0].h
                                    + y * vol->comps[0].w + x];

opj_jp3d_destroy_volume(vol);
```

### Error Handling

```c
void my_handler(opj_jp3d_msg_level_t level,
                const char *msg, void *data) {
    const char *prefix = "INFO";
    if (level == OPJ_JP3D_MSG_WARNING) prefix = "WARN";
    if (level == OPJ_JP3D_MSG_ERROR)   prefix = "ERROR";
    fprintf(stderr, "[%s] %s\n", prefix, msg);
}

/* Pass callback to encode or decode */
opj_jp3d_encode(vol, &params, &out, &size, my_handler, NULL);
```

## Command-Line Tools

### opj_jp3d_compress

Encode a raw binary volume into a JP3D codestream:

```bash
opj_jp3d_compress -i volume.raw -o volume.jp3d \
    -W 256 -H 256 -D 64 \
    -p 8 -f 53 -n 3,3,3
```

**Options:**

| Flag | Description |
|------|-------------|
| `-i <file>` | Input raw volume (required) |
| `-o <file>` | Output JP3D codestream (required) |
| `-W <width>` | Volume width (required) |
| `-H <height>` | Volume height (required) |
| `-D <depth>` | Volume depth (required) |
| `-p <bits>` | Bit depth per sample (default: 8) |
| `-s` | Signed samples |
| `-c <n>` | Number of components (default: 1) |
| `-t <w,h,d>` | Tile size |
| `-n <rx,ry,rz>` | Decomposition levels per axis |
| `-r <rate>` | Target bits/sample (0 = lossless) |
| `-f 53|97` | Filter: 53 (lossless) or 97 (lossy) |
| `-H2K` | Enable HTJ2K block coder |
| `-v` | Verbose output |

### opj_jp3d_decompress

Decode a JP3D codestream into a raw binary volume:

```bash
opj_jp3d_decompress -i volume.jp3d -o volume.raw
```

### opj_jp3d_dump

Inspect a JP3D codestream and print marker information:

```bash
opj_jp3d_dump -i volume.jp3d
```

### opj_jp3d_transcode

Transcode a JP3D codestream from EBCOT to HTJ2K:

```bash
opj_jp3d_transcode -i ebcot.jp3d -o htj2k.jp3d
```

### opj_jpip3d_server

Standalone JPIP server for JP3D datasets (reads queries from stdin):

```bash
echo "dataset=vol&fsiz3d=256,256,64&roff3d=0,0,0&rsiz3d=64,64,16" | \
    opj_jpip3d_server
```

## Advanced Options

### HTJ2K Mode

Enable the high-throughput block coder for faster encode/decode at the cost
of slightly larger codestreams:

```c
params.use_htj2k = OPJ_JP3D_USE_HTJ2K;
```

Or from the command line:

```bash
opj_jp3d_compress -i vol.raw -o vol.jp3d -W 256 -H 256 -D 64 -H2K
```

### Lossy Compression

Use the 9/7 float filter with a target bit-rate:

```c
params.filter = OPJ_JP3D_FILTER_97;
params.target_rate = 2.0f;  /* bits per sample */
```

Or from the command line:

```bash
opj_jp3d_compress -i vol.raw -o vol.jp3d \
    -W 256 -H 256 -D 64 -f 97 -r 2.0
```

### Tiling

Split large volumes into tiles for parallel processing and memory
efficiency:

```c
params.tile_width  = 64;
params.tile_height = 64;
params.tile_depth  = 16;
```

### Multi-Component Volumes

Encode volumes with multiple components (e.g., RGB or multi-channel
microscopy):

```c
opj_volume_t *vol = opj_jp3d_create_volume(
    3,        /* 3 components */
    256, 256, 64,
    16,       /* 16-bit */
    0         /* unsigned */
);
/* Fill vol->comps[0], vol->comps[1], vol->comps[2] */
```

### JPIP Streaming

Request a sub-volume from a JPIP server:

```c
#include "openjpip3d.h"

opj_jpip3d_server_t *srv = opj_jpip3d_server_create();
opj_jpip3d_server_load_dataset_mem(srv, "vol", cs_data, cs_size);

opj_jpip3d_session_t *sess = opj_jpip3d_session_open(srv);
opj_jpip3d_request_t req = {0};
snprintf(req.dataset, sizeof(req.dataset), "vol");
req.fsiz3d[0] = 256; req.fsiz3d[1] = 256; req.fsiz3d[2] = 64;
req.roff3d[0] = 0;   req.roff3d[1] = 0;   req.roff3d[2] = 0;
req.rsiz3d[0] = 64;  req.rsiz3d[1] = 64;  req.rsiz3d[2] = 16;

opj_volume_t *sub = opj_jpip3d_session_receive_volume(sess, &req);
/* Use sub-volume... */

opj_jp3d_destroy_volume(sub);
opj_jpip3d_session_close(sess);
opj_jpip3d_server_destroy(srv);
```

## Architecture Overview

See [doc/architecture.md](architecture.md) for a detailed description of
the internal module structure, data flow, and extension points.

## Further Reading

- [INSTALL.md](../INSTALL.md) — Detailed build instructions
- [CODING_STYLE.md](../CODING_STYLE.md) — Coding conventions
- [CONTRIBUTING.md](../CONTRIBUTING.md) — How to contribute
- [doc/migration-from-legacy.md](migration-from-legacy.md) — Migration
  from legacy OpenJPEG JP3D
- [doc/architecture.md](architecture.md) — Internal architecture
