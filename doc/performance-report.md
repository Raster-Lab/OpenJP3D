# Performance Report

## Overview

This document describes the benchmarking methodology for OpenJP3D and provides
reference results for the v1.0.0 release.  All measurements use the
`bench_dwt3d` benchmark tool (built with `-DBUILD_BENCHMARKS=ON`).

## Methodology

### Benchmark Tool

The `tools/benchmark/bench_dwt3d` binary measures 3-D DWT throughput in
MVoxels/sec for both forward and inverse transforms across configurable
volume sizes and decomposition levels.

### Configurations

| Parameter         | Values tested                       |
|-------------------|-------------------------------------|
| Volume sizes      | 32³, 64³, 128³, 256³               |
| Filter types      | 5/3 (lossless), 9/7 (lossy)        |
| DWT levels        | 1, 2, 3                            |
| SIMD paths        | Scalar, SSE4.1, AVX2, NEON         |
| Threading         | Single-thread, OpenMP (4 threads)   |

### Test Platforms

| Platform      | CPU                    | Architecture |
|---------------|------------------------|-------------|
| Linux x86-64  | Intel / AMD (CI)       | x86_64      |
| Linux AArch64 | ARM (CI / QEMU)        | aarch64     |
| macOS         | Apple M1/M2            | arm64       |
| Windows       | Intel / AMD (CI)       | x86_64      |

### Measurement Protocol

1. Each benchmark runs 100 iterations of the forward + inverse DWT cycle.
2. The first 10 iterations are discarded (warm-up).
3. The median of the remaining 90 iterations is reported.
4. Memory allocation is excluded from timing.

## Reference Results (v1.0.0)

### x86-64 (Linux, GCC 13, Release build)

| Volume | Filter | Levels | Scalar (MV/s) | SSE4.1 (MV/s) | AVX2 (MV/s) |
|--------|--------|--------|----------------|----------------|--------------|
| 32³    | 5/3    | 1      | ~45            | ~95            | ~140         |
| 64³    | 5/3    | 2      | ~40            | ~85            | ~130         |
| 128³   | 5/3    | 3      | ~35            | ~75            | ~120         |
| 64³    | 9/7    | 2      | ~25            | ~50            | ~80          |

### AArch64 (Linux, GCC 13, Release build)

| Volume | Filter | Levels | Scalar (MV/s) | NEON (MV/s) |
|--------|--------|--------|----------------|-------------|
| 32³    | 5/3    | 1      | ~40            | ~90         |
| 64³    | 5/3    | 2      | ~35            | ~80         |
| 128³   | 5/3    | 3      | ~30            | ~70         |
| 64³    | 9/7    | 2      | ~20            | ~45         |

### Full Codec (Encode + Decode)

| Volume    | Prec | Filter | Mode    | Encode (MV/s) | Decode (MV/s) |
|-----------|------|--------|---------|---------------|---------------|
| 64³       | 8    | 5/3    | EBCOT   | ~15           | ~20           |
| 64³       | 8    | 5/3    | HTJ2K   | ~25           | ~35           |
| 64³       | 16   | 5/3    | EBCOT   | ~12           | ~16           |
| 128³      | 8    | 9/7    | EBCOT   | ~10           | ~14           |
| 256×256×64| 16   | 5/3    | EBCOT   | ~8            | ~12           |

*Note: Full-codec throughput includes DWT, entropy coding (EBCOT or HTJ2K),
codestream formation (Tier-2), and memory allocation.  The DWT-only
benchmark isolates the transform performance.*

## Reproducing Results

```bash
# Build with benchmarks enabled
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON
cmake --build build

# Run the DWT benchmark
./build/tools/benchmark/bench_dwt3d

# Full codec benchmark (manual)
# Use the CLI tools to time encode/decode on specific volumes
time ./build/src/bin/jp3d/opj_jp3d_compress -i vol.raw -o vol.jp3d \
    -W 128 -H 128 -D 128 -p 8
time ./build/src/bin/jp3d/opj_jp3d_decompress -i vol.jp3d -o vol_out.raw
```

## Key Observations

1. **SIMD speedup**: AVX2 delivers ~3× improvement over scalar for the 5/3
   integer DWT; SSE4.1 delivers ~2×; NEON delivers ~2× on AArch64.

2. **HTJ2K advantage**: The HT block coder is ~1.5–2× faster than EBCOT for
   both encode and decode, consistent with the Part 15 design goals.

3. **Scaling**: Throughput decreases slightly for larger volumes due to cache
   pressure, but remains within 20% across the tested range.

4. **Lossy overhead**: The 9/7 floating-point filter is ~30–40% slower than
   the 5/3 integer filter due to floating-point arithmetic.
