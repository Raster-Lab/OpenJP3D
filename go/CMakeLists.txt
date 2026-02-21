# Copyright (c) 2024-2026, OpenJP3D Contributors
# All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause

# ---------------------------------------------------------------------------
# Go bindings CTest registration
#
# This CMakeLists.txt does *not* compile anything — all compilation is handled
# by the Go toolchain.  It optionally registers a CTest target that runs the
# Go test suite (if Go ≥ 1.21 is available and CGO_ENABLED=1).
# ---------------------------------------------------------------------------

find_program(Go_EXECUTABLE go)

if(NOT Go_EXECUTABLE)
    message(STATUS "Go executable not found — skipping Go binding tests")
    return()
endif()

# ---------------------------------------------------------------------------
# Verify Go version ≥ 1.21
# ---------------------------------------------------------------------------
execute_process(
    COMMAND ${Go_EXECUTABLE} version
    OUTPUT_VARIABLE _go_ver_out
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _go_ver_rc
)

if(NOT _go_ver_rc EQUAL 0)
    message(STATUS "Could not determine Go version — skipping Go binding tests")
    return()
endif()

# Parse major.minor from "go version go1.X.Y ..."
string(REGEX MATCH "go([0-9]+)\\.([0-9]+)" _gver "${_go_ver_out}")
set(_gmaj "${CMAKE_MATCH_1}")
set(_gmin "${CMAKE_MATCH_2}")

if(_gmaj LESS 1 OR (_gmaj EQUAL 1 AND _gmin LESS 21))
    message(STATUS
        "Go ${_go_ver_out} found but ≥ 1.21 is required — "
        "skipping Go binding tests")
    return()
endif()

message(STATUS "Go ${_go_ver_out} found at ${Go_EXECUTABLE}")

# ---------------------------------------------------------------------------
# Require a shared library build
# ---------------------------------------------------------------------------
if(NOT BUILD_SHARED_LIBS)
    message(STATUS
        "BUILD_SHARED_LIBS is OFF — Go CGo bindings require a shared "
        "library.  Re-configure with -DBUILD_SHARED_LIBS=ON to enable them.")
    return()
endif()

# ---------------------------------------------------------------------------
# CTest target: run Go test suite
# ---------------------------------------------------------------------------
set(_go_pkg_dir "${CMAKE_SOURCE_DIR}/go/openjp3d")

add_test(
    NAME test_go_bindings
    COMMAND ${Go_EXECUTABLE} test -v ./...
    WORKING_DIRECTORY "${_go_pkg_dir}"
)

set_tests_properties(test_go_bindings PROPERTIES
    ENVIRONMENT
        "OPENJP3D_LIBRARY=$<TARGET_FILE:openjp3d>;CGO_ENABLED=1"
    TIMEOUT 120
)

message(STATUS "Go binding tests registered (test_go_bindings)")
