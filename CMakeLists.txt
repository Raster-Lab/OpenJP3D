# Copyright (c) 2024-2026, OpenJP3D Contributors
# All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause

cmake_minimum_required(VERSION 3.14)

project(OpenJP3D
    VERSION 1.0.0
    DESCRIPTION "JPEG 2000 Part 10 (JP3D) volumetric codec"
    HOMEPAGE_URL "https://github.com/Raster-Lab/OpenJP3D"
    LANGUAGES C
)

# ---------------------------------------------------------------------------
# Build options
# ---------------------------------------------------------------------------
option(BUILD_JP3D       "Build the core JP3D codec library"      ON)
option(BUILD_HTJ2K_3D   "Build the HTJ2K 3-D block coder"       OFF)
option(BUILD_JPIP_3D    "Build the JPIP 3-D streaming library"   ON)
option(BUILD_CLI_TOOLS  "Build command-line utilities"           ON)
option(BUILD_DOC        "Build Doxygen documentation"            OFF)
option(BUILD_TESTING    "Build and enable CTest test suite"      ON)
option(BUILD_SHARED_LIBS "Build shared libraries"                OFF)
option(ENABLE_OPENMP    "Enable OpenMP multi-threading"          OFF)
option(BUILD_BENCHMARKS "Build the benchmark suite"              OFF)
option(BUILD_DOC_EXAMPLES "Build documentation example programs" OFF)
option(BUILD_FUZZ      "Build libFuzzer harnesses (Clang only)" OFF)
option(BUILD_GUI_TOOLS "Build interactive GUI test application (Dear ImGui + SDL2)" OFF)
option(BUILD_PYTHON_BINDINGS "Register Python binding tests (requires Python 3 + pytest + NumPy)" OFF)
option(BUILD_JULIA_BINDINGS "Register Julia binding tests (requires Julia ≥ 1.6)" OFF)

# ---------------------------------------------------------------------------
# Global compiler settings
# ---------------------------------------------------------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Export compile_commands.json for clang-tidy / editors
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# libm is required on Unix but is part of the CRT on MSVC.
find_library(MATH_LIBRARY m)

# ---------------------------------------------------------------------------
# Subdirectories
# ---------------------------------------------------------------------------
if(BUILD_JP3D)
    add_subdirectory(src/lib/openjp3d)
endif()

if(BUILD_JPIP_3D)
    add_subdirectory(src/lib/openjpip3d)
endif()

if(BUILD_CLI_TOOLS)
    add_subdirectory(src/bin/jp3d)
endif()

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

if(BUILD_DOC)
    add_subdirectory(doc)
endif()

if(BUILD_DOC_EXAMPLES)
    add_subdirectory(doc/examples)
endif()

if(BUILD_BENCHMARKS)
    add_subdirectory(tools/benchmark)
endif()

if(BUILD_FUZZ)
    add_subdirectory(tools/fuzz)
endif()

if(BUILD_GUI_TOOLS)
    add_subdirectory(src/bin/jp3d/gui)
endif()

if(BUILD_PYTHON_BINDINGS)
    add_subdirectory(python)
endif()

if(BUILD_JULIA_BINDINGS)
    add_subdirectory(julia)
endif()

# ---------------------------------------------------------------------------
# Install targets
# ---------------------------------------------------------------------------
include(GNUInstallDirs)

if(BUILD_JP3D)
    install(TARGETS openjp3d
        EXPORT openjp3d-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

if(BUILD_JPIP_3D)
    install(TARGETS openjpip3d
        EXPORT openjp3d-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

if(BUILD_CLI_TOOLS)
    set(_CLI_TARGETS opj_jp3d_compress opj_jp3d_decompress
        opj_jp3d_dump opj_jp3d_transcode)
    if(BUILD_JPIP_3D)
        list(APPEND _CLI_TARGETS opj_jpip3d_server)
    endif()
    install(TARGETS ${_CLI_TARGETS}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

# CMake package config for find_package(OpenJP3D)
if(BUILD_JP3D)
    install(EXPORT openjp3d-targets
        FILE OpenJP3DTargets.cmake
        NAMESPACE OpenJP3D::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/OpenJP3D
    )

    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/OpenJP3DConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )
    configure_file(cmake/OpenJP3DConfig.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/OpenJP3DConfig.cmake" @ONLY)
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/OpenJP3DConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/OpenJP3DConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/OpenJP3D
    )
endif()

# pkg-config
if(BUILD_JP3D)
    configure_file(cmake/openjp3d.pc.in
        "${CMAKE_CURRENT_BINARY_DIR}/openjp3d.pc" @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/openjp3d.pc"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
endif()

# ---------------------------------------------------------------------------
# CPack source / binary packaging
# ---------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME "openjp3d")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
set(CPACK_PACKAGE_VENDOR "OpenJP3D Contributors")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_SOURCE_GENERATOR "TGZ;TXZ")
set(CPACK_SOURCE_IGNORE_FILES
    "/\\\\.git/"
    "/build.*/"
    "/\\\\.github/"
    "/\\\\.vscode/"
    "/\\\\.idea/"
)
include(CPack)
