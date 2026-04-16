# Copyright (c) 2009-2011, Wei Mingzhi <whistler_wmz@users.sf.net>.
# Copyright (c) 2011-2024, SDLPAL development team.
# All rights reserved.
#
# This file is part of SDLPAL.
#
# SDLPAL is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 3
# as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# CMakeLists.txt: CMake build for SDLPAL
#
# Supported platforms: Linux, Windows (MSVC/MinGW), macOS, Emscripten
#
# Usage examples:
#   Linux/macOS:
#     cmake -B build && cmake --build build
#
#   Windows MSVC:
#     cmake -B build -G "Visual Studio 17 2022" && cmake --build build --config Release
#
#   Windows MinGW:
#     cmake -B build -G "MinGW Makefiles" && cmake --build build
#
#   Emscripten:
#     emcmake cmake -B build && cmake --build build
#
#   With embedded SDL3:
#     cmake -B build -DSDLPAL_USE_EMBEDDED_SDL=ON
#
#   With SDL2 instead of SDL3:
#     cmake -B build -DSDLPAL_USE_SDL3=OFF
#

cmake_minimum_required(VERSION 3.14)
project(sdlpal LANGUAGES C CXX)

# Export compile_commands.json for IDE / language-server (clangd) support.
# Has no effect on Visual Studio generator; takes effect with Ninja / Makefile generators.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# =============================================================================
# Build Options
# =============================================================================
option(SDLPAL_USE_SDL3         "Use SDL3 instead of SDL2 (default: ON)"       ON)
option(SDLPAL_USE_EMBEDDED_SDL "Build SDL3 from embedded (3rd/SDL)"           ON)
option(SDLPAL_USE_ALSA         "Use ALSA for native MIDI (Linux only)"        ON)
option(SDLPAL_USE_FLTK         "Use FLTK for configuration UI (Linux only)"   ON)
option(SDLPAL_ENABLE_TESTS     "Build unit tests"                             OFF)

# =============================================================================
# Language Standards
# =============================================================================
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)    # gnu99
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =============================================================================
# Git Revision
# =============================================================================
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_REV_COUNT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} show -s --format=%cs
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_DATE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(GIT_COMMIT_HASH)
        set(PAL_GIT_REVISION "v${GIT_COMMIT_DATE}-r${GIT_REV_COUNT}-g${GIT_COMMIT_HASH}")
        file(WRITE "${CMAKE_SOURCE_DIR}/generated.h"
            "#define PAL_GIT_REVISION \"${PAL_GIT_REVISION}\"\n"
        )
        set(SDLPAL_HAS_GIT_REVISION TRUE)
    endif()
endif()

