cmake_minimum_required(VERSION 3.12)

# CMake 4 no longer enables policies older than 3.5 automatically. Some of the
# bundled dependencies still declare an older minimum version, so provide the
# compatibility floor before entering their subdirectories.
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0")
    set(CMAKE_POLICY_VERSION_MINIMUM "3.5")
endif()

project(ninecraft LANGUAGES C CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR
        "Ninecraft requires a 32-bit target. Use '-A Win32' with Visual Studio "
        "or select an i686/ARM32 cross-compiler."
    )
endif()

include(CTest)

include_directories(${PROJECT_SOURCE_DIR}/stb)

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
