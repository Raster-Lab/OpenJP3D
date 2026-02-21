# SPDX-License-Identifier: BSD-2-Clause
# Minimal external project that uses find_package(OpenJP3D) to verify
# the installed CMake config package works correctly.

cmake_minimum_required(VERSION 3.14)
project(find_package_test C)

find_package(OpenJP3D REQUIRED)

add_executable(fp_test main.c)
target_link_libraries(fp_test PRIVATE openjp3d)
