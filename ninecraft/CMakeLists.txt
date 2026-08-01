cmake_minimum_required(VERSION 3.12)

project(ninecraft-core LANGUAGES C)

file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS src/*.c)

add_executable(ninecraft ${SOURCE_FILES} ../stb/stb_vorbis.c)
target_include_directories(ninecraft PRIVATE include/)
target_link_libraries(ninecraft PRIVATE ${CMAKE_DL_LIBS} zlibstatic SDL2::SDL2 SDL2main glad ancmp)

if(MSVC)
    set_property(TARGET ninecraft PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

if(BUILD_TESTING)
    add_test(NAME ninecraft-help COMMAND ninecraft --help)

    add_executable(ninecraft-ime-composition-test
        tests/ime_composition_test.c
        src/input/ime_composition.c
    )
    target_include_directories(ninecraft-ime-composition-test PRIVATE include/)
    add_test(NAME ninecraft-ime-composition COMMAND ninecraft-ime-composition-test)
endif()
