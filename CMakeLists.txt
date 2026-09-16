cmake_minimum_required(VERSION 3.26)
project(MidnaFX VERSION 0.5.1 LANGUAGES CXX)

option(MIDNAFX_BUILD_MOD "Build the native Dusklight mod and .dusk package" ON)
option(MIDNAFX_ALLOW_UNVERIFIED_SDK "Allow a Dusklight checkout other than the researched revision" OFF)
set(DUSKLIGHT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/upstream/dusklight" CACHE PATH "Dusklight checkout")
set(MIDNAFX_DUSKLIGHT_REVISION "edf42c6a7202647b56dd2fcdef02d17671bc814b")

include(CTest)
if(MIDNAFX_BUILD_MOD)
    find_package(Git REQUIRED)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -c "safe.directory=${DUSKLIGHT_DIR}" -C "${DUSKLIGHT_DIR}" rev-parse HEAD
        OUTPUT_VARIABLE _dusklight_revision OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE _revision_result)
    if(NOT MIDNAFX_ALLOW_UNVERIFIED_SDK AND
       (NOT _revision_result EQUAL 0 OR NOT _dusklight_revision STREQUAL MIDNAFX_DUSKLIGHT_REVISION))
        message(FATAL_ERROR "Dusklight must be pinned to ${MIDNAFX_DUSKLIGHT_REVISION}. Override only for deliberate compatibility testing with MIDNAFX_ALLOW_UNVERIFIED_SDK=ON.")
    endif()
    if(NOT EXISTS "${DUSKLIGHT_DIR}/sdk/CMakeLists.txt")
        message(FATAL_ERROR "Dusklight SDK missing. See docs/development.md or set DUSKLIGHT_DIR.")
    endif()
    if(NOT EXISTS "${DUSKLIGHT_DIR}/extern/aurora/cmake/AuroraDawnProvider.cmake")
        message(FATAL_ERROR "Initialize the pinned Dusklight extern/aurora submodule before configuring.")
    endif()
    add_subdirectory("${DUSKLIGHT_DIR}/sdk" dusklight-sdk EXCLUDE_FROM_ALL)
    add_mod(midnafx
        FEATURES game webgpu
        SOURCES src/mod.cpp src/config/grade.cpp src/config/presets.cpp src/config/visual.cpp src/game/camera_probe.cpp src/game/game_state.cpp src/game/twilight.cpp src/render/renderer.cpp src/ui/settings.cpp
        MOD_JSON mod.json
        RES_DIR res
    )
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/passthrough.wgsl")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/shaders/passthrough.wgsl" MIDNAFX_SHADER)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/grading.wgsl")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/shaders/grading.wgsl" MIDNAFX_GRADING_SHADER)
    configure_file(src/render/shader.hpp.in generated/midnafx_shader.hpp @ONLY)
    target_include_directories(midnafx PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_BINARY_DIR}/generated")
    if(MSVC)
        target_compile_options(midnafx PRIVATE /W4)
    else()
        target_compile_options(midnafx PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()
