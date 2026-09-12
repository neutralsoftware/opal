cmake_minimum_required(VERSION 3.21)
project(Opal VERSION 0.1.0 LANGUAGES C CXX)

include(FetchContent)

set(OPAL_BACKEND AUTO CACHE STRING "AUTO, METAL, VULKAN, or OPENGL")
set_property(CACHE OPAL_BACKEND PROPERTY STRINGS AUTO METAL VULKAN OPENGL)

string(TOUPPER "${OPAL_BACKEND}" OPAL_BACKEND)

set(OPAL_RESOLVED_BACKEND "${OPAL_BACKEND}")

if(OPAL_BACKEND STREQUAL "AUTO")
    if(APPLE)
        set(OPAL_RESOLVED_BACKEND METAL)
    else()
        set(OPAL_RESOLVED_BACKEND VULKAN)
    endif()
endif()

message(STATUS "Opal backend: ${OPAL_RESOLVED_BACKEND}")

if(NOT TARGET glm::glm AND NOT TARGET glm::glm-header-only AND NOT TARGET glm-header-only)
    FetchContent_Declare(glm GIT_REPOSITORY https://github.com/g-truc/glm.git GIT_TAG 1.0.1 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(glm)
endif()

if(TARGET glm::glm-header-only)
    set(OPAL_GLM_TARGET glm::glm-header-only)
elseif(TARGET glm::glm)
    set(OPAL_GLM_TARGET glm::glm)
elseif(TARGET glm-header-only)
    set(OPAL_GLM_TARGET glm-header-only)
else()
    message(FATAL_ERROR "Opal could not resolve a GLM target")
endif()

if(NOT TARGET SDL3::SDL3-static AND NOT TARGET SDL3::SDL3 AND NOT TARGET SDL3-static)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git GIT_TAG release-3.2.10 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(SDL3)
endif()

if(TARGET SDL3::SDL3-static)
    set(OPAL_SDL_TARGET SDL3::SDL3-static)
elseif(TARGET SDL3::SDL3)
    set(OPAL_SDL_TARGET SDL3::SDL3)
elseif(TARGET SDL3-static)
    set(OPAL_SDL_TARGET SDL3-static)
else()
    message(FATAL_ERROR "Opal could not resolve an SDL3 target")
endif()

set(OPAL_COMMON_SOURCES
    src/buffer.cpp
    src/command_buffer.cpp
    src/device.cpp
    src/diagnostics.cpp
    src/framebuffer.cpp
    src/pipeline.cpp
    src/ray_tracing.cpp
    src/shaders.cpp
    src/texture.cpp
)

add_library(opal STATIC ${OPAL_COMMON_SOURCES})
add_library(Opal::Opal ALIAS opal)

set_target_properties(opal PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON EXPORT_NAME Opal)
target_compile_features(opal PUBLIC cxx_std_20)
target_compile_definitions(opal PRIVATE OPAL_VERSION="${PROJECT_VERSION}")

target_include_directories(opal
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/metal
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan
        ${CMAKE_CURRENT_SOURCE_DIR}/third-party
)

target_link_libraries(opal PUBLIC
    $<BUILD_INTERFACE:${OPAL_GLM_TARGET}>
    $<BUILD_INTERFACE:${OPAL_SDL_TARGET}>
    $<INSTALL_INTERFACE:glm::glm>
    $<INSTALL_INTERFACE:SDL3::SDL3>
)

if(OPAL_RESOLVED_BACKEND STREQUAL "METAL")
    if(NOT APPLE)
        message(FATAL_ERROR "The Metal backend is available only on Apple platforms")
    endif()

    target_sources(opal PRIVATE
        src/metal/metal_state.cpp
    )

    target_compile_definitions(opal PUBLIC
        METAL
        GLM_FORCE_DEPTH_ZERO_TO_ONE
    )

    find_library(OPAL_METAL_FRAMEWORK Metal REQUIRED)
    find_library(OPAL_METALFX_FRAMEWORK MetalFX REQUIRED)
    find_library(OPAL_COCOA_FRAMEWORK Cocoa REQUIRED)
    find_library(OPAL_QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)

    target_link_libraries(opal PRIVATE
        ${OPAL_METAL_FRAMEWORK}
        ${OPAL_METALFX_FRAMEWORK}
        ${OPAL_COCOA_FRAMEWORK}
        ${OPAL_QUARTZCORE_FRAMEWORK}
    )

elseif(OPAL_RESOLVED_BACKEND STREQUAL "VULKAN")

    find_package(Vulkan REQUIRED)
    find_package(spirv_cross_core REQUIRED)
    find_package(spirv_cross_glsl REQUIRED)
    find_package(spirv_cross_reflect REQUIRED)

    file(GLOB_RECURSE OPAL_VULKAN_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan/*.cpp   )
    target_sources(opal PRIVATE
        ${OPAL_VULKAN_SRCS}
    )

    target_compile_definitions(opal PUBLIC
        VULKAN
        GLM_FORCE_DEPTH_ZERO_TO_ONE
    )

    target_link_libraries(opal PRIVATE
        Vulkan::Vulkan
        spirv-cross-core
        spirv-cross-glsl
        spirv-cross-reflect
    )

elseif(OPAL_RESOLVED_BACKEND STREQUAL "OPENGL")

    find_package(OpenGL REQUIRED)

    target_sources(opal PRIVATE
        third-party/glad/glad.c
    )

    target_compile_definitions(opal PUBLIC OPENGL)

    target_include_directories(opal INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/third-party
    )

    target_link_libraries(opal PRIVATE OpenGL::GL)

else()
    message(FATAL_ERROR "Invalid OPAL_BACKEND=${OPAL_BACKEND}")
endif()