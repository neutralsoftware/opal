# Backend behavior and troubleshooting

## Shader inputs

Metal consumes Metal Shading Language source, Vulkan compiles Slang source to SPIR-V, and OpenGL consumes GLSL. Opal does not translate one application source string across all three backends. Supply the appropriate source for the selected backend, as in the [triangle example](../examples/triangle.cpp).

Always pass an explicit entry point to `Shader::createFromSource()`. Its default is `main0`; GLSL normally uses `main`. Metal currently falls back to the first function in a library if the requested function cannot be found, so a successful compile alone does not prove that you selected the intended entry.

Metal discovers buffer and texture layouts by parsing source. Use straightforward named parameters and structs with explicit `[[buffer(n)]]` and `[[texture(n)]]` annotations. This parser is not a full MSL compiler/reflection API. Vulkan uses reflected Slang/SPIR-V bindings. Match resource names and host data layouts to the selected shader language.

Compile each shader, check its status, attach it to a program, link the program, check its status, attach it to a pipeline, then build the pipeline. Keep graphics, compute, and Vulkan ray-tracing stages in separate programs. Vulkan graphics programs require vertex and fragment stages, and paired tessellation control/evaluation stages when tessellation is used.

## Known boundaries

- `Device::submitCommandBuffer()` is currently empty. `CommandBuffer::commit()` submits Metal/Vulkan work and presents the default render target when required. OpenGL needs an explicit `SDL_GL_SwapWindow()`.
- OpenGL compute dispatch throws. `UniformBuffer` and `ShaderRead` buffer usages throw on OpenGL. `Pipeline::bindBuffer(name, Buffer)` throws there; the vector/raw-data overloads currently do nothing on OpenGL. Use scalar/vector/matrix uniform setters instead.
- `Pipeline::bindTextureArray()` has Metal and Vulkan definitions, but no OpenGL definition. Metal uses a compute argument buffer; Vulkan uses an image descriptor array in descriptor set zero.
- `ShaderProgram::use()` is declared in the public header but has no implementation. Bind a built pipeline through `CommandBuffer::bindPipeline()` instead.
- Metal rejects fixed-function logic operations during pipeline build. Geometry and tessellation stage enum values do not imply a complete Metal stage implementation; the Metal linker handles vertex, fragment, and compute functions.
- Ray acceleration structures are available only under `METAL` or `VULKAN`. Metal traces rays from compute kernels. `dispatchRays()` is implemented only for Vulkan ray-tracing programs and throws on other backends.
- `performSpatialUpscale()` is an optional Metal/Apple path and returns false when unavailable. Provide a normal rendering or resolve fallback.
- `Texture::readData()` is a low-level operation, not a universal format conversion API. OpenGL's path uses unsigned bytes for color and floats for depth; Metal reads the native texture storage with a limited row-size calculation. Use a compatible format and allocation, wait for GPU writes, and validate the target backend before extending beyond the RGBA8 example.
- Public IDs (`textureID`, `bufferID`, and others) are not portable native GPU handles. Use the typed resources, or explicitly available native accessors.

## Coordinate and state conventions

Metal and Vulkan targets propagate `GLM_FORCE_DEPTH_ZERO_TO_ONE`; OpenGL uses its normal depth convention. Construct projection matrices after including the target's supplied GLM definitions, and verify orientation when sharing projections across backends. Window sizes are logical units while render targets and viewports use pixels; query `SDL_GetWindowSizeInPixels()` for high-density windows.

Pipeline state defaults include triangles, filled polygons, back-face culling, counterclockwise front faces, no blending, no depth testing, and enabled depth writes. Explicitly configure what your pass needs before `build()`. Configure OpenGL vertex array attributes through `DrawingState::configureAttributes()` as well as the pipeline layout used by Metal/Vulkan.

Feature enum values are API vocabulary, not a promise that the selected device supports every combination. Multisample counts, attachment formats, filtering, storage textures, line widths, and geometry/tessellation capabilities depend on the backend and device. The only public specialized capability query is `supportsRayTracing()`.

## Troubleshooting

**The window stays black.** Check shader/program status and exception messages. Confirm a framebuffer was assigned to the render pass, `start()` and `beginPass()` were called, the pipeline was built and bound, and a drawing state was bound for vertex-buffer draws. For Metal set clear values before beginning the pass. For OpenGL swap the window after rendering. Calling only `submitCommandBuffer()` does not submit work.

**The triangle disappears.** Disable culling initially, verify stride and byte offsets, use 32-bit unsigned indices, and query the pixel viewport size. Ensure your shader attribute locations match the host layout.

**Vulkan fails during setup.** Check Vulkan 1.3 driver support, the SDK, SPIRV-Cross CMake packages, recursive submodules, and validation-layer availability. `createValidationLayers` defaults to true; disable it explicitly when validation is not installed.

**Metal shader compilation fails.** Read the thrown exception. Confirm the OS supports the requested MSL 4.0 version and that the source and entry name are MSL functions. Shader log getters are less detailed than compilation exceptions on this backend.

**A shader resource cannot be found.** Use its reflected declaration name, not a texture filename or arbitrary label. For Vulkan texture arrays, the numeric argument selects a binding in descriptor set zero. For Metal argument arrays it selects the compute argument-buffer index.

**An embedded window fails.** `makeWindowFromNative()` wraps Win32 HWND, X11 window IDs, or Wayland surfaces through SDL properties. It is not a general Cocoa view wrapper. For Apple integration use `adoptWindow()` and, when needed, `setMetalTargetView()` before device acquisition. OpenGL window adoption does not create a missing GL context automatically.

The examples and documentation are source-reviewed. They have not been built or run as part of this documentation change.
