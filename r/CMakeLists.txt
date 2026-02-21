# Copyright (c) 2024-2026, OpenJP3D Contributors
# All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause

# ---------------------------------------------------------------------------
# R bindings CTest registration
#
# This CMakeLists.txt does *not* compile anything — it delegates compilation
# to R's package build machinery via "R CMD INSTALL".  It optionally:
#   • installs the R package into a temporary library directory.
#   • registers a CTest target that runs tests/test_r.R with the shared
#     library available via OPENJP3D_LIBRARY.
# ---------------------------------------------------------------------------

find_program(R_EXECUTABLE R)
find_program(RSCRIPT_EXECUTABLE Rscript)

if(NOT R_EXECUTABLE OR NOT RSCRIPT_EXECUTABLE)
    message(STATUS "R/Rscript not found — skipping R binding tests")
    return()
endif()

# ---------------------------------------------------------------------------
# Require a shared library build
# ---------------------------------------------------------------------------
if(NOT BUILD_SHARED_LIBS)
    message(STATUS
        "BUILD_SHARED_LIBS is OFF — R bindings require a shared library. "
        "Re-configure with -DBUILD_SHARED_LIBS=ON to enable them.")
    return()
endif()

# ---------------------------------------------------------------------------
# Temporary R library directory for the installed package
# ---------------------------------------------------------------------------
set(_r_lib_dir "${CMAKE_BINARY_DIR}/r_library")
file(MAKE_DIRECTORY "${_r_lib_dir}")

set(_r_pkg_src "${CMAKE_SOURCE_DIR}/r/openjp3d")
set(_r_test    "${CMAKE_SOURCE_DIR}/tests/test_r.R")

# ---------------------------------------------------------------------------
# CTest: install the R package, then run the test script
# ---------------------------------------------------------------------------
add_test(
    NAME test_r_bindings
    COMMAND ${CMAKE_COMMAND} -E env
                "OPENJP3D_LIBRARY=$<TARGET_FILE:openjp3d>"
                "R_LIBS_USER=${_r_lib_dir}"
            ${RSCRIPT_EXECUTABLE} -e
                "install.packages('${_r_pkg_src}', lib='${_r_lib_dir}', repos=NULL, type='source', quiet=TRUE); .libPaths(c('${_r_lib_dir}', .libPaths())); library(openjp3d); source('${_r_test}')"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

set_tests_properties(test_r_bindings PROPERTIES
    TIMEOUT 300
)

message(STATUS "R binding tests registered (test_r_bindings)")
