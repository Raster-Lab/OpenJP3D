# openjp3d Python Package

Python bindings for the **OpenJP3D** JPEG 2000 Part 10 (JP3D) volumetric codec,
providing a NumPy-friendly API for encoding and decoding 3-D image volumes.

## Requirements

- Python ≥ 3.8
- The compiled `openjp3d` shared library (build with `BUILD_SHARED_LIBS=ON`)
- NumPy ≥ 1.20 (optional, required for `encode()` / `decode()`)

## Installation

```bash
# 1. Build the shared library
cmake -B build -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 2. Install the Python package (editable mode for development)
cd python
pip install -e ".[numpy]"

# 3. Point the loader at the built library (if not system-installed)
export OPENJP3D_LIBRARY=/path/to/build/src/lib/openjp3d/libopenjp3d.so
```

## Quick-start

```python
import numpy as np
import openjp3d

# Synthetic 16×32×32 uint8 volume (depth × height × width)
vol = np.random.randint(0, 256, (16, 32, 32), dtype=np.uint8)

# Lossless encode → JP3D bytes
codestream = openjp3d.encode(vol)
print(f"Encoded {vol.nbytes} bytes → {len(codestream)} bytes")

# Decode back
decoded = openjp3d.decode(codestream)
assert np.array_equal(vol, decoded), "Lossless round-trip failed!"
print("Round-trip OK")

# Check library version
print("Library version:", openjp3d.get_version())
```

## Lossy encoding

```python
from openjp3d import encode, decode, EncodeParams, FILTER_97

params = EncodeParams(
    filter=FILTER_97,
    target_rate=1.0,   # 1 bit/sample
)
codestream = encode(vol, params)
decoded = decode(codestream)
```

## HTJ2K mode

```python
from openjp3d import encode, decode, EncodeParams, USE_HTJ2K

params = EncodeParams(use_htj2k=USE_HTJ2K)
codestream = encode(vol, params)
decoded = decode(codestream)
```

## Transcoding

```python
from openjp3d import transcode_to_ht

ht_codestream = transcode_to_ht(ebcot_codestream)
```

## Library search order

1. `OPENJP3D_LIBRARY` environment variable (full path to the `.so`/`.dll`/`.dylib`)
2. Directory containing the `openjp3d` Python package
3. System library search path (`LD_LIBRARY_PATH`, `PATH`, etc.)

## Licence

BSD-2-Clause — see the top-level [LICENSE](../LICENSE) file.
