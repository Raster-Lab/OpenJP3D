# SPDX-License-Identifier: BSD-2-Clause

# ---------------------------------------------------------------------------
# Basic smoke test: verify library version string
# ---------------------------------------------------------------------------
add_executable(test_version test_version.c)
target_link_libraries(test_version PRIVATE openjp3d)
add_test(NAME version_string COMMAND test_version)
