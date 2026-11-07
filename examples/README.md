# Examples

These standalone C++20 applications use the public Opal API. They are source-reviewed examples; they have not been built or run as part of the documentation change.

| Target | Source | What to expect |
| --- | --- | --- |
| `opal_clear` | [clear.cpp](clear.cpp) | Blue window; SDL events, render pass, clearing, submission, presentation, and cleanup. Metal/Vulkan/OpenGL. |
| `opal_triangle` | [triangle.cpp](triangle.cpp) | Orange indexed triangle; backend-specific shaders, vertex layout, uint32 indices, pipeline, pixel viewport. Metal/Vulkan/OpenGL. |
| `opal_compute` | [compute.cpp](compute.cpp) | Hidden-window initialization, 256 GPU float writes, barrier, completion message. Metal/Vulkan only. No public buffer readback API is available, so it does not print computed values. |

From the repository root, with the [backend requirements](../docs/getting-started.md#requirements) installed:

```sh
cmake -S examples -B build/examples -DOPAL_BACKEND=AUTO
cmake --build build/examples
./build/examples/opal_clear
./build/examples/opal_triangle
./build/examples/opal_compute
```

Choose `-DOPAL_BACKEND=METAL`, `VULKAN`, or `OPENGL` explicitly to select a backend. With OpenGL, `opal_compute` is not generated. For multi-configuration generators, select a configuration with `--config Debug` and use its executable directory (for example `build/examples/Debug/opal_triangle.exe` on Windows).

The example CMake project is opt-in: adding Opal to an application does not add these executable targets. It also works as a copyable pattern for an application using `add_subdirectory()` and `Opal::Opal`.

The samples wait for GPU completion each frame to make resource lifetimes easy to follow. They do not demonstrate an optimized multi-frame renderer. Compile/link failures and backend initialization errors are printed to stderr. See [troubleshooting](../docs/backends.md#troubleshooting) for common causes.
