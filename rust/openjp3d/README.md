# openjp3d — Rust Bindings

Rust bindings for the [OpenJP3D](https://github.com/Raster-Lab/OpenJP3D)
volumetric codec library (JPEG 2000 Part 10 / ISO/IEC 15444-10).

The native shared library is loaded at **runtime** via `dlopen`/`LoadLibrary`,
so this crate can be compiled and distributed without the `libopenjp3d` shared
library present at build time.

---

## Requirements

| Requirement | Version |
|-------------|---------|
| Rust        | 1.70+   |
| OpenJP3D shared library | 1.0.0+ |
| `BUILD_SHARED_LIBS=ON` | when building OpenJP3D |

---

## Library search order

When `load_lib(None)` is called (or the library path is not explicitly
specified), the crate searches in the following order:

1. `OPENJP3D_LIBRARY` environment variable (full path to the `.so`/`.dylib`/`.dll`).
2. A `lib/` subdirectory relative to the current executable.
3. OS default library search path (`libopenjp3d.so` / `libopenjp3d.dylib` / `openjp3d.dll`).

---

## Installation

Add to your `Cargo.toml`:

```toml
[dependencies]
openjp3d = { path = "/path/to/OpenJP3D/rust/openjp3d" }
```

---

## Quick start

### Lossless round-trip

```rust
use openjp3d::{load_lib, encode, decode};

fn main() {
    load_lib(None).expect("failed to load openjp3d shared library");

    // Create a 4×4×4 uint8 volume.
    let vol: Vec<i32> = (0..4*4*4).map(|i| (i % 256) as i32).collect();

    // Encode.
    let cs = encode(&vol, 4, 4, 4, 1, 8, false, None, None)
        .expect("encode failed");

    // Decode.
    let (dec, info) = decode(&cs, None, None).expect("decode failed");
    assert_eq!(vol, dec);
    println!("Decoded {}×{}×{} volume", info.w, info.h, info.d);
}
```

### Lossy encoding (9/7 filter)

```rust
use openjp3d::{load_lib, encode, decode, default_encode_params, FILTER_97};

load_lib(None).unwrap();
let vol: Vec<i32> = (0..16*16*8).map(|i| (i % 256) as i32).collect();

let mut p = default_encode_params().unwrap();
p.filter = FILTER_97;
p.target_rate = 1.0; // 1 bit per sample

let cs = encode(&vol, 16, 16, 8, 1, 8, false, Some(&p), None).unwrap();
let (dec, _) = decode(&cs, None, None).unwrap();
// dec is an approximation of vol
```

### HTJ2K high-throughput encoding

```rust
use openjp3d::{load_lib, encode, decode, default_encode_params, USE_HTJ2K};

load_lib(None).unwrap();
let vol: Vec<i32> = (0..8*8*4).map(|i| (i % 256) as i32).collect();

let mut p = default_encode_params().unwrap();
p.use_htj2k = USE_HTJ2K;

let cs = encode(&vol, 8, 8, 4, 1, 8, false, Some(&p), None).unwrap();
let (dec, _) = decode(&cs, None, None).unwrap();
assert_eq!(vol, dec); // lossless
```

### Transcode to HTJ2K

```rust
use openjp3d::{load_lib, encode, transcode_to_ht, decode};

load_lib(None).unwrap();
let vol: Vec<i32> = (0..8*8*4).map(|i| (i % 256) as i32).collect();

let cs  = encode(&vol, 8, 8, 4, 1, 8, false, None, None).unwrap();
let ht  = transcode_to_ht(&cs, None, None).unwrap();
let (dec, _) = decode(&ht, None, None).unwrap();
assert_eq!(vol, dec);
```

### Multi-component (RGB) volume

```rust
use openjp3d::{load_lib, encode, decode};

load_lib(None).unwrap();
// 3-channel 8×8×4 volume
let vol: Vec<i32> = (0..3*8*8*4).map(|i| (i % 256) as i32).collect();
let cs = encode(&vol, 8, 8, 4, 3, 8, false, None, None).unwrap();
let (dec, info) = decode(&cs, None, None).unwrap();
assert_eq!(info.num_comps, 3);
assert_eq!(vol, dec);
```

### Message callback

```rust
use openjp3d::{load_lib, encode, default_encode_params, MSG_INFO};

load_lib(None).unwrap();
let vol: Vec<i32> = (0..8*8*4).map(|i| (i % 256) as i32).collect();

let mut p = default_encode_params().unwrap();
p.verbose = 1;

let cb = |level: i32, msg: &str| {
    if level == MSG_INFO {
        println!("[info] {}", msg);
    }
};

let _ = encode(&vol, 8, 8, 4, 1, 8, false, Some(&p), Some(&cb));
```

---

## API reference

### Functions

| Function | Description |
|----------|-------------|
| `load_lib(path)` | Load the shared library. `path = None` uses the search order above. Idempotent. |
| `is_loaded()` | Returns `true` if the library was loaded successfully. |
| `get_version()` | Returns the library version string (e.g. `"1.0.0"`). |
| `default_encode_params()` | Returns library-default encoder parameters. |
| `encode(...)` | Encode a 3-D volume to a JP3D codestream. |
| `decode(...)` | Decode a JP3D codestream to sample data + `VolumeInfo`. |
| `transcode_to_ht(...)` | Transcode a JP3D codestream to HTJ2K block coding. |

### Types

| Type | Description |
|------|-------------|
| `EncodeParams` | Encoder configuration (tile size, DWT levels, filter, …). |
| `DecodeParams` | Decoder configuration (verbose flag). |
| `VolumeInfo` | Decoded volume metadata (dimensions, precision, colour space). |
| `MsgCallback` | `Box<dyn Fn(i32, &str) + Send + 'static>` |
| `Error` | Error type returned by all fallible functions. |

### Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `FILTER_53` | 0 | Lossless 5/3 wavelet filter |
| `FILTER_97` | 1 | Lossy 9/7 wavelet filter |
| `USE_HTJ2K` | 1 | Enable HTJ2K block coder |
| `CS_UNKNOWN` | 0 | Unknown colour space |
| `CS_SRGB` | 1 | sRGB |
| `CS_GRAY` | 2 | Greyscale |
| `CS_YUV` | 3 | YCbCr |
| `MSG_INFO` | 0 | Informational message level |
| `MSG_WARNING` | 1 | Warning message level |
| `MSG_ERROR` | 2 | Error message level |

---

## Running tests

```bash
# Build OpenJP3D as a shared library first:
cmake -B build -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run Rust tests:
OPENJP3D_LIBRARY=$(pwd)/build/src/lib/openjp3d/libopenjp3d.so \
    cargo test
```

---

## Licence

BSD-2-Clause — see [LICENSE](../../LICENSE).
