# SPDX-License-Identifier: BSD-2-Clause

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
)

set(OPENJP3D_HEADERS
    openjp3d.h
    openjp3d_private.h
    opj_mem.h
    opj_volume.h
    opj_dwt3d.h
    opj_t1_3d.h
    opj_ht3d.h
    opj_t2_3d.h
    opj_cs3d.h
    opj_raw_io.h
    opj_tcd3d.h
)

add_library(openjp3d ${OPENJP3D_SOURCES} ${OPENJP3D_HEADERS})

target_include_directories(openjp3d
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(openjp3d PRIVATE m)

# Shared library export macro
if(BUILD_SHARED_LIBS)
    target_compile_definitions(openjp3d PRIVATE OPJ_JP3D_BUILDING_DLL)
endif()

set_target_properties(openjp3d PROPERTIES
    VERSION   ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER openjp3d.h
)
