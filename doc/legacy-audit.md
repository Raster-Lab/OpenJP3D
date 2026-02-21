# Legacy JP3D Code Audit

## Background

OpenJPEG versions ≤ 2.4.0 included an experimental JP3D implementation in
`src/lib/openjp3d/`. This code was removed in OpenJPEG 2.5.0 (May 2022) as
it was unmaintained and did not meet the project's quality standards.

## Assessment

| Aspect                | Finding |
|-----------------------|---------|
| **Code quality**      | The legacy code predates modern C practices and lacks consistent style. |
| **API design**        | Does not follow the current OpenJPEG 2.5.x public API conventions. |
| **3-D DWT**           | Basic separable 3-D DWT is present but lacks optimisation and boundary handling. |
| **Entropy coding**    | EBCOT 3-D extension is incomplete; rate control is missing. |
| **File format**       | JP3D box-based file format is partially implemented. |
| **Tests**             | No automated tests; only manual validation with a few sample volumes. |
| **Documentation**     | Minimal inline comments; no Doxygen annotations. |

## Recommendation

Implement the JP3D codec from scratch, referencing the legacy code and the
ISO/IEC 15444-10 specification for algorithmic guidance. This approach ensures:

1. Consistent API design with OpenJPEG 2.5.4.
2. Modern C11 code with proper error handling.
3. Comprehensive test coverage from the start.
4. Clean integration with the HTJ2K and JPIP extensions.

Specific algorithms (e.g., 3-D DWT lifting steps, EBCOT context modelling)
may be adapted from the legacy implementation after review and modernisation.