# =============================================================================
# Core Engine Sources (root directory)
# =============================================================================
# Matches the Makefile wildcard pattern: $(wildcard ../*.c) $(wildcard ../*.cpp)
file(GLOB CORE_C_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/*.c")
file(GLOB CORE_CXX_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/*.cpp")

# On macOS, exclude mini_glloader.c (OpenGL loaded via framework)
if(APPLE AND NOT EMSCRIPTEN)
    list(FILTER CORE_C_SOURCES EXCLUDE REGEX "mini_glloader\\.c$")
endif()

# =============================================================================
# SDL Compatibility Layer
# =============================================================================
file(GLOB SDL_COMPAT_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/sdl_compat/*.c")

# =============================================================================
# Bundled Libraries - AdPlug (OPL FM synthesis)
# =============================================================================
file(GLOB ADPLUG_C_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/adplug/*.c")
file(GLOB ADPLUG_CXX_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/adplug/*.cpp")

# =============================================================================
# Bundled Libraries - Timidity (MIDI synthesis)
# =============================================================================
file(GLOB TIMIDITY_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/timidity/*.c")

# =============================================================================
# Platform Detection & Platform-Specific Sources
# =============================================================================
set(NATIVE_MIDI_COMMON "${CMAKE_SOURCE_DIR}/native_midi/native_midi_common.c")

if(EMSCRIPTEN)
    # ---- Emscripten / WebAssembly ----
    set(SDLPAL_PLATFORM_DIR "${CMAKE_SOURCE_DIR}/emscripten")
    set(PLATFORM_SOURCES "${CMAKE_SOURCE_DIR}/emscripten/emscripten_adapter.cpp")

elseif(WIN32)
    # ---- Windows (MSVC / MinGW) ----
    set(SDLPAL_PLATFORM_DIR "${CMAKE_SOURCE_DIR}/win32")
    set(PLATFORM_SOURCES
        "${CMAKE_SOURCE_DIR}/win32/win32.cpp"
        "${CMAKE_SOURCE_DIR}/win32/native_midi.cpp"
        ${NATIVE_MIDI_COMMON}
    )

elseif(APPLE)
    # ---- macOS ----
    set(SDLPAL_PLATFORM_DIR "${CMAKE_SOURCE_DIR}/macos")
    set(PLATFORM_SOURCES
        "${CMAKE_SOURCE_DIR}/macos/native_midi_macosx.c"
        "${CMAKE_SOURCE_DIR}/macos/util.mm"
        ${NATIVE_MIDI_COMMON}
    )

else()
    # ---- Linux / Unix ----
    set(SDLPAL_PLATFORM_DIR "${CMAKE_SOURCE_DIR}/unix")
    set(PLATFORM_SOURCES "${CMAKE_SOURCE_DIR}/unix/unix.cpp")

    # ALSA native MIDI detection
    if(SDLPAL_USE_ALSA)
        find_package(ALSA QUIET)
    endif()
    if(ALSA_FOUND)
        list(APPEND PLATFORM_SOURCES
            "${CMAKE_SOURCE_DIR}/unix/native_midi_alsa.c"
            ${NATIVE_MIDI_COMMON}
        )
    else()
        list(APPEND PLATFORM_SOURCES
            "${CMAKE_SOURCE_DIR}/unix/native_midi.cpp"
        )
    endif()
endif()

# =============================================================================
# Assemble All Sources
# =============================================================================
set(ALL_SOURCES
    ${CORE_C_SOURCES}
    ${CORE_CXX_SOURCES}
    ${SDL_COMPAT_SOURCES}
    ${ADPLUG_C_SOURCES}
    ${ADPLUG_CXX_SOURCES}
    ${TIMIDITY_SOURCES}
    ${PLATFORM_SOURCES}
)

# =============================================================================
# Main Executable
# =============================================================================
if(WIN32)
    # Win32: build as Windows application with resource file
    add_executable(sdlpal WIN32 ${ALL_SOURCES} "${CMAKE_SOURCE_DIR}/win32/sdlpal.rc")
else()
    add_executable(sdlpal ${ALL_SOURCES})
endif()

# =============================================================================
# Include Directories
# =============================================================================
target_include_directories(sdlpal PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${SDLPAL_PLATFORM_DIR}"
    "${CMAKE_SOURCE_DIR}/sdl_compat"
    "${CMAKE_SOURCE_DIR}/timidity"
)

# =============================================================================
# Compile Definitions
# =============================================================================
target_compile_definitions(sdlpal PRIVATE PAL_HAS_PLATFORM_SPECIFIC_UTILS)

if(SDLPAL_HAS_GIT_REVISION)
    target_compile_definitions(sdlpal PRIVATE PAL_HAS_GIT_REVISION)
endif()

if(SDLPAL_USE_SDL3)
    target_compile_definitions(sdlpal PRIVATE USE_SDL3=1)
endif()

# =============================================================================
# Compile Options
# =============================================================================
if(MSVC)
    target_compile_options(sdlpal PRIVATE /utf-8 /W3)
    target_compile_definitions(sdlpal PRIVATE
        _CRT_SECURE_NO_WARNINGS
        WIN32
        _WINDOWS
    )
else()
    target_compile_options(sdlpal PRIVATE -Wall -O2 -fno-strict-aliasing)
    if(NOT EMSCRIPTEN)
        target_compile_definitions(sdlpal PRIVATE _GNU_SOURCE)
    endif()
endif()

# =============================================================================
# SDL Dependency
# =============================================================================
if(SDLPAL_USE_SDL3)
    if(SDLPAL_USE_EMBEDDED_SDL AND EXISTS "${CMAKE_SOURCE_DIR}/3rd/SDL/CMakeLists.txt")
        message(STATUS "Building embedded SDL3 (3rd/SDL)")
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
        add_subdirectory("${CMAKE_SOURCE_DIR}/3rd/SDL" EXCLUDE_FROM_ALL)
        target_link_libraries(sdlpal PRIVATE SDL3::SDL3-static)
    else()
        # Try find_package first, then pkg-config
        find_package(SDL3 QUIET)
        if(SDL3_FOUND)
            target_link_libraries(sdlpal PRIVATE SDL3::SDL3)
        else()
            find_package(PkgConfig REQUIRED)
            pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)
            target_link_libraries(sdlpal PRIVATE PkgConfig::SDL3)
        endif()
    endif()
else()
    # SDL2
    find_package(SDL2 QUIET)
    if(SDL2_FOUND)
        target_link_libraries(sdlpal PRIVATE SDL2::SDL2)
    else()
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
        target_link_libraries(sdlpal PRIVATE PkgConfig::SDL2)
    endif()
endif()

# =============================================================================
# Platform-Specific Dependencies
# =============================================================================
if(WIN32)
    # Windows system libraries
    target_link_libraries(sdlpal PRIVATE
        opengl32 winmm ole32 oleaut32 imm32
        comctl32 uuid dxguid version setupapi
    )
    if(MINGW)
        target_include_directories(sdlpal PRIVATE
            "${CMAKE_SOURCE_DIR}/3rd/mingw-std-threads"
        )
        target_link_options(sdlpal PRIVATE
            -static -static-libgcc -static-libstdc++
        )
    endif()

elseif(APPLE AND NOT EMSCRIPTEN)
    # macOS frameworks
    find_library(OPENGL_FRAMEWORK OpenGL REQUIRED)
    find_library(COREAUDIO_FRAMEWORK CoreAudio REQUIRED)
    find_library(COREMIDI_FRAMEWORK CoreMIDI REQUIRED)
    find_library(AUDIOTOOLBOX_FRAMEWORK AudioToolbox REQUIRED)
    find_library(AUDIOUNIT_FRAMEWORK AudioUnit REQUIRED)
    find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
    find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
    target_link_libraries(sdlpal PRIVATE
        ${OPENGL_FRAMEWORK}
        ${COREAUDIO_FRAMEWORK}
        ${COREMIDI_FRAMEWORK}
        ${AUDIOTOOLBOX_FRAMEWORK}
        ${AUDIOUNIT_FRAMEWORK}
        ${FOUNDATION_FRAMEWORK}
        ${COCOA_FRAMEWORK}
    )

elseif(EMSCRIPTEN)
    # Emscripten-specific compile/link flags
    if(SDLPAL_USE_SDL3)
        set(EMSCRIPTEN_SDL_OPTION -sUSE_SDL=3)
    else()
        set(EMSCRIPTEN_SDL_OPTION -sUSE_SDL=2)
    endif()
    target_compile_options(sdlpal PRIVATE ${EMSCRIPTEN_SDL_OPTION})
    target_compile_definitions(sdlpal PRIVATE
        "fopen=EMSCRIPTEN_fopen"
        "fclose=EMSCRIPTEN_fclose"
        "main=EMSCRIPTEN_main"
    )
    target_link_options(sdlpal PRIVATE
        ${EMSCRIPTEN_SDL_OPTION}
        -sASYNCIFY=1 -sEXIT_RUNTIME=1
        -sMAX_WEBGL_VERSION=2 -sFULL_ES3
        "-sTOTAL_STACK=5242880"
        "-sEXPORTED_FUNCTIONS=['_EMSCRIPTEN_main']"
        "-sEXPORTED_RUNTIME_METHODS=['cwrap', 'FS', 'IDBFS']"
        -lidbfs.js
    )
    set_target_properties(sdlpal PROPERTIES SUFFIX ".js")

else()
    # Linux / Unix
    find_package(OpenGL REQUIRED)
    find_package(Threads REQUIRED)
    target_link_libraries(sdlpal PRIVATE
        OpenGL::GL
        Threads::Threads
        m
        stdc++
    )

    # FLTK (optional configuration UI)
    if(SDLPAL_USE_FLTK)
        find_package(FLTK QUIET)
        if(FLTK_FOUND)
            target_link_libraries(sdlpal PRIVATE ${FLTK_LIBRARIES})
            target_include_directories(sdlpal PRIVATE ${FLTK_INCLUDE_DIR})
        else()
            message(STATUS "FLTK not found, config UI disabled")
            target_compile_definitions(sdlpal PRIVATE PAL_NO_LAUNCH_UI)
        endif()
    else()
        target_compile_definitions(sdlpal PRIVATE PAL_NO_LAUNCH_UI)
    endif()

    # ALSA (optional native MIDI)
    if(ALSA_FOUND)
        target_link_libraries(sdlpal PRIVATE ALSA::ALSA)
    endif()
endif()

# =============================================================================
# Unit Tests (optional)
# =============================================================================
if(SDLPAL_ENABLE_TESTS AND NOT EMSCRIPTEN)
    if(EXISTS "${CMAKE_SOURCE_DIR}/3rd/googletest/CMakeLists.txt")
        enable_testing()

        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        add_subdirectory("${CMAKE_SOURCE_DIR}/3rd/googletest" EXCLUDE_FROM_ALL)

        file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/tests/*.cpp")

        add_executable(sdlpal-tests ${ALL_SOURCES} ${TEST_SOURCES})

        # Inherit all settings from the main target
        target_include_directories(sdlpal-tests PRIVATE
            "${CMAKE_SOURCE_DIR}"
            "${SDLPAL_PLATFORM_DIR}"
            "${CMAKE_SOURCE_DIR}/sdl_compat"
            "${CMAKE_SOURCE_DIR}/timidity"
        )
        target_compile_definitions(sdlpal-tests PRIVATE
            PAL_HAS_PLATFORM_SPECIFIC_UTILS
            UNIT_TEST=1
        )
        if(SDLPAL_USE_SDL3)
            target_compile_definitions(sdlpal-tests PRIVATE USE_SDL3=1)
        endif()
        if(SDLPAL_HAS_GIT_REVISION)
            target_compile_definitions(sdlpal-tests PRIVATE PAL_HAS_GIT_REVISION)
        endif()
        if(MSVC)
            target_compile_options(sdlpal-tests PRIVATE /utf-8 /W3)
            target_compile_definitions(sdlpal-tests PRIVATE
                _CRT_SECURE_NO_WARNINGS WIN32 _WINDOWS
            )
        else()
            target_compile_options(sdlpal-tests PRIVATE -Wall -O2 -fno-strict-aliasing)
            target_compile_definitions(sdlpal-tests PRIVATE _GNU_SOURCE)
        endif()

        target_link_libraries(sdlpal-tests PRIVATE gtest gtest_main)

        # Re-link SDL
        if(SDLPAL_USE_SDL3)
            if(TARGET SDL3::SDL3-static)
                target_link_libraries(sdlpal-tests PRIVATE SDL3::SDL3-static)
            elseif(TARGET PkgConfig::SDL3)
                target_link_libraries(sdlpal-tests PRIVATE PkgConfig::SDL3)
            elseif(TARGET SDL3::SDL3)
                target_link_libraries(sdlpal-tests PRIVATE SDL3::SDL3)
            endif()
        else()
            if(TARGET PkgConfig::SDL2)
                target_link_libraries(sdlpal-tests PRIVATE PkgConfig::SDL2)
            elseif(TARGET SDL2::SDL2)
                target_link_libraries(sdlpal-tests PRIVATE SDL2::SDL2)
            endif()
        endif()

        # Re-link platform libraries
        if(WIN32)
            target_link_libraries(sdlpal-tests PRIVATE
                opengl32 winmm ole32 oleaut32 imm32
                comctl32 uuid dxguid version setupapi
            )
        elseif(APPLE)
            target_link_libraries(sdlpal-tests PRIVATE
                ${OPENGL_FRAMEWORK} ${COREAUDIO_FRAMEWORK}
                ${COREMIDI_FRAMEWORK} ${AUDIOTOOLBOX_FRAMEWORK}
                ${AUDIOUNIT_FRAMEWORK} ${FOUNDATION_FRAMEWORK}
                ${COCOA_FRAMEWORK}
            )
        else()
            target_link_libraries(sdlpal-tests PRIVATE
                OpenGL::GL Threads::Threads m stdc++
            )
            if(FLTK_FOUND)
                target_link_libraries(sdlpal-tests PRIVATE ${FLTK_LIBRARIES})
                target_include_directories(sdlpal-tests PRIVATE ${FLTK_INCLUDE_DIR})
            else()
                target_compile_definitions(sdlpal-tests PRIVATE PAL_NO_LAUNCH_UI)
            endif()
            if(ALSA_FOUND)
                target_link_libraries(sdlpal-tests PRIVATE ALSA::ALSA)
            endif()
        endif()

        add_test(NAME sdlpal-tests COMMAND sdlpal-tests)
    else()
        message(WARNING "GoogleTest not found in 3rd/googletest, tests disabled")
    endif()
endif()

# =============================================================================
# Installation (optional)
# =============================================================================
install(TARGETS sdlpal RUNTIME DESTINATION bin)

# =============================================================================
# Summary
# =============================================================================
message(STATUS "")
message(STATUS "SDLPAL build configuration:")
message(STATUS "  Platform:     ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Compiler:     ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "  SDL version:  ${SDLPAL_USE_SDL3}")
message(STATUS "  Embedded SDL: ${SDLPAL_USE_EMBEDDED_SDL}")
if(NOT WIN32 AND NOT APPLE AND NOT EMSCRIPTEN)
    message(STATUS "  ALSA MIDI:         ${ALSA_FOUND}")
    message(STATUS "  FLTK config UI:    ${FLTK_FOUND}")
endif()
if(SDLPAL_HAS_GIT_REVISION)
    message(STATUS "  Git revision:      ${PAL_GIT_REVISION}")
endif()
message(STATUS "  Tests:             ${SDLPAL_ENABLE_TESTS}")
message(STATUS "")
