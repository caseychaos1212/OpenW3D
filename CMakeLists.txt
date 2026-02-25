cmake_minimum_required(VERSION 3.25...4.0)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

include(FeatureSummary)
include(CMakeDependentOption)

# We don't support in tree builds, so help people make the right choice.
if (CMAKE_BINARY_DIR STREQUAL CMAKE_SOURCE_DIR)
    message(FATAL_ERROR "Building in-source is not supported! Create a build dir and remove ${CMAKE_SOURCE_DIR}/CMakeCache.txt")
endif()

project(commando LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# This file handles extra settings wanted/needed for different compilers.
include(compilers)

# Set where the build results will end up
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" CACHE PATH "Where to output archives")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" CACHE PATH "Where to output libraries")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" CACHE PATH "Where to output binaries")

option(W3D_CLIENT "Build full game client." ON)
add_feature_info(OpenW3DClient W3D_CLIENT "Build OpenW3D game client")

option(W3D_FDS "Build as headless server." ON)
add_feature_info(OpenW3DHeadless W3D_FDS "Build OpenW3D headless server")

# Do we want to build extra SDK stuff or just the game binary?
option(W3D_TOOLS "Build additional tools." ON)
add_feature_info(OpenW3DTools W3D_TOOLS "Build OpenW3D Mod Tools")

option(SYNTAX_CHECK_ONLY "Syntax check the files only?" OFF)
add_feature_info(SyntaxCheck SYNTAX_CHECK_ONLY "Syntax check files only")
if(SYNTAX_CHECK_ONLY)
    add_compile_options("-fsyntax-only")
endif(SYNTAX_CHECK_ONLY)

# Do we want to build with ffmpeg for audio/video decoding?
option(W3D_BUILD_OPTION_FFMPEG "Build with ffmpeg." OFF)
add_feature_info(FFMpegBuild W3D_BUILD_OPTION_FFMPEG "Build OpenW3D with FFMpeg")

option(W3D_BUILD_QT_TOOLS "Build Qt-based GUI tools." ON)
add_feature_info(QtTools W3D_BUILD_QT_TOOLS "Build Qt front-end tools (WWConfig, WDump, and W3DView on Windows)")

option(W3D_BUILD_LEVELEDIT_QT "Build Qt-based LevelEdit prototype tool." OFF)
add_feature_info(LevelEditQt W3D_BUILD_LEVELEDIT_QT "Build Qt front-end for LevelEdit on Windows")

set(W3D_LEVELEDIT_QT_PROFILE "public" CACHE STRING "LevelEditQt feature profile (public|full).")
set_property(CACHE W3D_LEVELEDIT_QT_PROFILE PROPERTY STRINGS public full)
set(W3D_LEVELEDIT_QT_PROFILE_FULL OFF)

set(W3D_LEVELEDIT_PROFILE "public" CACHE STRING "Legacy LevelEdit profile (public|full).")
set_property(CACHE W3D_LEVELEDIT_PROFILE PROPERTY STRINGS public full)
set(W3D_LEVELEDIT_PROFILE_FULL OFF)

option(W3D_LEVELEDIT_GIT_SCM "Enable Git-backed LevelEdit source-control integration." OFF)
add_feature_info(LevelEditGitScm W3D_LEVELEDIT_GIT_SCM "Enable Git-backed LevelEdit SCM abstractions")

option(W3D_LEVELEDIT_DDB_JSON_MIRROR "Enable deterministic .ddb.json sidecar generation." OFF)
add_feature_info(LevelEditDdbJsonMirror W3D_LEVELEDIT_DDB_JSON_MIRROR "Enable .ddb sidecar JSON output")

option(W3D_FETCHCONTENT_OFFLINE "Disable network access for FetchContent dependencies." OFF)
add_feature_info(OfflineFetchContent W3D_FETCHCONTENT_OFFLINE "Require local source dirs for FetchContent dependencies")

