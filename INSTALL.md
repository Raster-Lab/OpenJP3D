# Building OpenJP3D

## Prerequisites

- CMake ≥ 3.14
- C11-compatible compiler (GCC ≥ 7, Clang ≥ 6, MSVC ≥ 2019)
- (Optional) Doxygen ≥ 1.9 for API documentation

## Quick Start

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

## CMake Options

| Option            | Default | Description                          |
|-------------------|---------|--------------------------------------|
| `BUILD_JP3D`      | ON      | Build the core JP3D codec library    |
| `BUILD_HTJ2K_3D`  | OFF     | Build the HTJ2K 3-D block coder     |
| `BUILD_JPIP_3D`   | ON      | Build the JPIP 3-D streaming library |
| `BUILD_CLI_TOOLS` | ON      | Build command-line utilities         |
| `BUILD_DOC`       | OFF     | Build Doxygen documentation          |
| `BUILD_DOC_EXAMPLES` | OFF  | Build documentation example programs |
| `BUILD_TESTING`   | ON      | Build and enable CTest test suite    |
| `BUILD_SHARED_LIBS`| OFF    | Build shared libraries               |
| `ENABLE_OPENMP`   | OFF     | Enable OpenMP multi-threading        |
| `BUILD_BENCHMARKS`| OFF     | Build the benchmark suite            |

## Installation

```bash
cmake --install build --prefix /usr/local
```
