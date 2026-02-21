# SPDX-License-Identifier: BSD-2-Clause

set(OPENJPIP3D_SOURCES
    openjpip3d.c
    opj_jpip3d_request.c
    opj_jpip3d_stream.c
    opj_jpip3d_cache.c
    opj_jpip3d_meta.c
    opj_jpip3d_server.c
    opj_jpip3d_client.c
)

set(OPENJPIP3D_HEADERS
    openjpip3d.h
    opj_jpip3d_request.h
    opj_jpip3d_stream.h
    opj_jpip3d_cache.h
    opj_jpip3d_meta.h
    opj_jpip3d_server.h
    opj_jpip3d_client.h
)

add_library(openjpip3d ${OPENJPIP3D_SOURCES} ${OPENJPIP3D_HEADERS})

target_include_directories(openjpip3d
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/lib/openjp3d
)

target_link_libraries(openjpip3d PUBLIC openjp3d)

if(BUILD_SHARED_LIBS)
    target_compile_definitions(openjpip3d PRIVATE OPJ_JPIP3D_BUILDING_DLL)
endif()

set_target_properties(openjpip3d PROPERTIES
    VERSION   ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER openjpip3d.h
)
