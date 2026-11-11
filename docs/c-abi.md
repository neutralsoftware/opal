# Opal C ABI manual

The C interface in [`opal_c.h`](../abi/include/opal_c.h) exposes Opal's contexts, devices, command buffers, textures, shaders, pipelines, buffers, drawing state, framebuffer attachments, resolves, diagnostics, and acceleration structures. It wraps the existing C++ API without changing its public header. Template buffer uploads become pointer-and-byte-count uploads; overloaded methods have distinct C names. C++ implementation fields and private backend conversion helpers are not part of the C ABI.

Use the [complete function reference](c-abi-reference.md) for signatures and the [C clear-window example](../abi/examples/clear.c) for a complete application. Existing [backend notes](backends.md) and [rendering recipes](recipes.md) apply to both interfaces.

## Integrate with CMake

Your consumer can contain only C sources. Opal itself requires a C++20 toolchain and the dependencies of the chosen backend.

```cmake
cmake_minimum_required(VERSION 3.21)
project(MyRenderer LANGUAGES C CXX)
set(OPAL_BUILD_C_ABI ON CACHE BOOL "")
set(OPAL_BACKEND AUTO CACHE STRING "")
add_subdirectory(external/opal)
add_executable(my_renderer main.c)
target_compile_features(my_renderer PRIVATE c_std_99)
target_link_libraries(my_renderer PRIVATE Opal::C)
```

Include `<opal_c.h>`. The public C header requires only `<stddef.h>` and `<stdint.h>`; it does not expose SDL, GLM, STL, Metal, or Vulkan headers. `Opal::C` links the backend privately. If your application uses SDL for events, also link the SDL target supplied by your project (`SDL3::SDL3-static` or `SDL3::SDL3`). The included example's CMake configuration selects the same SDL target as Opal.

To configure, build, and install when you are ready:

```sh
cmake -S . -B build -DOPAL_BUILD_C_ABI=ON -DOPAL_BUILD_C_ABI_EXAMPLES=ON
cmake --build build --target opal_c opal_c_clear
cmake --install build --prefix ./install
```

These commands are instructions for the consumer; this change was not built or run during implementation. `opal_c` is a shared library: distribute its DLL and import library on Windows, dylib on macOS, or shared object on Linux, plus any shared dependencies in your configuration. Install rules include the C header and both manual files. There is no installed CMake package configuration; the `Opal::C` alias is available through `add_subdirectory`.

## ABI conventions

`opal_get_abi_version()` currently returns `1`. `opal_get_backend()` reports the backend compiled into the library. `AUTO` selects Metal on Apple systems and Vulkan elsewhere; context configuration does not switch the compiled backend.

Opaque handles are pointers to distinct incomplete structs. Every enum-like parameter, result code, and boolean uses a 32-bit integer typedef or `int32_t`. Boolean inputs accept zero for false and any nonzero value for true. Boolean outputs are zero or one. Counts and offsets in byte buffers use `size_t`; bind that to the native pointer width in a foreign-function interface. Arrays of handles are arrays of pointers, not arrays of objects.

Functions and callback pointers use `OPAL_C_CALL` (`__cdecl` on Windows). Structs use natural platform alignment; do not pack them. Matrix parameters are sixteen floats in column-major order, with translation at indices 12–14. All strings are NUL-terminated. Pass explicit arguments for C++ defaults: common values are `-1` for caller/object IDs, `1` for instance counts and unused dispatch dimensions, and `0` for first offsets.

ABI version 1 is the first complete C contract. It supersedes the unfinished context/device scaffold, including its void destroy declarations. The existing C++ API and object layouts remain unchanged.

## Errors and diagnostics

Every fallible entry point returns `OpalResult`. Check the result immediately:

