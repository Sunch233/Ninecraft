cmake_minimum_required(VERSION 3.12)

# CMake 4 no longer enables policies older than 3.5 automatically. Some of the
# bundled dependencies still declare an older minimum version, so provide the
# compatibility floor before entering their subdirectories.
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0")
    set(CMAKE_POLICY_VERSION_MINIMUM "3.5")
endif()

# Let CMake select the MSVC runtime for every target, including bundled
# dependencies.  This policy has to be enabled before project() initializes
# the MSVC ABI.
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(ninecraft LANGUAGES C CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR
        "Ninecraft requires a 32-bit target. Use '-A Win32' with Visual Studio "
        "or select an i686/ARM32 cross-compiler."
    )
endif()

set(_ninecraft_windows_xp_default OFF)
if(CMAKE_GENERATOR_TOOLSET MATCHES "(^|,)v141_xp(,|$)")
    set(_ninecraft_windows_xp_default ON)
endif()
option(NINECRAFT_WINDOWS_XP
    "Target 32-bit Windows XP SP3 with the MSVC v141_xp toolset"
    ${_ninecraft_windows_xp_default})
unset(_ninecraft_windows_xp_default)

if(NINECRAFT_WINDOWS_XP)
    if(NOT WIN32 OR NOT MSVC)
        message(FATAL_ERROR
            "NINECRAFT_WINDOWS_XP requires a 32-bit MSVC build.")
    endif()
    if(NOT CMAKE_GENERATOR MATCHES "^Visual Studio " OR
       NOT CMAKE_GENERATOR_TOOLSET MATCHES "(^|,)v141_xp(,|$)")
        message(FATAL_ERROR
            "NINECRAFT_WINDOWS_XP requires a Visual Studio generator with "
            "'-A Win32 -T v141_xp'.")
    endif()

    # Keep Windows declarations and conditional code restricted to APIs that
    # exist on 32-bit Windows XP SP3.  v141_xp supplies the Windows 7.1A SDK
    # and configures the PE OS/subsystem version as 5.01.
    add_compile_definitions(
        NINECRAFT_WINDOWS_XP=1
        WINVER=0x0501
        _WIN32_WINNT=0x0501
        NTDDI_VERSION=0x05010300)
    message(STATUS "Ninecraft Windows XP SP3 target enabled (v141_xp)")
endif()

include(CTest)

include_directories(${PROJECT_SOURCE_DIR}/stb)

if(WIN32)
    # The Windows distribution is a single executable.  Libraries without an
    # explicit type (currently glad and ancmp) must not turn into DLLs when a
    # caller has BUILD_SHARED_LIBS enabled in its cache.
    set(BUILD_SHARED_LIBS OFF CACHE BOOL
        "Build bundled Windows dependencies as static libraries" FORCE)
endif()

if(MSVC)
    if(POLICY CMP0091)
        # Release-like configurations use /MT; Debug uses /MTd.
        set(CMAKE_MSVC_RUNTIME_LIBRARY
            "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
        # CMake 3.12-3.14 predates CMP0091.  Preserve the documented minimum
        # version by translating its default runtime flags instead.
        foreach(flag_var
                CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
                CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
                CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
                CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
            if(DEFINED ${flag_var})
                string(REGEX REPLACE "/MDd" "/MTd"
                    ${flag_var} "${${flag_var}}")
                string(REGEX REPLACE "/MD" "/MT"
                    ${flag_var} "${${flag_var}}")
            endif()
        endforeach()
    endif()
    set(SDL_FORCE_STATIC_VCRT ON CACHE BOOL
        "Link SDL against the static MSVC runtime" FORCE)
endif()

SET(SDL_STATIC ON CACHE BOOL "Enable SDL_STATIC" FORCE)
SET(SDL_SHARED OFF CACHE BOOL "Disable SDL_SHARED" FORCE)
SET(SDL_MMX OFF CACHE BOOL "Disable SDL_MMX" FORCE)
SET(SDL_SSE OFF CACHE BOOL "Disable SDL_SSE" FORCE)
SET(SDL_SSE2 OFF CACHE BOOL "Disable SDL_SSE2" FORCE)
SET(SDL_SSE3 OFF CACHE BOOL "Disable SDL_SSE3" FORCE)
SET(SDL_SSEMATH OFF CACHE BOOL "Disable SDL_SSEMATH" FORCE)
SET(SDL_3DNOW OFF CACHE BOOL "Disable SDL_3DNOW" FORCE)
SET(SDL_TEST OFF CACHE BOOL "Disable SDL_TEST" FORCE)
SET(SDL_TESTS OFF CACHE BOOL "Disable SDL_TESTS" FORCE)
SET(SDL2_DISABLE_INSTALL ON CACHE BOOL "Disable SDL installation targets" FORCE)

if(NINECRAFT_WINDOWS_XP)
    # These optional SDL backends target post-XP Windows versions and are not
    # used by Ninecraft's OpenGL/legacy-audio path.
    SET(SDL_WASAPI OFF CACHE BOOL "WASAPI requires Windows Vista" FORCE)
    SET(SDL_VULKAN OFF CACHE BOOL "Vulkan is unavailable on Windows XP" FORCE)
    SET(SDL_SENSOR OFF CACHE BOOL "Windows sensors are unavailable on XP" FORCE)
endif()

SET(ZLIB_BUILD_TESTING OFF CACHE BOOL "Disable zlib tests" FORCE)
SET(ZLIB_BUILD_SHARED OFF CACHE BOOL "Disable the unused shared zlib library" FORCE)
SET(ZLIB_BUILD_STATIC ON CACHE BOOL "Build the static zlib library" FORCE)
SET(ZLIB_INSTALL OFF CACHE BOOL "Disable zlib installation targets" FORCE)

add_subdirectory(glad/cmake)
glad_add_library(glad REPRODUCIBLE LOADER API egl=1.5 gl:compatibility=2.0)
add_subdirectory(zlib)
add_subdirectory(ancmp)
add_subdirectory(SDL)
add_subdirectory(ninecraft)

if(MSVC)
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ninecraft)
endif()
