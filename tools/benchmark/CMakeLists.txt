# SPDX-License-Identifier: BSD-2-Clause

add_executable(bench_dwt3d bench_dwt3d.c)
target_include_directories(bench_dwt3d PRIVATE
    ${CMAKE_SOURCE_DIR}/src/lib/openjp3d)
target_link_libraries(bench_dwt3d PRIVATE openjp3d)
