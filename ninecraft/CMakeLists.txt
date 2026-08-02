cmake_minimum_required(VERSION 3.12)

project(ninecraft-core LANGUAGES C)

file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS src/*.c)

add_executable(ninecraft ${SOURCE_FILES} ../stb/stb_vorbis.c)
target_include_directories(ninecraft PRIVATE include/)
target_link_libraries(ninecraft PRIVATE ${CMAKE_DL_LIBS} zlibstatic SDL2::SDL2 SDL2main glad ancmp)

if(WIN32)
    target_link_libraries(ninecraft PRIVATE iphlpapi comdlg32)
    set_target_properties(ninecraft PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

if(MSVC)
    target_compile_options(ninecraft PRIVATE /utf-8)
    set_property(TARGET ninecraft PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

if(BUILD_TESTING)
    add_test(NAME ninecraft-help COMMAND ninecraft --help)

    add_executable(
        ninecraft-runtime-config-test
        tests/runtime_config_test.c
        src/runtime_config.c)
    target_include_directories(
        ninecraft-runtime-config-test
        PRIVATE include/)
    target_link_libraries(
        ninecraft-runtime-config-test
        PRIVATE SDL2::SDL2)
    set_target_properties(
        ninecraft-runtime-config-test
        PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY
            "${CMAKE_CURRENT_BINARY_DIR}/runtime-config-test-bin/$<CONFIG>")
    if(MSVC)
        target_compile_options(ninecraft-runtime-config-test PRIVATE /utf-8)
    endif()
    add_test(
        NAME ninecraft-runtime-config
        COMMAND ninecraft-runtime-config-test)

    add_executable(
        ninecraft-alt-mouse-mode-test
        tests/alt_mouse_mode_test.c
        src/input/alt_mouse_mode.c)
    target_include_directories(
        ninecraft-alt-mouse-mode-test
        PRIVATE include/)
    set_target_properties(
        ninecraft-alt-mouse-mode-test
        PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY
            "${CMAKE_CURRENT_BINARY_DIR}/alt-mouse-mode-test-bin/$<CONFIG>")
    if(MSVC)
        target_compile_options(ninecraft-alt-mouse-mode-test PRIVATE /utf-8)
    endif()
    add_test(
        NAME ninecraft-alt-mouse-mode
        COMMAND ninecraft-alt-mouse-mode-test)
endif()
