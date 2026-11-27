cmake_minimum_required(VERSION 3.21)
project(Opal VERSION 1.0.0 LANGUAGES C CXX)

include(FetchContent)
include(GNUInstallDirs)

option(OPAL_INSTALL "Install the Opal CMake package and bundled dependencies" OFF)
if(OPAL_INSTALL)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
endif()

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
    if(OPAL_INSTALL)
        set(GLM_BUILD_INSTALL ON)
        set(GLM_BUILD_LIBRARY OFF)
        set(OPAL_INSTALL_GLM_LICENSE ON)
    endif()
    FetchContent_Declare(glm GIT_REPOSITORY https://github.com/g-truc/glm.git GIT_TAG 1.0.1 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(glm)
endif()

if(TARGET glm::glm-header-only)
    set(OPAL_GLM_TARGET glm::glm-header-only)
    set(OPAL_INSTALLED_GLM_TARGET glm::glm-header-only)
elseif(TARGET glm::glm)
    set(OPAL_GLM_TARGET glm::glm)
    set(OPAL_INSTALLED_GLM_TARGET glm::glm)
elseif(TARGET glm-header-only)
    set(OPAL_GLM_TARGET glm-header-only)
    set(OPAL_INSTALLED_GLM_TARGET glm::glm-header-only)
else()
    message(FATAL_ERROR "Opal could not resolve a GLM target")
endif()

if(NOT TARGET SDL3::SDL3-static AND NOT TARGET SDL3::SDL3 AND NOT TARGET SDL3-static)
    if(OPAL_INSTALL)
        set(SDL_INSTALL ON)
    endif()
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git GIT_TAG release-3.2.10 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(SDL3)
endif()

if(TARGET SDL3::SDL3-static)
    set(OPAL_SDL_TARGET SDL3::SDL3-static)
    set(OPAL_INSTALLED_SDL_TARGET SDL3::SDL3-static)
elseif(TARGET SDL3::SDL3)
    set(OPAL_SDL_TARGET SDL3::SDL3)
    set(OPAL_INSTALLED_SDL_TARGET SDL3::SDL3)
elseif(TARGET SDL3-static)
    set(OPAL_SDL_TARGET SDL3-static)
    set(OPAL_INSTALLED_SDL_TARGET SDL3::SDL3-static)
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
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/metal
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan
        ${CMAKE_CURRENT_SOURCE_DIR}/third-party
)

target_link_libraries(opal PUBLIC
    $<BUILD_INTERFACE:${OPAL_GLM_TARGET}>
    $<BUILD_INTERFACE:${OPAL_SDL_TARGET}>
    $<INSTALL_INTERFACE:${OPAL_INSTALLED_GLM_TARGET}>
    $<INSTALL_INTERFACE:${OPAL_INSTALLED_SDL_TARGET}>
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

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(opal PRIVATE -Wno-elaborated-enum-base)
    endif()

    include(packaging/OpalMetal.cmake)
    if(NOT TARGET Opal::MetalFrameworks)
        message(FATAL_ERROR "Opal requires Metal, MetalFX, Cocoa, and QuartzCore")
    endif()
    target_link_libraries(opal PRIVATE Opal::MetalFrameworks)

elseif(OPAL_RESOLVED_BACKEND STREQUAL "VULKAN")

    find_package(Vulkan REQUIRED)

    if(OPAL_INSTALL)
        include(packaging/OpalVulkan.cmake)
        if(NOT TARGET spirv-cross-core)
            find_package(spirv_cross_core CONFIG REQUIRED)
        endif()
        if(NOT TARGET Opal::Slang)
            message(FATAL_ERROR "Opal SDK builds require Slang. Set VULKAN_SDK or pass OpalSlang_ROOT to CMake.")
        endif()
        target_link_libraries(opal PRIVATE Opal::Slang)
    else()
        find_package(spirv_cross_core REQUIRED)
        find_package(spirv_cross_glsl REQUIRED)
        find_package(spirv_cross_reflect REQUIRED)
        include(packaging/BuildSlang.cmake)
        target_link_libraries(opal PRIVATE slang spirv-cross-glsl spirv-cross-reflect)
    endif()

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
    )

elseif(OPAL_RESOLVED_BACKEND STREQUAL "OPENGL")

    find_package(OpenGL REQUIRED)

    target_sources(opal PRIVATE
        third-party/glad/glad.c
    )

    target_compile_definitions(opal PUBLIC OPENGL)

    target_include_directories(opal INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third-party>
    )

    target_link_libraries(opal PRIVATE OpenGL::GL)

else()
    message(FATAL_ERROR "Invalid OPAL_BACKEND=${OPAL_BACKEND}")
endif()

option(OPAL_BUILD_C_ABI "Build the C ABI for Opal" OFF)

if(OPAL_BUILD_C_ABI)
    add_subdirectory(abi)
endif()

if(OPAL_INSTALL)
    include(packaging/InstallOpal.cmake)
endif()
