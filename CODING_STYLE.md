# OpenJP3D Coding Style

## Language

- All library and tool code is written in **C11** (ISO/IEC 9899:2011).
- Use only standard C11 features; avoid compiler-specific extensions unless
  wrapped behind a portability macro.

## Naming Conventions

| Element                    | Convention          | Example                                 |
|----------------------------|---------------------|-----------------------------------------|
| Public functions / types   | `opj_` prefix       | `opj_jp3d_encode`, `opj_volume_t`       |
| Module-specific symbols    | Extended prefix     | `opj_jp3d_`, `opj_jpip3d_`, `opj_htj2k3d_` |
| Functions, variables       | `snake_case`        | `opj_jp3d_set_default_encoder_parameters` |
| Macros and constants       | `UPPER_SNAKE_CASE`  | `OPJ_JP3D_USE_HTJ2K`                   |

## Formatting

- Indent with **4 spaces** (no tabs).
- Maximum line length: **100 characters**.
- Braces: Linux kernel style (opening brace on same line for functions, on new
  line for control structures — see `.clang-format` for details).
- Run `clang-format` before committing:

  ```bash
  clang-format -i src/lib/openjp3d/*.c src/lib/openjp3d/*.h
  ```

## Documentation

- Use **Doxygen**-style comments (`/** ... */`) for all public types, functions,
  and macros.
- Every public header must have complete Doxygen annotations.
- Write `@brief`, `@param`, and `@return` tags.

## Static Analysis

- Run `clang-tidy` to catch common issues:

  ```bash
  clang-tidy -p build src/lib/openjp3d/*.c
  ```

## Licence Header

Every source and header file must include the BSD-2-Clause licence header.
See `LICENSE` for the full text.