```c
OpalBuffer *buffer = NULL;
OpalResult result = opal_buffer_create(
    OPAL_BUFFER_USAGE_VERTEX_BUFFER, sizeof(vertices), vertices,
    OPAL_MEMORY_USAGE_TYPE_GPU_ONLY, -1, &buffer);
if (result != OPAL_SUCCESS) {
    fprintf(stderr, "%s\n", opal_get_last_error());
    return 1;
}
```

| Result | Meaning |
| --- | --- |
| `OPAL_SUCCESS` | The wrapper/native call completed. This does not certify GPU execution. |
| `OPAL_ERROR_INVALID_ARGUMENT` | Missing or invalid arguments, invalid handle/type, invalid enum, missing device, or invalid recording sequence. |
| `OPAL_ERROR_INITIALIZATION_FAILED` | A window-creation call returned no window. |
| `OPAL_ERROR_OUT_OF_MEMORY` | A C++ allocation threw `std::bad_alloc`. |
| `OPAL_ERROR_UNSUPPORTED` | The wrapper's explicit backend check rejected the operation. |
| `OPAL_ERROR_UNKNOWN` | A native runtime error or other C++ exception; inspect its message. |

`opal_get_last_error()` returns a borrowed, per-thread message containing the exported function name and error details. It remains valid until the next result-returning C call on that thread; successful calls clear it. Copy or print it before cleanup, because destroy calls also update the error state. The three infallible queries (`get_last_error`, `get_abi_version`, `get_backend`) do not clear it. Exception text is truncated to fit a 2048-byte buffer and always terminated. C++ exceptions are caught before returning to C.

Handles are checked against a type-specific live-handle registry before dereferencing. This detects null, wrong-type, foreign, and already-destroyed handles while their addresses have not been reused. As with other pointer APIs, an address reused by a new allocation cannot identify an older handle. Set your variable to `NULL` after destruction. Raw data pointers and output pointers must still point to valid memory of sufficient size; the ABI cannot determine their allocation bounds, recover a device loss, or catch a native access violation.

Internal C++ checks also report invalid inputs through the existing Opal log callback and throw standard exceptions. They cover null device contexts, null pipeline and attachment references, invalid resolve endpoints, invalid dimensions and cubemap faces, buffer offset/size overflow, failed OpenGL context activation, and shader-log output buffers. Metal shader logs are now explicitly terminated. Existing Atlas calls keep their signatures; callers should already catch `std::exception` around rendering work.

Register process-wide log/resource/draw callbacks with `opal_set_log_callback`, `opal_set_resource_callback`, and `opal_set_draw_callback`. Each accepts an independent `void *user_data`; a null callback unregisters it. Events and their strings are borrowed only for the duration of the callback. Copy them if needed. Callbacks must not throw, destroy active rendering objects, or re-enter rendering. A log callback receives native log messages; C-boundary validation errors are retrieved through `opal_get_last_error()`.

Callback registration pairs are copied under a mutex; callbacks run after the mutex is released. A callback already executing may finish after it is unregistered: retain its user data until the rendering threads have stopped using it. Registration is shared with the C++ callback setters, so the most recent setter for each callback category wins. Unregister callbacks before unloading the shared library.

## Ownership and threading

Each successful create/acquire/get-default/get-texture call returns one owned handle. Destroy each returned handle exactly once, including the default framebuffer and the depth-stencil texture accessor's handle. Accessors returning raw native pointers or numeric IDs instead return borrowed values. Destroying `NULL` succeeds.

The C interface retains a resource's device until the resource handle is destroyed. The device retains its context, and the native objects retain the relationships implemented by the C++ API. A pass retains its framebuffer, a framebuffer retains attachments, and a shader program retains attached shaders. Keep buffers, textures, drawing states, pipelines, and acceleration structures alive through command submission and completion, even if a backend currently retains some of them internally.

There is one live rendering device because Opal internally uses `Device::globalInstance`. Acquiring a second device while the first or its resources remain alive is rejected by the C wrapper. Create the C device through `opal_device_acquire` before creating GPU resources. Do not mix C-created and C++-created devices in the same process. Render calls and handle operations, including creation/destruction, belong on one rendering thread; on platforms requiring it, use the main thread. Registries and native graphics state are not generally thread-safe.

