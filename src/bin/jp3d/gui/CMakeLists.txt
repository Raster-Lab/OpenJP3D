# SPDX-License-Identifier: BSD-2-Clause
# Phase 8A: Interactive GUI — Application Framework & UI Shell
#
# This CMakeLists.txt builds the opj_jp3d_gui target using Dear ImGui,
# SDL2, and OpenGL 3.3.  It is only processed when BUILD_GUI_TOOLS=ON.

cmake_minimum_required(VERSION 3.14)

# We need C++ for Dear ImGui
enable_language(CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -----------------------------------------------------------------------
# Dependencies
# -----------------------------------------------------------------------

include(FetchContent)

# --- Dear ImGui (docking branch — includes built-in docking support) ---
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        docking
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

# Build Dear ImGui as a static library with SDL2+OpenGL3 backends
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
# Suppress warnings in third-party code
target_compile_options(imgui_lib PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-w>
    $<$<CXX_COMPILER_ID:MSVC>:/W0>
)

# --- SDL2 -------------------------------------------------------------
# Try system SDL2 first, fall back to FetchContent
find_package(SDL2 QUIET)
if(NOT SDL2_FOUND)
    message(STATUS "System SDL2 not found — fetching via FetchContent")
    FetchContent_Declare(
        sdl2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        release-2.30.11
        GIT_SHALLOW    TRUE
    )
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(sdl2)
    set(SDL2_LIBRARIES SDL2-static SDL2main)
    set(SDL2_INCLUDE_DIRS ${sdl2_SOURCE_DIR}/include)
else()
    # Normalise imported target names
    if(TARGET SDL2::SDL2)
        set(SDL2_LIBRARIES SDL2::SDL2)
        if(TARGET SDL2::SDL2main)
            list(APPEND SDL2_LIBRARIES SDL2::SDL2main)
        endif()
    endif()
endif()

target_include_directories(imgui_lib PUBLIC ${SDL2_INCLUDE_DIRS})
target_link_libraries(imgui_lib PUBLIC ${SDL2_LIBRARIES})

# --- OpenGL ------------------------------------------------------------
find_package(OpenGL REQUIRED)

# -----------------------------------------------------------------------
# opj_jp3d_gui executable
# -----------------------------------------------------------------------

add_executable(opj_jp3d_gui
    opj_jp3d_gui.cpp
    gui_theme.cpp
    gui_volume.cpp
    gui_codec.cpp
    gui_roundtrip.cpp
    gui_jpip.cpp
    gui_log_prefs.cpp
)

target_include_directories(opj_jp3d_gui PRIVATE
    ${CMAKE_SOURCE_DIR}/src/lib/openjp3d
    ${CMAKE_SOURCE_DIR}/src/lib/openjpip3d
)

target_link_libraries(opj_jp3d_gui PRIVATE
    openjp3d
    openjpip3d
    imgui_lib
    OpenGL::GL
)

# Platform-specific link dependencies
if(APPLE)
    # macOS frameworks for SDL2
    target_link_libraries(opj_jp3d_gui PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
    )
elseif(UNIX)
    # X11 / Wayland dependencies usually pulled in by SDL2
    find_package(Threads REQUIRED)
    target_link_libraries(opj_jp3d_gui PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
endif()

# -----------------------------------------------------------------------
# Install target (8F.3 — cross-platform packaging)
# -----------------------------------------------------------------------

include(GNUInstallDirs)
install(TARGETS opj_jp3d_gui
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE  DESTINATION .   # macOS .app bundle goes to the install prefix
)

# -----------------------------------------------------------------------
# CPack packaging (8F.3)
# -----------------------------------------------------------------------
# Only configure CPack when this file is processed as the top-level
# (i.e., when BUILD_GUI_TOOLS is the entry point).  The root
# CMakeLists.txt controls whether CPack is included at all.

if(NOT CPACK_PACKAGE_NAME)
    set(CPACK_PACKAGE_NAME            "OpenJP3D-GUI")
    set(CPACK_PACKAGE_VENDOR          "OpenJP3D Contributors")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
        "Interactive GUI for the OpenJP3D JPEG 2000 Part 10 volumetric codec")
    set(CPACK_PACKAGE_VERSION         "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "OpenJP3D-GUI")

    if(WIN32)
        # Windows: portable ZIP  (8F.3)
        set(CPACK_GENERATOR "ZIP")
        set(CPACK_PACKAGE_FILE_NAME
            "OpenJP3D-GUI-${PROJECT_VERSION}-win64")
    elseif(APPLE)
        # macOS: DragNDrop DMG with .app bundle  (8F.3)
        set(CPACK_GENERATOR "DragNDrop")
        set(CPACK_PACKAGE_FILE_NAME
            "OpenJP3D-GUI-${PROJECT_VERSION}-macOS")
        set_target_properties(opj_jp3d_gui PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_BUNDLE_NAME          "OpenJP3D GUI"
            MACOSX_BUNDLE_BUNDLE_VERSION       "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            MACOSX_BUNDLE_GUI_IDENTIFIER       "org.openjp3d.gui"
        )
    else()
        # Linux: TGZ archive — users may repackage as AppImage/Flatpak  (8F.3)
        set(CPACK_GENERATOR "TGZ")
        set(CPACK_PACKAGE_FILE_NAME
            "OpenJP3D-GUI-${PROJECT_VERSION}-linux-x86_64")
    endif()

    include(CPack)
endif()
