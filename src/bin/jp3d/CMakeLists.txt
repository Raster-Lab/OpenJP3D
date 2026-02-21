# SPDX-License-Identifier: BSD-2-Clause
# Phase 5: Command-Line Tools

# ---------------------------------------------------------------------------
# opj_jp3d_compress — encode raw volumes to JP3D codestream
# ---------------------------------------------------------------------------
add_executable(opj_jp3d_compress opj_jp3d_compress.c)
target_include_directories(opj_jp3d_compress PRIVATE
    ${CMAKE_SOURCE_DIR}/src/lib/openjp3d)
target_link_libraries(opj_jp3d_compress PRIVATE openjp3d)

# ---------------------------------------------------------------------------
# opj_jp3d_decompress — decode JP3D codestream to raw volumes
# ---------------------------------------------------------------------------
add_executable(opj_jp3d_decompress opj_jp3d_decompress.c)
target_include_directories(opj_jp3d_decompress PRIVATE
    ${CMAKE_SOURCE_DIR}/src/lib/openjp3d)
target_link_libraries(opj_jp3d_decompress PRIVATE openjp3d)

# ---------------------------------------------------------------------------
# opj_jp3d_dump — inspect JP3D codestream structure
# ---------------------------------------------------------------------------
add_executable(opj_jp3d_dump opj_jp3d_dump.c)
target_include_directories(opj_jp3d_dump PRIVATE
    ${CMAKE_SOURCE_DIR}/src/lib/openjp3d)
target_link_libraries(opj_jp3d_dump PRIVATE openjp3d)

# ---------------------------------------------------------------------------
# opj_jp3d_transcode — transcode EBCOT <-> HTJ2K
# ---------------------------------------------------------------------------
add_executable(opj_jp3d_transcode opj_jp3d_transcode.c)
target_include_directories(opj_jp3d_transcode PRIVATE
    ${CMAKE_SOURCE_DIR}/src/lib/openjp3d)
target_link_libraries(opj_jp3d_transcode PRIVATE openjp3d)

# ---------------------------------------------------------------------------
# opj_jpip3d_server — standalone JPIP server for JP3D datasets
# ---------------------------------------------------------------------------
if(BUILD_JPIP_3D)
    add_executable(opj_jpip3d_server opj_jpip3d_server.c)
    target_include_directories(opj_jpip3d_server PRIVATE
        ${CMAKE_SOURCE_DIR}/src/lib/openjp3d
        ${CMAKE_SOURCE_DIR}/src/lib/openjpip3d)
    target_link_libraries(opj_jpip3d_server PRIVATE openjpip3d openjp3d)
endif()

# ---------------------------------------------------------------------------
# Man pages
# ---------------------------------------------------------------------------
set(MAN_PAGES
    ${CMAKE_CURRENT_SOURCE_DIR}/opj_jp3d_compress.1
    ${CMAKE_CURRENT_SOURCE_DIR}/opj_jp3d_decompress.1
    ${CMAKE_CURRENT_SOURCE_DIR}/opj_jp3d_dump.1
    ${CMAKE_CURRENT_SOURCE_DIR}/opj_jp3d_transcode.1
)
if(BUILD_JPIP_3D)
    list(APPEND MAN_PAGES
        ${CMAKE_CURRENT_SOURCE_DIR}/opj_jpip3d_server.1)
endif()

install(FILES ${MAN_PAGES} DESTINATION share/man/man1 OPTIONAL)
