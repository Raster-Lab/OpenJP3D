# openjp3d — MATLAB/Octave Bindings

MATLAB and Octave bindings for the OpenJP3D volumetric codec.
Encode and decode 3-D image volumes using JPEG 2000 Part 10 (JP3D) directly
from MATLAB or Octave scripts.

## Requirements

| Tool | Version |
|------|---------|
| Octave | ≥ 6.0 (recommended for open-source use) |
| MATLAB | R2019b+ (optional) |
| OpenJP3D shared library | built with `-DBUILD_SHARED_LIBS=ON` |

## Installation

### Step 1 — Build the shared library

```bash
cmake -B build -DBUILD_SHARED_LIBS=ON -DBUILD_JP3D=ON
cmake --build build
```

### Step 2 — Compile the MEX file (Octave)

```bash
mkoctfile --mex -o openjp3d_mex \
    matlab/openjp3d/src/openjp3d_mex.c
```

Or with MATLAB:

```matlab
mex -output openjp3d_mex matlab/openjp3d/src/openjp3d_mex.c
```

### Step 3 — Add paths

In Octave/MATLAB:

```matlab
addpath('/path/to/OpenJP3D/matlab/openjp3d');   % EncodeParams class
addpath('/path/to/build');                        % openjp3d_mex.mex
```

### Step 4 — Point to the shared library

Either set the environment variable before starting Octave/MATLAB:

```bash
export OPENJP3D_LIBRARY=/path/to/build/src/lib/openjp3d/libopenjp3d.so
```

Or call `load_lib` explicitly in your script:

```matlab
openjp3d.load_lib('/path/to/libopenjp3d.so');
```

## Quick Start

```matlab
% 1. Create a test volume [D H W] = [16 64 64]
vol = int16(randi([-32768 32767], 16, 64, 64));

% 2. Encode (lossless by default)
cs = openjp3d.encode(vol);

% 3. Decode
[recovered, dtype] = openjp3d.decode(cs);
disp(dtype);                   % 'int16'
assert(isequal(recovered, int32(vol)));

% 4. Lossy encoding with the 9/7 filter
p  = EncodeParams('Filter', 1, 'TargetRate', 2.0);
cs2 = openjp3d.encode(vol, p);

% 5. HTJ2K transcode
cs_ht = openjp3d.transcode_to_ht(cs);
```

## API Reference

### `openjp3d.encode(volume [, params [, options]])`

Encode a 3-D volume to a JP3D codestream.

| Argument | Type | Description |
|----------|------|-------------|
| `volume` | numeric array `[D H W]` or `[D H W C]` | Input voxel data |
| `params` | `EncodeParams` | Encoder settings (optional; default = lossless) |
| `options` | struct | `Prec`, `Sgnd`, `OnMessage` fields |

Returns `uint8` row vector (JP3D codestream bytes).

### `[volume, dtype] = openjp3d.decode(data [, 'Verbose', v] [, 'OnMessage', cb])`

Decode a JP3D codestream.

| Return | Type | Description |
|--------|------|-------------|
| `volume` | `int32 [D H W]` or `[D H W C]` | Decoded voxels |
| `dtype` | `char` | Original type string: `'uint8'`, `'int8'`, `'uint16'`, `'int16'`, `'int32'` |

### `openjp3d.transcode_to_ht(data [, params] [, 'OnMessage', cb])`

Losslessly transcode a JP3D codestream from EBCOT to HTJ2K block coding.

### `openjp3d.get_version()`

Return the native library version string (e.g. `'1.0.0'`).

### `openjp3d.load_lib([path])`

Load the native shared library.  Called automatically on first API use.

## `EncodeParams` Class

```matlab
p = EncodeParams()                             % lossless defaults
p = EncodeParams('Filter', 1, 'TargetRate', 2) % lossy 9/7, 2 bps
p = EncodeParams('UseHTJ2K', 1)                % HTJ2K block coder
```

| Property | Default | Description |
|----------|---------|-------------|
| `TileWidth/Height/Depth` | 0 | Tile size (0 = whole volume) |
| `NumResolutionsX/Y/Z` | 3 | DWT decomposition levels per axis |
| `CblkWidth/Height/Depth` | 4 | Code-block size |
| `Filter` | 0 | 0 = 5/3 lossless, 1 = 9/7 lossy |
| `NumLayers` | 1 | Quality layers |
| `TargetRate` | 0 | Bits/sample (0 = lossless) |
| `UseHTJ2K` | 0 | 1 = HTJ2K high-throughput coder |
| `Verbose` | 0 | 1 = print codec messages |

## Library Search Order

1. `OPENJP3D_LIBRARY` environment variable.
2. `lib/` subdirectory next to the `+openjp3d/` package directory.
3. OS default loader search path (`LD_LIBRARY_PATH` / `DYLD_LIBRARY_PATH` /
   `PATH`).

## Running Tests

```bash
OPENJP3D_LIBRARY=/path/to/libopenjp3d.so \
    octave --no-gui \
        --path matlab/openjp3d \
        --path /path/to/build \
        tests/test_matlab.m
```

Or via CTest:

```bash
cmake -B build -DBUILD_MATLAB_BINDINGS=ON -DBUILD_SHARED_LIBS=ON
cmake --build build
ctest --build-dir build -R matlab
```

## Licence

BSD-2-Clause — see [LICENSE](../../LICENSE) for details.