if(NOT W3D_LEVELEDIT_QT_PROFILE STREQUAL "public" AND NOT W3D_LEVELEDIT_QT_PROFILE STREQUAL "full")
    message(FATAL_ERROR "W3D_LEVELEDIT_QT_PROFILE must be 'public' or 'full' (got '${W3D_LEVELEDIT_QT_PROFILE}').")
endif()
if(W3D_LEVELEDIT_QT_PROFILE STREQUAL "full")
    set(W3D_LEVELEDIT_QT_PROFILE_FULL ON)
endif()
add_feature_info(LevelEditQtProfileFull W3D_LEVELEDIT_QT_PROFILE_FULL "Build LevelEditQt with full profile menu surface")

if(NOT W3D_LEVELEDIT_PROFILE STREQUAL "public" AND NOT W3D_LEVELEDIT_PROFILE STREQUAL "full")
    message(FATAL_ERROR "W3D_LEVELEDIT_PROFILE must be 'public' or 'full' (got '${W3D_LEVELEDIT_PROFILE}').")
endif()
if(W3D_LEVELEDIT_PROFILE STREQUAL "full")
    set(W3D_LEVELEDIT_PROFILE_FULL ON)
endif()
add_feature_info(LevelEditProfileFull W3D_LEVELEDIT_PROFILE_FULL "Build legacy LevelEdit with full profile behavior")

if(NOT WIN32)
    add_compile_definitions(__cdecl=)
    add_compile_definitions(__stdcall=)
    add_compile_definitions(_stdcall=)
    add_compile_definitions(stricmp=strcasecmp)
    add_compile_definitions(strnicmp=strncasecmp)
    add_compile_definitions(wcsnicmp=wcsncasecmp)
    add_compile_definitions(wcsicmp=wcscasecmp)
endif()

include(FetchContent)
if(W3D_FETCHCONTENT_OFFLINE)
    set(FETCHCONTENT_FULLY_DISCONNECTED ON CACHE BOOL "Disable all FetchContent download/update steps" FORCE)
    set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "Disable FetchContent update checks" FORCE)
endif()

if(W3D_BUILD_OPTION_FFMPEG)
    include(ffmpeg)
endif()

if(W3D_BUILD_QT_TOOLS)
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "Qt tooling is only supported for 64-bit builds. Configure with a 64-bit generator or set W3D_BUILD_QT_TOOLS=OFF.")
    endif()
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)
    include(qt_tools)
endif()

# Find/Add build dependencies and stubs.
# Only 32bit Windows can make use of these APIs as the libraries shipped with the game.
# Linux needs miles headers to compile, until miles is removed
include(miles)
if(WIN32)
    # Do we want to build with bink for video decoding?
    option(W3D_BUILD_OPTION_BINK "Build with bink." ON)
    add_feature_info(BinkBuild W3D_BUILD_OPTION_BINK "Build OpenW3D with Bink")

    if(W3D_BUILD_OPTION_BINK)
        include(bink)
        target_compile_definitions(binkstub INTERFACE W3D_HAS_BINK)
    endif()
    
    # Do we want to build with miles for audio playback?
    option(W3D_BUILD_OPTION_MILES "Build with miles." ON)
    add_feature_info(MilesBuild W3D_BUILD_OPTION_MILES "Build OpenW3D with Miles Audio")

    if(W3D_BUILD_OPTION_MILES)
        include(miles)
    endif()

    include(dx9)

    option(W3D_BUILD_OPTION_ICU "Build with ICU." OFF)
    add_feature_info(ICUBuild W3D_BUILD_OPTION_ICU "Build OpenW3D with ICU")

    if(W3D_BUILD_OPTION_ICU)
        include(icu)
    endif()
else()
    set(W3D_BUILD_OPTION_ICU ON)
    include(icu)
endif()

include(gamespy)

# Add main build targets
add_subdirectory(Code)

feature_summary(WHAT ENABLED_FEATURES DESCRIPTION "Enabled features:")
feature_summary(WHAT DISABLED_FEATURES DESCRIPTION "Disabled features:")
