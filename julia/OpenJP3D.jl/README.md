# OpenJP3D.jl — Julia Bindings

[![License: BSD-2-Clause](https://img.shields.io/badge/License-BSD%202--Clause-blue.svg)](../../LICENSE)

Julia bindings for the [OpenJP3D](https://github.com/Raster-Lab/OpenJP3D)
volumetric codec library.  Pure `ccall`-based — no compilation required.

## Requirements

- Julia ≥ 1.6
- The OpenJP3D shared library (`libopenjp3d.so` / `.dylib` / `.dll`) built
  with `-DBUILD_SHARED_LIBS=ON`

## Installation

```julia
# From the repository root
julia> using Pkg; Pkg.develop(path="julia/OpenJP3D.jl")
```

Or set the `LOAD_PATH` manually:

```julia
push!(LOAD_PATH, "/path/to/OpenJP3D/julia/OpenJP3D.jl/src")
using OpenJP3D
```

## Library search order

The package looks for the shared library in this order:

1. `OPENJP3D_LIBRARY` environment variable — set to the full path.
2. A `lib/` subdirectory relative to the package source.
3. `Libdl.find_library("libopenjp3d")` — searches `LD_LIBRARY_PATH` / `DYLD_LIBRARY_PATH` / `PATH`.
4. Bare name `libopenjp3d` — lets the OS linker search at `ccall` time.

## Quick start

```julia
using OpenJP3D

# Lossless round-trip (UInt8, Int8, UInt16, Int16, Int32 supported)
vol = rand(UInt8, 16, 32, 32)          # (D, H, W) array
cs  = encode(vol)                       # → Vector{UInt8} codestream
dec = decode(cs)                        # → Array{UInt8,3}
@assert dec == vol

# Multi-component volume (D, H, W, C)
rgb = rand(UInt8, 8, 16, 16, 3)
cs  = encode(rgb)
out = decode(cs)
@assert out == rgb

# Lossy encoding with 9/7 filter
params = EncodeParams(filter=FILTER_97, target_rate=2.0f0)
cs_lossy = encode(vol, params)
dec_lossy = decode(cs_lossy)

# HTJ2K high-throughput mode
params_ht = EncodeParams(use_htj2k=USE_HTJ2K)
cs_ht = encode(vol, params_ht)
dec_ht = decode(cs_ht)
@assert dec_ht == vol

# Transcode EBCOT → HTJ2K
cs_ebcot = encode(vol)
cs_ht2   = transcode_to_ht(cs_ebcot)
@assert decode(cs_ht2) == vol

# Message callback
msgs = String[]
encode(vol; on_message=(level, msg) -> push!(msgs, msg))

# Version
println(get_version())  # e.g. "1.0.0"
```

## API reference

### `get_version() → String`

Returns the native library version string (e.g. `"1.0.0"`).

### `encode(volume [, params]; on_message=nothing) → Vector{UInt8}`

Encode a 3-D volume to a JP3D codestream.

- `volume`: `AbstractArray` of shape `(D, H, W)` or `(D, H, W, C)`.
  Supported element types: `UInt8`, `Int8`, `UInt16`, `Int16`, `Int32`.
- `params`: [`EncodeParams`](#encodeparams) (optional; lossless defaults).
- `on_message`: `(level::Int, msg::String) -> nothing` callback for codec
  messages (`level` ∈ `{MSG_INFO, MSG_WARNING, MSG_ERROR}`).

### `decode(data; verbose=false, on_message=nothing) → Array`

Decode a JP3D codestream to a Julia array.

- `data`: `Vector{UInt8}` codestream.
- Returns `Array{T,3}` `(D, H, W)` for single-component volumes or
  `Array{T,4}` `(D, H, W, C)` for multi-component volumes, where `T` is
  chosen from the encoded bit-depth and signedness.

### `transcode_to_ht(data; params=nothing, on_message=nothing) → Vector{UInt8}`

Losslessly transcode a JP3D codestream from EBCOT to HTJ2K block coding.

### `EncodeParams`

```julia
EncodeParams(;
    tile_width        = 0,      # 0 = whole volume
    tile_height       = 0,
    tile_depth        = 0,
    num_resolutions_x = 3,
    num_resolutions_y = 3,
    num_resolutions_z = 3,
    cblk_width        = 4,
    cblk_height       = 4,
    cblk_depth        = 4,
    filter            = FILTER_53,  # or FILTER_97
    num_layers        = 1,
    target_rate       = 0.0f0,  # 0 = lossless
    use_htj2k         = Int32(0),   # or USE_HTJ2K
    verbose           = false,
)
```

### Constants

| Constant    | Value   | Description                         |
|-------------|---------|-------------------------------------|
| `FILTER_53` | `0`     | Lossless 5/3 integer lifting filter |
| `FILTER_97` | `1`     | Lossy 9/7 float lifting filter      |
| `USE_HTJ2K` | `1`     | Enable HTJ2K block coder            |
| `CS_UNKNOWN`| `0`     | Unknown colour space                |
| `CS_SRGB`   | `1`     | sRGB                                |
| `CS_GRAY`   | `2`     | Greyscale                           |
| `CS_YUV`    | `3`     | YCbCr / YUV                         |
| `MSG_INFO`  | `0`     | Informational message level         |
| `MSG_WARNING`| `1`    | Warning message level               |
| `MSG_ERROR` | `2`     | Error message level                 |

## Testing

```bash
# Via Julia directly (requires OPENJP3D_LIBRARY to be set)
OPENJP3D_LIBRARY=/path/to/libopenjp3d.so \
  julia --project=julia/OpenJP3D.jl julia/OpenJP3D.jl/test/runtests.jl

# Via CMake / CTest
cmake -B build -DBUILD_SHARED_LIBS=ON -DBUILD_JULIA_BINDINGS=ON
cmake --build build
ctest --test-dir build -R test_julia_bindings -V
```

## Licence

BSD-2-Clause — see [LICENSE](../../LICENSE) for details.
