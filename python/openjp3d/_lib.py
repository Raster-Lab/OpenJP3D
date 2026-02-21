# Copyright (c) 2024-2026, OpenJP3D Contributors
# All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause

"""Shared-library loader for the OpenJP3D native library."""

import ctypes
import ctypes.util
import os
import sys

_lib = None


def _find_lib():
    """Locate the openjp3d shared library, return ctypes.CDLL or raise."""
    # 1. Explicit override via environment variable
    env_path = os.environ.get("OPENJP3D_LIBRARY")
    if env_path:
        return ctypes.CDLL(env_path)

    # 2. Search next to this package (installed wheel layout)
    pkg_dir = os.path.dirname(os.path.abspath(__file__))
    candidates = []
    if sys.platform == "win32":
        candidates.append(os.path.join(pkg_dir, "openjp3d.dll"))
    elif sys.platform == "darwin":
        candidates.append(os.path.join(pkg_dir, "libopenjp3d.dylib"))
        candidates.append(os.path.join(pkg_dir, "libopenjp3d.1.dylib"))
    else:
        candidates.append(os.path.join(pkg_dir, "libopenjp3d.so"))
        candidates.append(os.path.join(pkg_dir, "libopenjp3d.so.1"))

    for path in candidates:
        if os.path.exists(path):
            return ctypes.CDLL(path)

    # 3. System search via ctypes.util
    name = ctypes.util.find_library("openjp3d")
    if name:
        return ctypes.CDLL(name)

    raise OSError(
        "Cannot find the openjp3d shared library. "
        "Build the project with BUILD_SHARED_LIBS=ON and ensure the "
        "library is on your library search path, or set the "
        "OPENJP3D_LIBRARY environment variable to its full path."
    )


def get_lib():
    """Return the loaded ctypes library handle (loads once on first call)."""
    global _lib
    if _lib is None:
        _lib = _find_lib()
    return _lib
