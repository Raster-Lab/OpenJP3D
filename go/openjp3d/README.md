# OpenJP3D Go Bindings

<!-- SPDX-License-Identifier: BSD-2-Clause -->

Go bindings for the [OpenJP3D](https://github.com/raster-lab/OpenJP3D)
volumetric codec library (JPEG 2000 Part 10 — JP3D).

The library is loaded at **runtime** via `dlopen`/`LoadLibrary` so the Go
package can be compiled without the OpenJP3D shared library present on the
build machine.

---

## Requirements

- Go 1.21 or later with CGo enabled (`CGO_ENABLED=1`).
- A C compiler (GCC, Clang, or MSVC) visible to the Go toolchain.
- The `openjp3d` shared library built with `BUILD_SHARED_LIBS=ON`:

  ```sh
  cmake -B build -DBUILD_SHARED_LIBS=ON -DBUILD_JP3D=ON
  cmake --build build
  ```

---

## Installation

```sh
# From the repository root:
cd go/openjp3d
go get github.com/raster-lab/openjp3d
```

---

## Library search order

When `LoadLib("")` is called (or automatically on first use), the package
searches for the shared library in this order:

1. **`OPENJP3D_LIBRARY` environment variable** — full path to the `.so` /
   `.dylib` / `.dll`.
2. **`lib/` directory next to the executable** — e.g.
   `<exe-dir>/lib/libopenjp3d.so`.
3. **OS default loader** — `libopenjp3d.so` on Linux,
   `libopenjp3d.dylib` on macOS, `openjp3d.dll` on Windows.

---

## Quick start

```go
package main

import (
    "fmt"
    "log"

    "github.com/raster-lab/openjp3d"
)

func main() {
    // Optional: load explicitly from a known path.
    if err := openjp3d.LoadLib("/usr/local/lib/libopenjp3d.so"); err != nil {
        log.Fatal(err)
    }

    version, _ := openjp3d.GetVersion()
    fmt.Println("OpenJP3D version:", version)

    // -----------------------------------------------------------------------
    // Lossless encoding
    // -----------------------------------------------------------------------
    w, h, d := uint32(32), uint32(32), uint32(16)
    vol := make([]int32, w*h*d)
    for i := range vol {
        vol[i] = int32(i % 256)
    }

    cs, err := openjp3d.Encode(vol, w, h, d, 1, 8, false, nil, nil)
    if err != nil {
        log.Fatal(err)
    }
    fmt.Printf("Encoded %d bytes\n", len(cs))

    // -----------------------------------------------------------------------
    // Decoding
    // -----------------------------------------------------------------------
    dec, info, err := openjp3d.Decode(cs, nil, nil)
    if err != nil {
        log.Fatal(err)
    }
    fmt.Printf("Decoded %dx%dx%d volume (%d components, %d-bit)\n",
        info.W, info.H, info.D, info.NumComps, info.Prec)
    _ = dec

    // -----------------------------------------------------------------------
    // Lossy encoding (9/7 wavelet, 2 bits/sample)
    // -----------------------------------------------------------------------
    p := openjp3d.DefaultEncodeParams()
    p.Filter = openjp3d.Filter97
    p.TargetRate = 2.0
    csLossy, err := openjp3d.Encode(vol, w, h, d, 1, 8, false, &p, nil)
    if err != nil {
        log.Fatal(err)
    }
    fmt.Printf("Lossy encoded %d bytes\n", len(csLossy))

    // -----------------------------------------------------------------------
    // HTJ2K lossless encoding
    // -----------------------------------------------------------------------
    pHT := openjp3d.DefaultEncodeParams()
    pHT.UseHTJ2K = openjp3d.UseHTJ2K
    csHT, err := openjp3d.Encode(vol, w, h, d, 1, 8, false, &pHT, nil)
    if err != nil {
        log.Fatal(err)
    }
    fmt.Printf("HTJ2K encoded %d bytes\n", len(csHT))

    // -----------------------------------------------------------------------
    // Transcode standard codestream → HTJ2K
    // -----------------------------------------------------------------------
    csTranscoded, err := openjp3d.TranscodeToHT(cs, nil, nil)
    if err != nil {
        log.Fatal(err)
    }
    fmt.Printf("Transcoded to HTJ2K: %d bytes\n", len(csTranscoded))

    // -----------------------------------------------------------------------
    // Message callback
    // -----------------------------------------------------------------------
    pVerbose := openjp3d.DefaultEncodeParams()
    pVerbose.Verbose = 1
    _, err = openjp3d.Encode(vol, w, h, d, 1, 8, false, &pVerbose,
        func(level int, msg string) {
            fmt.Printf("[%d] %s\n", level, msg)
        })
    if err != nil {
        log.Fatal(err)
    }
}
```

---

## API reference

### Functions

| Function | Description |
|----------|-------------|
| `LoadLib(path string) error` | Load the shared library. `path=""` triggers auto-search. |
| `IsLoaded() bool` | Report whether the library is loaded. |
| `GetVersion() (string, error)` | Return the native library version string. |
| `Encode(samples []int32, w, h, d, numComps, prec uint32, signed bool, params *EncodeParams, cb MsgCallback) ([]byte, error)` | Encode a 3-D volume to a JP3D codestream. |
| `Decode(data []byte, params *DecodeParams, cb MsgCallback) ([]int32, VolumeInfo, error)` | Decode a JP3D codestream. |
| `TranscodeToHT(src []byte, params *EncodeParams, cb MsgCallback) ([]byte, error)` | Transcode a JP3D codestream to HTJ2K block coding. |
| `DefaultEncodeParams() EncodeParams` | Return encoder defaults from the library. |

### Types

#### `EncodeParams`

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `TileWidth` | `uint32` | 0 (whole image) | Tile width. |
| `TileHeight` | `uint32` | 0 | Tile height. |
| `TileDepth` | `uint32` | 0 | Tile depth. |
| `NumResolutionsX/Y/Z` | `uint32` | 3 | DWT levels per axis. |
| `CblkWidth/Height/Depth` | `uint32` | 64 | Code-block dimensions. |
| `Filter` | `int32` | `Filter53` | `Filter53` (lossless) or `Filter97` (lossy). |
| `NumLayers` | `uint32` | 1 | Number of quality layers. |
| `TargetRate` | `float32` | 0.0 | Target bits/sample (0 = lossless). |
| `UseHTJ2K` | `int32` | 0 | Set to `UseHTJ2K` (1) to enable HT block coder. |
| `Verbose` | `int32` | 0 | Enable informational messages. |

#### `DecodeParams`

| Field | Type | Description |
|-------|------|-------------|
| `Verbose` | `int32` | Enable informational messages. |

#### `VolumeInfo`

| Field | Type | Description |
|-------|------|-------------|
| `NumComps` | `uint32` | Number of components. |
| `W`, `H`, `D` | `uint32` | Volume dimensions. |
| `Prec` | `uint32` | Bit depth. |
| `Signed` | `bool` | Whether samples are signed. |
| `ColorSpace` | `uint32` | Colour space constant. |

### Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `Filter53` | 0 | Lossless 5/3 integer wavelet. |
| `Filter97` | 1 | Lossy 9/7 floating-point wavelet. |
| `UseHTJ2K` | 1 | Enable HTJ2K block coder. |
| `CSUnknown` | 0 | Unknown colour space. |
| `CSSrgb` | 1 | sRGB. |
| `CSGray` | 2 | Greyscale. |
| `CSYUV` | 3 | YUV. |
| `MsgInfo` | 0 | Informational message level. |
| `MsgWarning` | 1 | Warning message level. |
| `MsgError` | 2 | Error message level. |

### Sample layout

`Encode` and `Decode` use a **component-major, depth-major, row-major**
layout:

```
index = comp * (D*H*W) + z * (H*W) + y * W + x
```

This matches the C library's internal layout, so no transposition is required.

---

## Running tests

```sh
export OPENJP3D_LIBRARY=/path/to/libopenjp3d.so
cd go/openjp3d
go test -v ./...
```

Tests are automatically skipped when `OPENJP3D_LIBRARY` is not set or when the
library cannot be loaded.

---

## Licence

BSD-2-Clause — see [LICENSE](../../LICENSE).
