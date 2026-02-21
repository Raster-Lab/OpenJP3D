# Copyright (c) 2024-2026, OpenJP3D Contributors
# All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause

# ---------------------------------------------------------------------------
# MATLAB/Octave bindings CTest registration
#
# This CMakeLists.txt does *not* install anything — it:
#   • Locates the Octave (preferred) or MATLAB mex compiler.
#   • Registers two CTest targets:
#       build_matlab_mex   — compile openjp3d_mex.mex with mkoctfile
#       test_matlab_bindings — run tests/test_matlab.m with octave
# ---------------------------------------------------------------------------

option(BUILD_MATLAB_BINDINGS
    "Register MATLAB/Octave binding tests (requires Octave or MATLAB)"
    OFF)

if(NOT BUILD_MATLAB_BINDINGS)
    return()
endif()

# ---------------------------------------------------------------------------
# Require a shared library build
# ---------------------------------------------------------------------------
if(NOT BUILD_SHARED_LIBS)
    message(STATUS
        "BUILD_SHARED_LIBS is OFF — MATLAB bindings require a shared library. "
        "Re-configure with -DBUILD_SHARED_LIBS=ON to enable them.")
    return()
endif()

# ---------------------------------------------------------------------------
# Locate Octave tools (preferred over MATLAB for open-source CI)
# ---------------------------------------------------------------------------
find_program(OCTAVE_EXECUTABLE    octave)
find_program(MKOCTFILE_EXECUTABLE mkoctfile)

if(NOT OCTAVE_EXECUTABLE OR NOT MKOCTFILE_EXECUTABLE)
    # Fall back to MATLAB if available
    find_package(Matlab QUIET COMPONENTS MEX_COMPILER)
    if(Matlab_FOUND)
        set(_matlab_mode TRUE)
        message(STATUS "Octave not found — using MATLAB mex compiler")
    else()
        message(STATUS
            "Octave (mkoctfile + octave) not found and MATLAB not available "
            "— skipping MATLAB/Octave binding tests")
        return()
    endif()
else()
    set(_matlab_mode FALSE)
    message(STATUS "Found Octave: ${OCTAVE_EXECUTABLE}")
    message(STATUS "Found mkoctfile: ${MKOCTFILE_EXECUTABLE}")
endif()

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
set(_mex_src    "${CMAKE_SOURCE_DIR}/matlab/openjp3d/src/openjp3d_mex.c")
set(_mex_out    "${CMAKE_BINARY_DIR}/openjp3d_mex")
set(_mat_dir    "${CMAKE_SOURCE_DIR}/matlab/openjp3d")
set(_test_script "${CMAKE_SOURCE_DIR}/tests/test_matlab.m")

# ---------------------------------------------------------------------------
# CTest: build MEX file
# ---------------------------------------------------------------------------
if(_matlab_mode)
    add_test(
        NAME build_matlab_mex
        COMMAND ${Matlab_MEX_COMPILER}
                    -output "${_mex_out}"
                    "${_mex_src}"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
else()
    add_test(
        NAME build_matlab_mex
        COMMAND ${MKOCTFILE_EXECUTABLE}
                    --mex
                    -o "${_mex_out}"
                    "${_mex_src}"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
endif()

set_tests_properties(build_matlab_mex PROPERTIES
    TIMEOUT 120
)

# ---------------------------------------------------------------------------
# CTest: run the test suite
# ---------------------------------------------------------------------------
if(_matlab_mode)
    # MATLAB batch mode
    add_test(
        NAME test_matlab_bindings
        COMMAND ${CMAKE_COMMAND} -E env
                    "OPENJP3D_LIBRARY=$<TARGET_FILE:openjp3d>"
                ${Matlab_MAIN_PROGRAM}
                    -batch
                    "addpath('${_mat_dir}'); addpath('${CMAKE_BINARY_DIR}'); run('${_test_script}')"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
else()
    add_test(
        NAME test_matlab_bindings
        COMMAND ${CMAKE_COMMAND} -E env
                    "OPENJP3D_LIBRARY=$<TARGET_FILE:openjp3d>"
                ${OCTAVE_EXECUTABLE}
                    --no-gui
                    --path "${_mat_dir}"
                    --path "${CMAKE_BINARY_DIR}"
                    "${_test_script}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endif()

set_tests_properties(test_matlab_bindings PROPERTIES
    TIMEOUT 300
    DEPENDS build_matlab_mex
)

message(STATUS "MATLAB/Octave binding tests registered "
               "(build_matlab_mex, test_matlab_bindings)")
