# SPDX-License-Identifier: BSD-2-Clause

find_package(Doxygen QUIET)

if(DOXYGEN_FOUND)
    set(DOXYGEN_PROJECT_NAME "OpenJP3D")
    set(DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION}")
    set(DOXYGEN_PROJECT_BRIEF "JPEG 2000 Part 10 (JP3D) volumetric codec")
    set(DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/doxygen")
    set(DOXYGEN_EXTRACT_ALL YES)
    set(DOXYGEN_RECURSIVE YES)
    set(DOXYGEN_GENERATE_HTML YES)
    set(DOXYGEN_GENERATE_LATEX NO)

    doxygen_add_docs(doc
        "${CMAKE_SOURCE_DIR}/src/lib/openjp3d"
        "${CMAKE_SOURCE_DIR}/src/lib/openjpip3d"
        COMMENT "Generating API documentation with Doxygen"
    )
else()
    message(STATUS "Doxygen not found — documentation target will not be available")
endif()
