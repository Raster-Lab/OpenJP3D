# Contributing to OpenJP3D

Thank you for your interest in contributing to OpenJP3D! This document explains
how to build the project, run tests, follow coding conventions, and submit
pull requests.

## Getting Started

### Prerequisites

- CMake ≥ 3.14
- C11-compatible compiler (GCC ≥ 7, Clang ≥ 6, MSVC ≥ 2019)
- (Optional) Doxygen ≥ 1.9 for API documentation

### Building from Source

```bash
git clone https://github.com/Raster-Lab/OpenJP3D.git
cd OpenJP3D

cmake -B build -DCMAKE_BUILD_TYPE=Debug \
      -DBUILD_TESTING=ON    \
      -DBUILD_CLI_TOOLS=ON  \
      -DBUILD_JPIP_3D=ON
cmake --build build
```

### Running Tests

```bash
ctest --test-dir build -V
```

All tests must pass before submitting a pull request. The test suite covers:

- Volume lifecycle and memory management
- 3-D DWT round-trips (5/3 and 9/7 filters)
- Encode → decode lossless round-trips
- HTJ2K block coder correctness
- JPIP 3-D request parsing, caching, and streaming
- SIMD correctness vs. scalar reference
- CLI tool end-to-end tests
- Parameter validation and error handling

### Sanitizer Builds

Run tests under AddressSanitizer and UndefinedBehaviorSanitizer to catch
memory errors:

```bash
cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
      -DBUILD_TESTING=ON -DBUILD_CLI_TOOLS=ON -DBUILD_JPIP_3D=ON
cmake --build build-asan
ctest --test-dir build-asan -V
```

## Coding Conventions

See [CODING_STYLE.md](CODING_STYLE.md) for the full style guide. Key points:

- **Language**: C11 (ISO/IEC 9899:2011). No compiler-specific extensions
  unless wrapped behind a portability macro.
- **Naming**: `opj_` prefix for all public symbols; `snake_case` for functions
  and variables; `UPPER_SNAKE_CASE` for macros and constants.
- **Formatting**: 4-space indentation, no tabs. Run `clang-format` before
  committing:
  ```bash
  clang-format -i src/lib/openjp3d/*.c src/lib/openjp3d/*.h
  ```
- **Documentation**: Doxygen-style comments (`/** ... */`) on all public types,
  functions, and macros with `@brief`, `@param`, and `@return` tags.
- **Licence**: Every source and header file must include the BSD-2-Clause
  licence header.

## Directory Structure

```
src/lib/openjp3d/    — Core JP3D codec library
src/lib/openjpip3d/  — JPIP 3-D streaming library
src/bin/jp3d/        — CLI tools (compress, decompress, dump, transcode, server)
tests/               — CTest-based test suites
doc/                 — Doxygen config, user guide, architecture docs, examples
thirdparty/          — Vendored or FetchContent-managed dependencies
cmake/               — CMake helper modules
tools/               — Benchmarks and development utilities
```

Place new files in the directory that matches this layout.

## Pull Request Workflow

1. **Fork** the repository and create a feature branch from `main`.
2. Make your changes, keeping each PR focused on a single logical change.
3. Add or update tests for your changes.
4. Ensure all tests pass and the build succeeds on your platform.
5. Run `clang-format` and `clang-tidy`:
   ```bash
   clang-format -i <changed-files>
   clang-tidy -p build <changed-files>
   ```
6. Update documentation if your change affects the public API, build options,
   or CLI tools:
   - Doxygen headers for API changes
   - `INSTALL.md` for build option changes
   - Man pages for CLI tool changes
   - `CHANGELOG.md` entry under `[Unreleased]`
7. Open a pull request with a clear description of the change and its
   motivation.

## Reporting Issues

- Use GitHub Issues for bug reports and feature requests.
- Include steps to reproduce, expected vs. actual behaviour, and platform
  details (OS, compiler, CMake version).
- For crash reports, include a sanitizer backtrace if possible.

## Licence

By contributing to OpenJP3D, you agree that your contributions will be
licensed under the [BSD-2-Clause](LICENSE) licence.
