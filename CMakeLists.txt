# Copyright (c) 2024-2026, OpenJP3D Contributors
# All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause

cmake_minimum_required(VERSION 3.14)

project(OpenJP3D
    VERSION 0.1.0
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

# ---------------------------------------------------------------------------
# Global compiler settings
# ---------------------------------------------------------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Export compile_commands.json for clang-tidy / editors
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
