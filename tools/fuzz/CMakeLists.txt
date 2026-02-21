# SPDX-License-Identifier: BSD-2-Clause
# Fuzzer harness — requires a compiler with -fsanitize=fuzzer support (Clang).

if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    add_executable(fuzz_decode fuzz_decode.c)
    target_include_directories(fuzz_decode PRIVATE
        ${CMAKE_SOURCE_DIR}/src/lib/openjp3d)
    target_link_libraries(fuzz_decode PRIVATE openjp3d)
    target_compile_options(fuzz_decode PRIVATE -fsanitize=fuzzer,address -O1 -g)
    target_link_options(fuzz_decode PRIVATE -fsanitize=fuzzer,address)
    message(STATUS "OpenJP3D: libFuzzer harness enabled (fuzz_decode)")
else()
    message(STATUS "OpenJP3D: libFuzzer harness requires Clang — skipping")
endif()
