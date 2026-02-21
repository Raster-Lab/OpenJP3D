# Copyright (c) 2024-2026, OpenJP3D Contributors
# All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause

# ---------------------------------------------------------------------------
# Python bindings installation / test target
#
# This CMakeLists.txt does *not* compile anything — the Python package is
# pure ctypes.  It optionally:
#   • copies the built shared library next to the Python package so it can
#     be found without setting OPENJP3D_LIBRARY.
#   • adds a CTest that runs the Python test suite (if Python + pytest are
#     available).
# ---------------------------------------------------------------------------

find_package(Python3 COMPONENTS Interpreter)

if(NOT Python3_FOUND)
    message(STATUS "Python3 interpreter not found — skipping Python binding tests")
    return()
endif()

# Check for pytest
execute_process(
    COMMAND ${Python3_EXECUTABLE} -m pytest --version
    RESULT_VARIABLE _pytest_result
    OUTPUT_QUIET ERROR_QUIET
)

if(NOT _pytest_result EQUAL 0)
    message(STATUS
        "pytest not found — Python binding tests will be skipped. "
        "Install with: pip install pytest")
    return()
endif()

# Check for numpy
execute_process(
    COMMAND ${Python3_EXECUTABLE} -c "import numpy"
    RESULT_VARIABLE _numpy_result
    OUTPUT_QUIET ERROR_QUIET
)

if(NOT _numpy_result EQUAL 0)
    message(STATUS
        "NumPy not found — Python binding tests will be skipped. "
        "Install with: pip install numpy")
    return()
endif()

# ---------------------------------------------------------------------------
# Determine the path to the built shared library
# ---------------------------------------------------------------------------
if(BUILD_SHARED_LIBS)
    set(_lib_hint "$<TARGET_FILE:openjp3d>")
else()
    # Static build — bindings require a shared library; skip tests silently
    message(STATUS
        "BUILD_SHARED_LIBS is OFF — Python ctypes bindings require a shared "
        "library.  Re-configure with -DBUILD_SHARED_LIBS=ON to enable them.")
    return()
endif()

# ---------------------------------------------------------------------------
# CTest target: run pytest against tests/test_python.py
# ---------------------------------------------------------------------------
set(_py_test_dir "${CMAKE_SOURCE_DIR}/tests")
set(_py_pkg_dir  "${CMAKE_SOURCE_DIR}/python")

add_test(
    NAME test_python_bindings
    COMMAND ${Python3_EXECUTABLE} -m pytest
            "${_py_test_dir}/test_python.py"
            -v
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Make the shared library findable via OPENJP3D_LIBRARY
set_tests_properties(test_python_bindings PROPERTIES
    ENVIRONMENT
        "OPENJP3D_LIBRARY=$<TARGET_FILE:openjp3d>;PYTHONPATH=${_py_pkg_dir}"
)

message(STATUS "Python binding tests registered (test_python_bindings)")
