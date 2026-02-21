# openjp3d — R Bindings for the OpenJP3D Volumetric Codec

<!-- Copyright (c) 2024-2026, OpenJP3D Contributors -->
<!-- SPDX-License-Identifier: BSD-2-Clause -->

R bindings for the [OpenJP3D](https://github.com/Raster-Lab/OpenJP3D)
volumetric codec library. Encode and decode 3-D image volumes using
**JPEG 2000 Part 10 (JP3D / ISO/IEC 15444-10)** directly from R integer arrays.

## Features

- **`encode(volume, params, prec, sgnd)`** — compress an R integer array to a
  JP3D codestream (`raw` vector)
- **`decode(data)`** — decompress a JP3D codestream (`raw` vector) back to an
  R integer array
- **`transcode_to_ht(data, params)`** — losslessly transcode a JP3D codestream
  from EBCOT to HTJ2K block coding
- **`get_version()`** — return the native library version string
- **`EncodeParams(...)`** — construct encoder parameters (tile size, DWT
  levels, filter, bit-rate, HTJ2K flag, etc.)
- Lossless and lossy compression modes
- Optional HTJ2K high-throughput block coding (JPEG 2000 Part 15)
- Supports 8, 16, and 32-bit integer volumes (signed and unsigned)
- Single- and multi-component `(D, H, W)` / `(D, H, W, C)` array layouts
- Message callback for codec errors/warnings/info

## Installation

```r
# From the repository root (requires the openjp3d shared library to have
# been built with -DBUILD_SHARED_LIBS=ON):
install.packages("r/openjp3d", repos = NULL, type = "source")

# Or using devtools:
devtools::install("r/openjp3d")
```

The package compiles a thin C wrapper during installation.  No separate Python
or Julia installation is required.

### Library search order

The package locates the openjp3d shared library at load time:

1. **`OPENJP3D_LIBRARY` environment variable** — full path to the shared
   library (highest priority, used by CTest).
2. **`lib/` directory next to the installed R package** — place
   `libopenjp3d.so` / `.dylib` / `.dll` there for a self-contained
   installation.
3. **OS default loader** — `libopenjp3d` on the system library path
   (`LD_LIBRARY_PATH`, `DYLD_LIBRARY_PATH`, or `PATH`).

## Quick Start

```r
library(openjp3d)

# Create a synthetic 3-D volume (D=8, H=16, W=16), uint8
vol <- array(as.integer(sample(0:255, 8*16*16, replace = TRUE)),
             dim = c(8L, 16L, 16L))

# Encode to a JP3D codestream (raw vector)
cs <- encode(vol, prec = 8L, sgnd = FALSE)
cat("Codestream size:", length(cs), "bytes\n")

# Decode back to an integer array
decoded <- decode(cs)
stopifnot(identical(decoded, vol))   # lossless by default
cat("Lossless round-trip: OK\n")
```

### Lossy encoding (9/7 filter)

```r
p  <- EncodeParams(filter = FILTER_97, target_rate = 1.5)
cs <- encode(vol, params = p, prec = 8L, sgnd = FALSE)
out <- decode(cs)
cat("Max error:", max(abs(as.integer(out) - as.integer(vol))), "\n")
```

### HTJ2K high-throughput mode

```r
p  <- EncodeParams(use_htj2k = USE_HTJ2K)
cs <- encode(vol, params = p, prec = 8L, sgnd = FALSE)
out <- decode(cs)
stopifnot(identical(out, vol))
```

### Transcode EBCOT → HTJ2K

```r
cs_ebcot <- encode(vol, prec = 8L, sgnd = FALSE)
cs_ht    <- transcode_to_ht(cs_ebcot)
out      <- decode(cs_ht)
stopifnot(identical(out, vol))
```

### Multi-component volume

```r
# (D, H, W, C) layout — 3-channel
vol3 <- array(as.integer(sample(0:255, 4*8*8*3, replace = TRUE)),
              dim = c(4L, 8L, 8L, 3L))
cs   <- encode(vol3, prec = 8L, sgnd = FALSE)
out  <- decode(cs)
stopifnot(identical(out, vol3))
```

### Message callback

```r
msgs <- character(0)
p    <- EncodeParams(verbose = TRUE)
cs   <- encode(vol, params = p, prec = 8L, sgnd = FALSE,
               on_message = function(level, msg) {
                   msgs <<- c(msgs, sprintf("[%d] %s", level, msg))
               })
cat(msgs, sep = "\n")
```

## API Reference

| Function | Description |
|---|---|
| `get_version()` | Returns native library version string. |
| `encode(volume, params, prec, sgnd, on_message)` | Encode array → `raw` JP3D codestream. |
| `decode(data, verbose, on_message)` | Decode `raw` codestream → integer array. |
| `transcode_to_ht(data, params, on_message)` | Transcode EBCOT → HTJ2K. |
| `EncodeParams(...)` | Construct encoder parameters. |

### EncodeParams fields

| Field | Default | Description |
|---|---|---|
| `tile_width`, `tile_height`, `tile_depth` | `0` | Tile dimensions (0 = whole volume). |
| `num_resolutions_x/y/z` | `3` | DWT decomposition levels per axis. |
| `cblk_width`, `cblk_height`, `cblk_depth` | `4` | Code-block dimensions. |
| `filter` | `FILTER_53` | `FILTER_53` (lossless) or `FILTER_97` (lossy). |
| `num_layers` | `1` | Number of quality layers. |
| `target_rate` | `0.0` | Target bits/sample (0 = lossless). |
| `use_htj2k` | `0` | Set to `USE_HTJ2K` to enable HT block coder. |
| `verbose` | `FALSE` | Enable codec info messages. |

## Building the Tests

```bash
cmake -B build -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=ON \
      -DBUILD_R_BINDINGS=ON
cmake --build build
ctest --test-dir build -R test_r_bindings -V
```

## Licence

BSD-2-Clause — see [LICENSE](../../LICENSE) for details.
