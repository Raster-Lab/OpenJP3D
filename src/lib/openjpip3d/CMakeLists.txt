# SPDX-License-Identifier: BSD-2-Clause

set(OPENJPIP3D_SOURCES
    openjpip3d.c
)

set(OPENJPIP3D_HEADERS
    openjpip3d.h
)

add_library(openjpip3d ${OPENJPIP3D_SOURCES} ${OPENJPIP3D_HEADERS})

target_include_directories(openjpip3d
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

if(BUILD_SHARED_LIBS)
    target_compile_definitions(openjpip3d PRIVATE OPJ_JPIP3D_BUILDING_DLL)
endif()

set_target_properties(openjpip3d PROPERTIES
    VERSION   ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER openjpip3d.h
)
