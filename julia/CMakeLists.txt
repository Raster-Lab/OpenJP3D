# Copyright (c) 2024-2026, OpenJP3D Contributors
# All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause

# ---------------------------------------------------------------------------
# Julia bindings CTest registration
#
# This CMakeLists.txt does *not* compile anything — the Julia package is
# pure ccall.  It optionally registers a CTest target that runs the Julia
# test suite (if Julia ≥ 1.6 is available).
# ---------------------------------------------------------------------------

find_program(Julia_EXECUTABLE julia)

if(NOT Julia_EXECUTABLE)
    message(STATUS "Julia executable not found — skipping Julia binding tests")
    return()
endif()

# Verify Julia version ≥ 1.6
execute_process(
    COMMAND ${Julia_EXECUTABLE} --version
    OUTPUT_VARIABLE _julia_ver_out
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _julia_ver_rc
)

if(NOT _julia_ver_rc EQUAL 0)
    message(STATUS "Could not determine Julia version — skipping Julia binding tests")
    return()
endif()

# Parse major.minor from "julia version X.Y.Z"
string(REGEX MATCH "([0-9]+)\\.([0-9]+)" _jver "${_julia_ver_out}")
set(_jmaj "${CMAKE_MATCH_1}")
set(_jmin "${CMAKE_MATCH_2}")

if(_jmaj LESS 1 OR (_jmaj EQUAL 1 AND _jmin LESS 6))
    message(STATUS
        "Julia ${_julia_ver_out} found but ≥ 1.6 is required — "
        "skipping Julia binding tests")
    return()
endif()

message(STATUS "Julia ${_julia_ver_out} found at ${Julia_EXECUTABLE}")

# ---------------------------------------------------------------------------
# Require a shared library build
# ---------------------------------------------------------------------------
if(NOT BUILD_SHARED_LIBS)
    message(STATUS
        "BUILD_SHARED_LIBS is OFF — Julia ccall bindings require a shared "
        "library.  Re-configure with -DBUILD_SHARED_LIBS=ON to enable them.")
    return()
endif()

# ---------------------------------------------------------------------------
# CTest target: run Julia test suite
# ---------------------------------------------------------------------------
set(_jl_pkg_dir  "${CMAKE_SOURCE_DIR}/julia/OpenJP3D.jl")
set(_jl_test_dir "${_jl_pkg_dir}/test")

add_test(
    NAME test_julia_bindings
    COMMAND ${Julia_EXECUTABLE}
            --project=${_jl_pkg_dir}
            ${_jl_test_dir}/runtests.jl
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

set_tests_properties(test_julia_bindings PROPERTIES
    ENVIRONMENT
        "OPENJP3D_LIBRARY=$<TARGET_FILE:openjp3d>"
    TIMEOUT 120
)

message(STATUS "Julia binding tests registered (test_julia_bindings)")
