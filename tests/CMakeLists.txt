# SPDX-License-Identifier: BSD-2-Clause

# ---------------------------------------------------------------------------
# Basic smoke test: verify library version string
# ---------------------------------------------------------------------------
add_executable(test_version test_version.c)
target_link_libraries(test_version PRIVATE openjp3d)
add_test(NAME version_string COMMAND test_version)

# ---------------------------------------------------------------------------
# Volume lifecycle tests
# ---------------------------------------------------------------------------
add_executable(test_volume test_volume.c)
target_link_libraries(test_volume PRIVATE openjp3d)
add_test(NAME volume COMMAND test_volume)

# ---------------------------------------------------------------------------
# 3-D DWT round-trip tests
# ---------------------------------------------------------------------------
add_executable(test_dwt3d test_dwt3d.c)
target_include_directories(test_dwt3d PRIVATE
    ${CMAKE_SOURCE_DIR}/src/lib/openjp3d)
target_link_libraries(test_dwt3d PRIVATE openjp3d)
add_test(NAME dwt3d COMMAND test_dwt3d)

# ---------------------------------------------------------------------------
# Encode/decode round-trip tests
# ---------------------------------------------------------------------------
add_executable(test_roundtrip test_roundtrip.c)
target_link_libraries(test_roundtrip PRIVATE openjp3d)
add_test(NAME roundtrip COMMAND test_roundtrip)

# ---------------------------------------------------------------------------
# Parameter initialisation tests
# ---------------------------------------------------------------------------
add_executable(test_params test_params.c)
target_link_libraries(test_params PRIVATE openjp3d)
add_test(NAME params COMMAND test_params)

# ---------------------------------------------------------------------------
# Raw I/O tests
# ---------------------------------------------------------------------------
add_executable(test_raw_io test_raw_io.c)
target_include_directories(test_raw_io PRIVATE
    ${CMAKE_SOURCE_DIR}/src/lib/openjp3d)
target_link_libraries(test_raw_io PRIVATE openjp3d)
add_test(NAME raw_io COMMAND test_raw_io)

# ---------------------------------------------------------------------------
# HTJ2K (Phase 2) tests
# ---------------------------------------------------------------------------
add_executable(test_htj2k test_htj2k.c)
target_include_directories(test_htj2k PRIVATE
    ${CMAKE_SOURCE_DIR}/src/lib/openjp3d)
target_link_libraries(test_htj2k PRIVATE openjp3d)
add_test(NAME htj2k COMMAND test_htj2k)



# ---------------------------------------------------------------------------
# SIMD (Phase 4) tests
# ---------------------------------------------------------------------------
add_executable(test_simd test_simd.c)
target_include_directories(test_simd PRIVATE
    ${CMAKE_SOURCE_DIR}/src/lib/openjp3d)
target_link_libraries(test_simd PRIVATE openjp3d)
add_test(NAME simd COMMAND test_simd)
if(BUILD_JPIP_3D)
    add_executable(test_jpip3d test_jpip3d.c)
    target_include_directories(test_jpip3d PRIVATE
        ${CMAKE_SOURCE_DIR}/src/lib/openjpip3d
        ${CMAKE_SOURCE_DIR}/src/lib/openjp3d)
    target_link_libraries(test_jpip3d PRIVATE openjpip3d openjp3d)
    add_test(NAME jpip3d COMMAND test_jpip3d)
endif()
