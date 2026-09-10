# Opal

Opal is a C++20 rendering abstraction library with Metal, Vulkan, and OpenGL backends. It provides contexts, devices, command buffers, shaders, pipelines, textures, framebuffers, buffers, compute dispatch, and ray-tracing support.

## Add Opal to a CMake project

### Git submodule

```bash
git submodule add https://github.com/neutralsoftware/opal.git external/opal
git submodule update --init --recursive
```

```cmake
set(OPAL_BACKEND AUTO CACHE STRING "Opal rendering backend")
add_subdirectory(external/opal)

target_link_libraries(your_target PRIVATE Opal::Opal)
```

### FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(
    opal
    GIT_REPOSITORY https://github.com/neutralsoftware/opal.git
    GIT_TAG main
)

set(OPAL_BACKEND AUTO CACHE STRING "Opal rendering backend")
FetchContent_MakeAvailable(opal)

target_link_libraries(your_target PRIVATE Opal::Opal)
```

```cpp
#include <opal/opal.h>
```

Applications do not need to include Metal headers or list Apple frameworks themselves. The `Opal::Opal` target supplies the selected backend, public include paths, SDL3, GLM, and the required platform linkage.

## Backend selection

`OPAL_BACKEND` accepts `AUTO`, `METAL`, `VULKAN`, or `OPENGL`.

- `AUTO` selects Metal on Apple platforms and Vulkan elsewhere.
- `METAL` requires an Apple platform and uses the Metal, MetalFX, Cocoa, and QuartzCore system frameworks.
- `VULKAN` requires the Vulkan SDK and SPIRV-Cross CMake packages.
- `OPENGL` uses Opal's bundled GLAD loader and the platform OpenGL package.

GLM and SDL3 are reused when the parent project already provides their CMake targets. Otherwise, Opal downloads its pinned versions during configuration.

## Minimal setup

```cpp
#include <opal/opal.h>

int main() {
    opal::ContextConfiguration configuration;
    configuration.applicationName = "Opal Application";

    auto context = opal::Context::create(configuration);
    context->makeWindow(1280, 720, "Opal Application");

    auto device = opal::Device::acquire(context);
    auto commandBuffer = device->acquireCommandBuffer();

    commandBuffer->start();
    device->submitCommandBuffer(commandBuffer);
}
```

## Logging

Opal writes informational messages to standard output and warnings or errors to standard error by default. Applications can provide a callback:

```cpp
void receiveOpalLog(opal::LogLevel level, const char *message) {
}

opal::setLogCallback(receiveOpalLog);
```

## License

Opal is available under the MIT License. The bundled metal-cpp header retains Apple's Apache 2.0 license notice.
