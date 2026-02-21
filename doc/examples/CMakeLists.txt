# SPDX-License-Identifier: BSD-2-Clause

# Build doc/examples as standalone programs that link against the codec
# libraries.  These are not part of the default build — enable with
#   cmake -DBUILD_DOC_EXAMPLES=ON

if(NOT TARGET openjp3d)
    return()
endif()

set(EXAMPLES_CORE
    encode_volume
    decode_volume
    htj2k_encode
)

foreach(ex ${EXAMPLES_CORE})
    add_executable(example_${ex} ${ex}.c)
    target_link_libraries(example_${ex} PRIVATE openjp3d)
endforeach()

# Examples that also require the JPIP library.
if(TARGET openjpip3d)
    set(EXAMPLES_JPIP
        partial_decode
        jpip3d_client
    )

    foreach(ex ${EXAMPLES_JPIP})
        add_executable(example_${ex} ${ex}.c)
        target_link_libraries(example_${ex} PRIVATE openjpip3d openjp3d)
    endforeach()
endif()