Wait for submitted work, destroy command buffers, passes, pipelines, drawing states, resources, framebuffer handles, then device and context handles. An adopted SDL window stays owned by its original owner unless `take_ownership` is true. Raw SDL windows, GL contexts, Metal views/devices/textures, resource IDs, and Vulkan acceleration handles must not be released through unrelated ownership APIs. Call `SDL_Quit()` after all Opal objects are destroyed if your application owns SDL initialization/shutdown.

Constructor output pointers must be writable and must not already contain the only reference to a live handle. They are initialized to `NULL` before creation, including unavailable backend constructors. Other outputs are valid only on success. Array inputs are borrowed during the call and copied where a native vector is required; a null array is accepted only for a zero count where the operation permits emptiness. Shader source is copied by the native implementation.

## First frame

1. Initialize `OpalContextConfiguration` using `opal_context_configuration_default`, optionally set application metadata, and create a context. Alternatively use `opal_context_create` for defaults.
2. Set window flags before calling `opal_context_make_window`, or use `make_window_on_display`, `make_window_from_native`, or `adopt_window`. Native window types are None, Win32, X11, and Wayland as supported by the existing backend. `context_get_window` returns a borrowed SDL window as `void *`.
3. Acquire the device, command buffer, default framebuffer handle, and render pass. Assign the framebuffer to the pass.
4. Poll your platform events. Skip rendering while minimized or when the pixel size is zero.
5. Start recording. For Metal set clear values before `begin_pass`; for Vulkan/OpenGL clear inside the pass. Record draws, end the pass, and commit.
6. On OpenGL call `opal_context_swap_window` to present. Metal/Vulkan presentation is part of commit. Wait before tearing down resources.

`opal_device_submit_command_buffer` mirrors the existing C++ no-op for API parity. Use `opal_command_buffer_commit` for submission. Recording validation catches repeated starts, commit without start, nested passes, drawing outside a pass, and drawing/dispatch without a bound pipeline. `end_pass` may also close a compute encoder without a render pass.

The complete [clear example](../abi/examples/clear.c) follows this sequence and checks each rendering call. It includes SDL only to handle events and minimized windows; the ABI itself does not expose a second event system.

## Shaders and graphics pipelines

Use the shader language for the compiled backend: Metal source for Metal, Slang source or the native accepted SPIR-V representation for Vulkan, and GLSL for OpenGL. The existing [triangle example](../examples/triangle.cpp) contains matching shader sources for each backend; the shader text is identical when supplied from C.

Create a shader with `opal_shader_create_from_source`, compile it, and check `opal_shader_get_status`. A null entry selects `main0`; pass `main` explicitly for GLSL or your actual named entry for Metal/Slang. `opal_shader_for_function` creates a separately owned shader handle selecting another entry. Attach shader handles to a program, link it, and check `opal_shader_program_get_status`.

Shader compilation or linking can report failure through the native status rather than an exception, so an `OPAL_SUCCESS` return alone is insufficient. Retrieve the native log with `opal_shader_get_log` or `opal_shader_program_get_log` into a positive-capacity buffer. Logs are terminated and may be truncated.

Create a pipeline, set its program, attributes, primitive/rasterizer/cull/front-face state, depth/blend/logic state, and viewport; then build it. Use the matching setters before building and follow the backend's existing rebuild rules. Attribute offsets and strides are bytes; attribute size is the number of components. Attribute names are required, and must match your shader interface.

For an indexed triangle with `struct Vertex { float position[2]; };` and initialized `program`, this setup translates directly:

