# SPDX-License-Identifier: BSD-2-Clause

# ---------------------------------------------------------------------------
# CPU / SIMD feature detection
# ---------------------------------------------------------------------------
include(CheckCCompilerFlag)

# SSE4.1
if(MSVC AND (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i[3-9]86"))
    # MSVC x64 supports SSE4.1 intrinsics without special flags
    set(OPJ_JP3D_HAVE_SSE41 ON)
else()
    check_c_compiler_flag("-msse4.1" COMPILER_HAS_SSE41)
    if(COMPILER_HAS_SSE41 AND (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i[3-9]86"))
        set(OPJ_JP3D_HAVE_SSE41 ON)
    endif()
endif()

# AVX2
if(MSVC AND (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64"))
    check_c_compiler_flag("/arch:AVX2" COMPILER_HAS_AVX2_MSVC)
    if(COMPILER_HAS_AVX2_MSVC)
        set(OPJ_JP3D_HAVE_AVX2 ON)
    endif()
else()
    check_c_compiler_flag("-mavx2" COMPILER_HAS_AVX2)
    if(COMPILER_HAS_AVX2 AND (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64"))
        set(OPJ_JP3D_HAVE_AVX2 ON)
    endif()
endif()

# NEON (AArch64 — always present on 64-bit Arm)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    set(OPJ_JP3D_HAVE_NEON ON)
endif()

# ---------------------------------------------------------------------------
# Source file lists
# ---------------------------------------------------------------------------
set(OPENJP3D_SOURCES
    openjp3d.c
    opj_mem.c
    opj_volume.c
    opj_dwt3d.c
    opj_t1_3d.c
    opj_ht3d.c
    opj_t2_3d.c
    opj_cs3d.c
    opj_raw_io.c
    opj_tcd3d.c
    opj_cpu.c
)

set(OPENJP3D_HEADERS
    openjp3d.h
    openjp3d_private.h
    opj_mem.h
    opj_volume.h
    opj_dwt3d.h
    opj_dwt3d_simd.h
    opj_t1_3d.h
    opj_ht3d.h
    opj_t2_3d.h
    opj_cs3d.h
    opj_raw_io.h
    opj_tcd3d.h
    opj_cpu.h
)

# SIMD source files compiled with per-file ISA flags
if(OPJ_JP3D_HAVE_SSE41)
    list(APPEND OPENJP3D_SOURCES opj_dwt3d_sse41.c)
endif()
if(OPJ_JP3D_HAVE_AVX2)
    list(APPEND OPENJP3D_SOURCES opj_dwt3d_avx2.c)
endif()
if(OPJ_JP3D_HAVE_NEON)
    list(APPEND OPENJP3D_SOURCES opj_dwt3d_neon.c)
endif()

add_library(openjp3d ${OPENJP3D_SOURCES} ${OPENJP3D_HEADERS})

target_include_directories(openjp3d
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

if(MATH_LIBRARY)
    target_link_libraries(openjp3d PRIVATE ${MATH_LIBRARY})
endif()

# ---------------------------------------------------------------------------
# SIMD compile definitions and per-file flags
# ---------------------------------------------------------------------------
if(OPJ_JP3D_HAVE_SSE41)
    target_compile_definitions(openjp3d PUBLIC OPJ_JP3D_HAVE_SSE41)
    if(NOT MSVC)
        set_source_files_properties(opj_dwt3d_sse41.c
            PROPERTIES COMPILE_FLAGS "-msse4.1")
    endif()
endif()

if(OPJ_JP3D_HAVE_AVX2)
    target_compile_definitions(openjp3d PUBLIC OPJ_JP3D_HAVE_AVX2)
    if(MSVC)
        set_source_files_properties(opj_dwt3d_avx2.c
            PROPERTIES COMPILE_FLAGS "/arch:AVX2")
    else()
        set_source_files_properties(opj_dwt3d_avx2.c
            PROPERTIES COMPILE_FLAGS "-mavx2")
    endif()
endif()

if(OPJ_JP3D_HAVE_NEON)
    target_compile_definitions(openjp3d PUBLIC OPJ_JP3D_HAVE_NEON)
    # No extra flags needed: NEON is always on for AArch64 targets.
endif()

# ---------------------------------------------------------------------------
# OpenMP (optional multi-threading)
# ---------------------------------------------------------------------------
if(ENABLE_OPENMP)
    find_package(OpenMP)
    if(OpenMP_C_FOUND)
        target_link_libraries(openjp3d PRIVATE OpenMP::OpenMP_C)
        target_compile_definitions(openjp3d PRIVATE _OPENMP)
        message(STATUS "OpenJP3D: OpenMP enabled")
    else()
        message(WARNING "OpenJP3D: ENABLE_OPENMP requested but OpenMP not found")
    endif()
endif()

# ---------------------------------------------------------------------------
# Shared library export macro
# ---------------------------------------------------------------------------
if(BUILD_SHARED_LIBS)
    target_compile_definitions(openjp3d PRIVATE OPJ_JP3D_BUILDING_DLL)
endif()

set_target_properties(openjp3d PROPERTIES
    VERSION   ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/openjp3d.h"
)
