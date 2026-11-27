# Install the Opal SDK

An Opal SDK contains the static C++ library, public headers, and CMake package. A standalone source install also includes the pinned GLM headers and static SDL3 library. Each installation has one rendering backend.

## Install once on Windows

Install Git, CMake 3.21 or newer with support for your version of Visual Studio, and Visual Studio or Build Tools with the Desktop development with C++ workload. For Vulkan, install the x64 Vulkan SDK with its SPIRV-Cross CMake packages and Slang compiler. Start a new terminal after installing Vulkan so `VULKAN_SDK` and `PATH` are available.

Run these commands in PowerShell if you need a checkout:

```powershell
git clone https://github.com/neutralsoftware/opal.git
Set-Location opal
```

From the checkout, run:

```powershell
.\Install-Opal.ps1
```

This builds x64 Release with MSVC and installs to `%LOCALAPPDATA%\OpalSDK\Vulkan`. The script sets `Opal_ROOT` in the current PowerShell session and persists it in your user environment. Restart any open IDEs so they pick it up. Administrator privileges are unnecessary.

If PowerShell blocks the script, run `powershell -NoProfile -ExecutionPolicy Bypass -File .\Install-Opal.ps1` from the checkout. This applies to that PowerShell process. Open a new terminal afterward to pick up `Opal_ROOT`.

Use `.\Install-Opal.ps1 -Backend OpenGL` for an OpenGL SDK or `-Prefix 'D:\SDKs\Opal'` to choose the install directory. Use a separate prefix for each backend or toolchain you want to keep. The Windows script installs Release libraries. Build your application as x64 Release with a compatible MSVC toolset and runtime. Debug builds need matching Debug libraries, including Vulkan's SPIRV-Cross dependency.

## Install with CMake on macOS or Linux

Run these commands from the Opal checkout. `AUTO` selects Metal on macOS and Vulkan on Linux. Pass `-DOPAL_BACKEND=OPENGL` to select OpenGL explicitly.

```sh
cmake -S . -B build/sdk -DOPAL_INSTALL=ON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$HOME/.local/opal"
cmake --build build/sdk --config Release --parallel
cmake --install build/sdk --config Release
export Opal_ROOT="$HOME/.local/opal"
```

Add the `export` line to your shell startup file for future sessions. You can also pass `-DOpal_ROOT=/path/to/sdk` when configuring an application, or include the SDK prefix in `CMAKE_PREFIX_PATH`.

Vulkan SDK builds use installed Vulkan, SPIRV-Cross, and Slang libraries. Set `VULKAN_SDK` to the Vulkan SDK prefix. If Slang is installed separately, set `OpalSlang_ROOT` to its prefix at SDK and application configuration time. The matching Vulkan dependencies must be available to SDK consumers. Keep the Slang shared libraries on the runtime search path. On Windows, this is normally handled by the Vulkan SDK's `Bin` directory on `PATH`. Metal uses the system frameworks. Linux source builds also need the SDL development dependencies for your window system and the development libraries for the selected backend.

## Use the installed SDK

```cmake
cmake_minimum_required(VERSION 3.21)
project(MyOpalApp LANGUAGES CXX)

find_package(Opal REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE Opal::Opal)
```

Use `#include <opal/opal.h>` in your application. `Opal::Opal` conveys C++20, the backend definitions, include directories, and required libraries. `find_package(Opal CONFIG REQUIRED)` is equivalent when using the SDK and selects CMake config mode explicitly.

If you switch SDKs and CMake keeps finding the old installation, clear the cached `Opal_DIR` or start with a fresh application build directory. CMake selects the backend from the package it finds and reports it in `Opal_BACKEND`.

To include the optional C ABI, build the SDK with `-DOPAL_BUILD_C_ABI=ON`. Consumers can use `find_package(Opal CONFIG REQUIRED COMPONENTS C)`, link `Opal::C`, and include `<opal_c.h>`. Make the shared library available to the application at runtime.

## Share a prebuilt SDK

Create a Windows ZIP while installing from source:

```powershell
.\Install-Opal.ps1 -PackageDirectory .\dist
```

Add `-NoRegister` if you're preparing an archive and want to keep your current `Opal_ROOT`. On macOS or Linux, run `cpack --config build/sdk/CPackConfig.cmake -G TGZ -C Release -B dist` after the SDK build. Publish the archive as an asset on the corresponding GitHub Release. List the architecture, backend, compiler, and required Vulkan SDK version for each asset.

Users extract the archive to a permanent directory. On Windows, they open PowerShell in the extracted SDK root and run:

```powershell
.\Install-Opal.ps1 -RegisterOnly
```

On macOS or Linux, users set `Opal_ROOT` to the extracted SDK root. A prebuilt SDK skips the Opal build. Users still need a compatible compiler and the backend prerequisites above.

To remove a Windows installation from CMake discovery, clear the user setting with `[Environment]::SetEnvironmentVariable('Opal_ROOT', $null, 'User')` and restart the terminal and IDE. You can then remove the SDK directory.