```c
OpalBuffer *vertex_buffer = NULL, *index_buffer = NULL;
OpalDrawingState *drawing = NULL;
OpalPipeline *pipeline = NULL;
OpalVertexAttribute position = {
    "position", OPAL_VERTEX_ATTRIBUTE_TYPE_FLOAT,
    offsetof(struct Vertex, position), 0, 0, 2, sizeof(struct Vertex),
    OPAL_VERTEX_BINDING_INPUT_RATE_VERTEX, 0
};
OpalVertexBinding binding = {
    sizeof(struct Vertex), OPAL_VERTEX_BINDING_INPUT_RATE_VERTEX
};
OpalVertexAttributeBinding source = {position, NULL};
CHECK(opal_buffer_create(OPAL_BUFFER_USAGE_VERTEX_BUFFER,
    sizeof(vertices), vertices, OPAL_MEMORY_USAGE_TYPE_GPU_ONLY, -1, &vertex_buffer));
CHECK(opal_buffer_create(OPAL_BUFFER_USAGE_INDEX_ARRAY,
    sizeof(indices), indices, OPAL_MEMORY_USAGE_TYPE_GPU_ONLY, -1, &index_buffer));
CHECK(opal_drawing_state_create(vertex_buffer, index_buffer, &drawing));
source.source_buffer = vertex_buffer;
CHECK(opal_drawing_state_configure_attributes(drawing, &source, 1));
CHECK(opal_pipeline_create(&pipeline));
CHECK(opal_pipeline_set_shader_program(pipeline, program));
CHECK(opal_pipeline_set_vertex_attributes(pipeline, &position, 1, &binding));
CHECK(opal_pipeline_set_cull_mode(pipeline, OPAL_CULL_MODE_NONE));
CHECK(opal_pipeline_enable_depth_write(pipeline, 0));
CHECK(opal_pipeline_build(pipeline));
```

`CHECK` is the error-and-cleanup macro in the clear example. Snippets assume their named resources exist and must be added to your cleanup path. Index buffers contain `uint32_t` indices. During an active pass:

```c
CHECK(opal_command_buffer_bind_pipeline(commands, pipeline));
CHECK(opal_drawing_state_bind(drawing));
CHECK(opal_command_buffer_bind_drawing_state(commands, drawing));
CHECK(opal_command_buffer_draw_indexed(commands, 3, 1, 0, 0, 0, -1));
```

Set viewport dimensions from the window's pixel size when it changes. Instance attributes use `OPAL_VERTEX_BINDING_INPUT_RATE_INSTANCE`, with divisor 1 for one record per instance; the drawing state can also select an instance buffer explicitly. Tessellation uses patch style and a positive `opal_pipeline_set_patch_vertices` count, followed by `draw_patches` on a supported backend.

Uniform setters cover integer, boolean, scalar/vector floats, and column-major matrices. `opal_pipeline_bind_buffer_data` exposes both C++ vector-upload overloads as bytes. `bind_buffer` and `bind_shader_read_write_buffer` bind actual resource handles by shader name. Backend binding semantics and unsupported operations are inherited from C++.

## Buffers, textures, and offscreen passes

Buffer sizes and update offsets are bytes. Data may be null at creation for uninitialized storage. A nonempty update requires valid readable data. Offset-plus-size overflow is rejected before copying or resizing. Metal/Vulkan retain their existing buffer-growth behavior; OpenGL allocation limits and update bounds still follow its native implementation. Choose GPU-only, CPU-to-GPU, or GPU-to-CPU memory intent using `OpalMemoryUsageType`. The native API has no general buffer map/readback function, so the C ABI does not invent one.

Texture constructors cover regular, multisampled, depth-cubemap, and 3D textures. Data may be null at construction; update functions require data. Specify data format separately from storage format. Allocate enough bytes for the exact native pixel representation, dimensions, layers/faces, and transfer alignment; texture readback receives a pointer without a capacity parameter, matching C++. Readback format and availability vary by backend. Faces are numbered 0–5: +X, -X, +Y, -Y, +Z, -Z. The explicit 3D constructor accepts a depth; the generic constructor preserves C++ behavior and does not add an array-layer argument.

