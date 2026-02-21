# Copyright (c) 2024-2026, OpenJP3D Contributors
# All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause

# .onLoad — locate and dynamically open the openjp3d shared library.
#
# Search order (mirrors Python _lib.py and Julia lib_path()):
#   1. OPENJP3D_LIBRARY environment variable.
#   2. A lib/ directory next to the installed package (for testing against
#      a build tree).
#   3. System library search (Sys.getenv("LD_LIBRARY_PATH") etc.) via
#      system2("sh", "-c 'ldconfig -p'") is too fragile; we simply try the
#      bare name and let the OS loader handle it.

.onLoad <- function(libname, pkgname) {
    path <- .find_lib()
    if (is.null(path)) {
        packageStartupMessage(
            "openjp3d: shared library not found.\n",
            "  Build the project with -DBUILD_SHARED_LIBS=ON and set\n",
            "  OPENJP3D_LIBRARY to the full path of libopenjp3d.so / .dylib / .dll,\n",
            "  or place it next to the installed R package in a lib/ subdirectory."
        )
        return(invisible(NULL))
    }
    tryCatch(
        .Call("ojp3d_load_lib", path, PACKAGE = pkgname),
        error = function(e) {
            packageStartupMessage("openjp3d: failed to load library '", path,
                                  "': ", conditionMessage(e))
        }
    )
    invisible(NULL)
}

# Internal helper: return the library path string, or NULL if not found.
.find_lib <- function() {
    # 1. Environment variable
    env <- Sys.getenv("OPENJP3D_LIBRARY", unset = "")
    if (nzchar(env) && file.exists(env))
        return(env)

    # 2. lib/ next to the R package
    pkg_dir <- system.file(package = "openjp3d")
    lib_dir  <- file.path(pkg_dir, "lib")
    if (dir.exists(lib_dir)) {
        exts <- if (.Platform$OS.type == "windows") {
            "openjp3d.dll"
        } else if (Sys.info()[["sysname"]] == "Darwin") {
            c("libopenjp3d.dylib", "libopenjp3d.1.dylib")
        } else {
            c("libopenjp3d.so", "libopenjp3d.so.1")
        }
        for (name in exts) {
            candidate <- file.path(lib_dir, name)
            if (file.exists(candidate))
                return(candidate)
        }
    }

    # 3. Bare name — let the OS loader find it
    bare <- if (.Platform$OS.type == "windows") "openjp3d" else "libopenjp3d"
    bare
}
