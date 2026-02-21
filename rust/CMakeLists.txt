# Copyright (c) 2024-2026, OpenJP3D Contributors
# All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause

# ---------------------------------------------------------------------------
# Rust bindings CTest registration
#
# This CMakeLists.txt does *not* compile anything — all compilation is handled
# by the Rust toolchain (cargo).  It optionally registers a CTest target that
# runs the Rust test suite (if cargo is available and BUILD_SHARED_LIBS=ON).
# ---------------------------------------------------------------------------

find_program(Cargo_EXECUTABLE cargo)

if(NOT Cargo_EXECUTABLE)
    message(STATUS "cargo not found — skipping Rust binding tests")
    return()
endif()

# ---------------------------------------------------------------------------
# Verify Rust / cargo version (require 1.70+)
# ---------------------------------------------------------------------------
execute_process(
    COMMAND ${Cargo_EXECUTABLE} --version
    OUTPUT_VARIABLE _cargo_ver_out
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _cargo_ver_rc
)

if(NOT _cargo_ver_rc EQUAL 0)
    message(STATUS "Could not determine cargo version — skipping Rust binding tests")
    return()
endif()

message(STATUS "${_cargo_ver_out} found at ${Cargo_EXECUTABLE}")

# ---------------------------------------------------------------------------
# Require a shared library build
# ---------------------------------------------------------------------------
if(NOT BUILD_SHARED_LIBS)
    message(STATUS
        "BUILD_SHARED_LIBS is OFF — Rust bindings require a shared "
        "library.  Re-configure with -DBUILD_SHARED_LIBS=ON to enable them.")
    return()
endif()

# ---------------------------------------------------------------------------
# CTest target: run Rust test suite
# ---------------------------------------------------------------------------
set(_rust_pkg_dir "${CMAKE_SOURCE_DIR}/rust/openjp3d")

add_test(
    NAME test_rust_bindings
    COMMAND ${Cargo_EXECUTABLE} test --manifest-path "${_rust_pkg_dir}/Cargo.toml" -- --nocapture
    WORKING_DIRECTORY "${_rust_pkg_dir}"
)

set_tests_properties(test_rust_bindings PROPERTIES
    ENVIRONMENT
        "OPENJP3D_LIBRARY=$<TARGET_FILE:openjp3d>"
    TIMEOUT 300
)

message(STATUS "Rust binding tests registered (test_rust_bindings)")