Use wrap/filter setters or combined parameter setters, generate mipmaps, and bind textures by handle with `opal_pipeline_bind_texture`. Numeric-ID bindings and texture arrays mirror the native backend-specific interfaces; native IDs are not portable across backends. `texture_get_info` copies metadata without exposing mutable object internals.

Create an offscreen framebuffer with its dimensions, add attachments by type or color index, optionally attach a depth-stencil buffer, and select draw buffers. `framebuffer_create_empty` preserves the native empty-framebuffer overload. `texture_get_depth_stencil_texture` returns an independently owned handle to the buffer's texture. Assign the framebuffer to a render pass and use the same command sequence as the default framebuffer. `framebuffer_get_status` exposes native completeness checking; check it after attaching your resources.

Multisample resolves use `resolve_action_create`, `create_for_color_attachment`, or `create_for_depth`. `resolve_action_set_options` selects color/depth and a color attachment index (`-1` means all colors). Perform the resolve on the command buffer with valid source and destination sizes/formats/sample counts. Vulkan's `texture_get_supported_sample_count` queries a common count for an array of formats; other backends return unsupported for that query. Spatial upscale returns a boolean availability/result through its output parameter.

## Compute and ray tracing

Compute is available through the existing Metal/Vulkan implementations. Create/compile a compute shader, link its program, create a pipeline, set the compute threadgroup size, and build it. Bind the storage buffer by its shader resource name. Recording follows this pattern:

```c
CHECK(opal_pipeline_bind_shader_read_write_buffer(pipeline, "values", values, -1));
CHECK(opal_command_buffer_start(commands));
CHECK(opal_command_buffer_bind_pipeline(commands, pipeline));
CHECK(opal_command_buffer_dispatch(commands, 256, 1, 1));
CHECK(opal_command_buffer_compute_barrier(commands));
CHECK(opal_command_buffer_commit(commands));
CHECK(opal_command_buffer_wait_for_submitted_work(commands));
```

Dispatch dimensions preserve the native API: Metal and Vulkan interpret them as thread counts and round up to workgroups using the pipeline's compute threadgroup size. On Vulkan, set that size to match the shader's declared local size. Use the [existing compute example](../examples/compute.cpp) for shader declarations and initialization; output stays in the GPU buffer because no general buffer readback is exposed.

Check `opal_device_supports_ray_tracing` before creating acceleration structures. Primitive structures accept full `OpalPrimitiveVertex` records, flat float XYZ positions (the count is floats, a multiple of three), or an array of `OpalGeometry` records. Index counts are counts of `uint32_t`, in triangle triples, and indices must reference existing vertices. Instance records contain a BLAS handle, column-major transform, instance ID, mask, and culling flag. Zero-initialize records and set an identity matrix and mask explicitly; a zero matrix is not identity.

Create and build BLASes, then create/build a TLAS or use `opal_instance_acceleration_structure_build` for the combined native operation. Bind the instance structure at the shader binding and retain input structures until completion. `is_built` reports native build state; synchronize before consuming results. Vulkan exposes native acceleration handles as `uint64_t`. Direct BLAS binding is Metal-only; instance binding exists on Metal/Vulkan. `dispatch_rays` is Vulkan-only in this native implementation; Metal ray queries use the native compute path.

All exported names are present on every backend, including acceleration functions. Operations whose C++ declarations are backend-conditional return `OPAL_ERROR_UNSUPPORTED` when unavailable, rather than removing symbols. Hardware support, shader/pipeline compatibility, and native backend errors still need to be checked; a compiled entry point does not promise hardware support.

## Verification boundary

The implementation was inspected with source-level declaration/definition, enum-mapping, coverage, documentation-link, formatting, and whitespace checks. No project build, compiler syntax check, GPU execution, or Atlas integration run was performed. Run the commands above and exercise your actual backend before relying on runtime compatibility. Static checks are available through `python3 abi/check_contract.py` and do not build the project.
