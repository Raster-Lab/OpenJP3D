# GUI Toolkit Selection Rationale

## Decision

**Dear ImGui** (v1.91.x) with **SDL2** backend and **OpenGL 3.3** renderer.

## Criteria

| Criterion | Weight | Dear ImGui + SDL2 | Qt 6 | GTK 4 | wxWidgets |
|-----------|--------|-------------------|-------|--------|-----------|
| Dependency footprint | High | ★★★★★ Header-only, ~200 KB | ★★ ~50 MB | ★★★ ~20 MB | ★★★ ~15 MB |
| Cross-platform | High | ★★★★★ Linux/macOS/Windows | ★★★★★ | ★★★★ | ★★★★ |
| C/C++ compatibility | High | ★★★★ Minimal C++11 | ★★★ Heavy C++ | ★★★★★ C (GObject) | ★★★ C++ |
| Ease of embedding | High | ★★★★★ Drop-in files | ★★ Complex build | ★★★ pkg-config | ★★★ |
| GPU rendering support | Med | ★★★★★ Built-in OpenGL/Vulkan | ★★★★ QOpenGLWidget | ★★★ GtkGLArea | ★★★ |
| Docking / panels | Med | ★★★★★ imgui_dock branch (mainline since 1.89) | ★★★★★ QDockWidget | ★★★ | ★★★★ |
| Licence compatibility | High | ★★★★★ MIT | ★★★ LGPL/Commercial | ★★★★ LGPL | ★★★★★ wxWindows |
| Learning curve | Low | ★★★★ Immediate-mode, simple API | ★★★ Signal/slot, MOC | ★★★ GObject | ★★★ |

## Rationale

1. **Minimal dependency footprint** — Dear ImGui is distributed as a handful of
   source files (~200 KB) that compile directly into the application. SDL2 is
   widely available on all target platforms. This keeps `BUILD_GUI_TOOLS`
   lightweight and avoids pulling in a multi-megabyte framework.

2. **Immediate-mode paradigm** — Dear ImGui uses an immediate-mode rendering
   model that maps naturally to OpenJP3D's use-case: display diagnostic panels,
   parameter controls, and image viewports. There is no widget tree to maintain,
   reducing boilerplate significantly.

3. **Built-in docking** — Since version 1.89, Dear ImGui ships with native
   docking support in the main branch, satisfying the requirement for resizable
   and dockable panels without additional libraries.

4. **GPU integration** — Dear ImGui renders through OpenGL (or Vulkan), making
   it straightforward to composite the GUI with GPU-accelerated slice and volume
   rendering in the same context.

5. **Licence compatibility** — Dear ImGui (MIT) and SDL2 (zlib) are both
   permissively licensed, fully compatible with OpenJP3D's BSD-2-Clause licence.

6. **Language fit** — While Dear ImGui is written in C++, it requires only
   minimal C++11 features. The GUI application is the only component that uses
   C++; the core OpenJP3D library remains pure C11. The `BUILD_GUI_TOOLS`
   option (default OFF) conditionally enables C++ compilation so the standard
   build is unaffected.

## Alternatives Considered

- **Qt 6**: Powerful but heavyweight; LGPL licence requires dynamic linking or a
  commercial licence; adds significant build complexity.
- **GTK 4**: Good C API but weaker Windows/macOS support; Wayland-centric
  direction may limit portability.
- **wxWidgets**: Mature cross-platform toolkit, but heavier than Dear ImGui for
  the diagnostic/testing use-case targeted here.

## Dependencies

| Dependency | Version | Source | Licence |
|------------|---------|--------|---------|
| Dear ImGui | docking branch | FetchContent (GitHub) | MIT |
| SDL2 | ≥ 2.28 | System package or FetchContent | zlib |
| OpenGL | 3.3+ | System | — |

## Build Integration

```cmake
option(BUILD_GUI_TOOLS "Build the interactive GUI test application" OFF)
```

When `BUILD_GUI_TOOLS=ON`:
- CMake enables C++ (`enable_language(CXX)`)
- Dear ImGui is fetched via `FetchContent` if not found locally
- SDL2 is located via `find_package(SDL2)` (system) or `FetchContent` (fallback)
- The `opj_jp3d_gui` target is built under `src/bin/jp3d/gui/`

The core library build (`BUILD_JP3D`, `BUILD_CLI_TOOLS`, etc.) is completely
unaffected by this option.
