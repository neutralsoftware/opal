# Getting started

## Requirements

Use CMake 3.21 or newer, a C++20 compiler, Git, and a graphics driver for your chosen backend. Opal builds a static library named `opal` with the alias `Opal::Opal`.

Set `OPAL_BACKEND` to `AUTO`, `METAL`, `VULKAN`, or `OPENGL`. `AUTO` chooses Metal on Apple platforms and Vulkan elsewhere. This is a build setting: `ContextConfiguration::useOpenGL` does not switch a compiled Metal or Vulkan library to OpenGL.

- Metal needs an Apple platform and the Metal, MetalFX, Cocoa, and QuartzCore frameworks. Shader compilation currently requests Metal language version 4.0; the installed OS and compiler must support it.
- Vulkan needs the Vulkan SDK, Vulkan 1.3 at runtime, SPIRV-Cross CMake packages (`spirv_cross_core`, `spirv_cross_glsl`, `spirv_cross_reflect`), and the Slang submodule.
- OpenGL needs the platform OpenGL package. The default context requests OpenGL 4.1 Core. Compute dispatch is unavailable through Opal's OpenGL backend.

Opal reuses existing GLM and SDL3 CMake targets when present. Otherwise configuration downloads GLM 1.0.1 and SDL 3.2.10. A first configuration therefore needs network access unless dependencies are already provided.

## Add the library

From your application's repository:

```sh
git submodule add https://github.com/neutralsoftware/opal.git external/opal
git submodule update --init --recursive
```

A minimal application `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.21)
project(MyOpalApp LANGUAGES C CXX)
set(OPAL_BACKEND AUTO CACHE STRING "Opal rendering backend")
add_subdirectory(external/opal)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE Opal::Opal)
```

`Opal::Opal` supplies C++20, backend definitions, public include directories, SDL3, GLM, and platform linkage. Do not define `METAL`, `VULKAN`, or `OPENGL` manually in application code.

Alternatively:

```cmake
include(FetchContent)
FetchContent_Declare(
    opal
    GIT_REPOSITORY https://github.com/neutralsoftware/opal.git
    GIT_TAG main
    GIT_SUBMODULES third-party/slang
    GIT_SUBMODULES_RECURSE TRUE
)
set(OPAL_BACKEND AUTO CACHE STRING "Opal rendering backend")
FetchContent_MakeAvailable(opal)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE Opal::Opal)
```

Pin `GIT_TAG` to a reviewed commit for reproducible integrations. The repository currently provides an in-tree CMake target; it does not provide an installed package configuration for `find_package(Opal)`.

## First frame

Copy [clear.cpp](../examples/clear.cpp) to `main.cpp`. It is a complete application that opens a window and displays a blue background until you close it. See [example commands](../examples/README.md) to configure the standalone examples.

The frame sequence is:

1. Create a `Context`, configure window flags, then create the window.
2. Acquire a `Device` before creating GPU resources or compiling shaders.
3. Acquire a command buffer and a render pass targeting `device->getDefaultFramebuffer()`.
4. Poll SDL events every frame and skip rendering while minimized.
5. Call `start()`, set the clear value, begin the pass, issue commands, end the pass, and call `commit()`.
6. With OpenGL, call `SDL_GL_SwapWindow(context->getWindow())` to present.
7. Wait for submitted GPU work before destroying resources.

Metal consumes pending clear values when it creates the render encoder, so clear before `beginPass()`. OpenGL clears the currently bound framebuffer, so clear after `beginPass()`. Vulkan can clear an active pass. The examples handle this difference explicitly.

`Device::submitCommandBuffer()` currently has an empty implementation. Use `CommandBuffer::commit()` to submit Metal/Vulkan work; the former is not a substitute. OpenGL commands execute immediately and its `commit()` does no work.

## Ownership and threading

Factories return `std::shared_ptr`. Keep the context and device alive until every GPU resource has been released. Many factory functions and destructors use `Device::globalInstance`, a raw pointer to the most recently acquired device. Treat the current API as a single-device API; it does not provide an independent device parameter on resource creation.

Keep resources referenced by submitted work alive until `waitForSubmittedWork()` returns. Before changing a buffer still used by the GPU, wait or use a separate buffer for the next frame. The examples wait each frame for clarity; a production renderer can manage multiple frames and resources explicitly.

Run SDL window/event operations and rendering on the application's main thread. Opal does not expose a general thread-safety guarantee. For OpenGL, `makeCurrent()` makes an existing context current on the calling thread; device acquisition also initializes the GL loader.

`Context` destroys its SDL window when `ownsWindow` is true. `adoptWindow(window, false)` borrows an existing window, which must outlive the context. Opal does not call `SDL_Quit()` in its context destructor: the examples call it after their rendering objects leave scope.

Use the factory methods and setters instead of default-constructing resource objects or changing their public IDs. Public native-looking IDs can be internal registry keys on Metal and Vulkan.

## Next steps

The [indexed triangle](../examples/triangle.cpp) adds shader compilation, pipeline state, vertex layout, buffers, and drawing. The [compute example](../examples/compute.cpp) shows shader-readable buffers and dispatch on Metal/Vulkan. For render targets and more advanced features, follow the [recipes](recipes.md).
