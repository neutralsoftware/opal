# API reference

All names below are in namespace `opal`, declared in [`include/opal/opal.h`](../include/opal/opal.h). `uint` aliases `unsigned int`. Tables group overloads by method name; the header supplies exact signatures. Factories return shared pointers unless noted. Destructors release backend state; release resources before the device and context. See [backend limits](backends.md) before using advanced features.

## Diagnostics

| Symbol | Contract |
| --- | --- |
| `LogLevel` | `Info`, `Warning`, `Error`. |
| `LogCallback` | Function pointer `void(LogLevel, const char *)`. The message is borrowed for the callback duration. |
| `setLogCallback(callback)` | Installs a process-wide log callback. Pass `nullptr` to restore default stdout/stderr logging. |
| `ResourceType` | `Texture`, `Buffer`, `Shader`, `Mesh`. |
| `ResourceOperation` | `Created`, `Loaded`, `Unloaded`. These are event labels, not a complete GPU allocation audit. |
| `ResourceEvent` | `callerObject`: caller label, often a decimal caller ID; `type`, `operation`; `frameNumber`; `sizeMb`: reported size in MiB. |
| `DrawType` | `Draw`, `Indexed`, `Patch`. |
| `DrawEvent` | `callerObject`, `type`, `frameNumber`. |
| `ResourceCallback` | Function pointer `void(const ResourceEvent &)`. Borrowed event. |
| `DrawCallback` | Function pointer `void(const DrawEvent &)`. Borrowed event. |
| `setResourceCallback(callback)`, `setDrawCallback(callback)` | Replace the process-wide event callback; `nullptr` disables it. |

Callbacks run synchronously on the emitting thread. Copy data if it must survive the callback; keep handlers short. Event coverage and reported size differ by resource and backend. Optional `callerId` and `objectId` arguments default to `-1` and identify diagnostic events, not GPU bindings.

## Context and window creation

`OpenGLProfile`: `Core`, `Compatibility`.

`NativeWindowType`: `None`, `Win32`, `X11`, `Wayland`.

`ContextConfiguration` fields:

| Field | Default and meaning |
| --- | --- |
| `useOpenGL` | `true`; honored by the OpenGL setup path; forced false in Metal/Vulkan builds. |
| `majorVersion`, `minorVersion` | `4`, `1`; requested OpenGL version. |
| `profile` | `OpenGLProfile::Core`. |
| `applicationName`, `applicationVersion` | Empty strings; application metadata. Vulkan currently uses a fixed numeric application version rather than parsing `applicationVersion`. |
| `createValidationLayers` | `true`; requests Vulkan validation. |

`Context` methods:

| Method | Behavior and requirements |
| --- | --- |
| `create(config = {})` | Initializes SDL video/gamepad/joystick support and backend context state. Throws on initialization failure. |
| `setDecorated(enabled)` | Stores whether the next created window has decorations. Default true. |
| `setResizable(enabled)` | Stores whether the next created window can resize. Default true. |
| `setTransparent(enabled)` | Stores transparent-window request. Default false. |
| `setAlwaysOnTop(enabled)` | Stores the always-on-top request applied during window creation. Default false. |
| `setSamples(value)` | Stores requested OpenGL default-window multisampling count. Default zero. Offscreen MSAA uses multisampled textures separately. |
| `setHighPixelDensity(enabled)` | Stores high-density window request. Default true. |
| `setHidden(enabled)` | Stores hidden-window request. Default false. |
| `makeWindow(width, height, title, displayID = 0)` | Creates and owns an SDL window; dimensions are logical window units. A nonzero display ID places it at that display's origin. Returns `SDL_Window *`. |
| `makeWindowFromNative(nativeWindow, type, width, height, title)` | Creates an owned SDL wrapper for a Win32 HWND, X11 window ID cast through `uintptr_t`, or Wayland surface pointer. Rejects null/`None`. |
| `adoptWindow(existingWindow, takeOwnership = false)` | Adopts or borrows a non-null SDL window. The caller keeps a borrowed window alive. |
| `setMetalTargetView(view)`, `getMetalTargetView()` | Set/get an opaque Apple view pointer for Metal integration; set before device acquisition. |
| `getWindow()` | Returns the window; throws if none exists. |
| `makeCurrent()` | Makes an existing SDL GL context current; does nothing if the window/context is missing. |
| `~Context()` | Releases backend context state, GL context, and an owned SDL window. Does not call `SDL_Quit()`. |

Window setters store creation options; they do not generally reconfigure an existing window. Use SDL APIs for runtime window changes. Create or adopt a window once during setup.

Exposed state: `window = nullptr`, `glContext = nullptr`, `ownsWindow = true`, `metalTargetView = nullptr`, `config`, and the option fields `decorated`, `resizable`, `transparent`, `alwaysOnTop`, `highPixelDensity`, `hidden`, `samples`. Prefer the methods above over direct mutation.

## Device

| Method | Behavior |
| --- | --- |
| `acquire(context)` | Acquires backend device state for the context, initializes the OpenGL loader where applicable, and sets `globalInstance`. Create the window first for window rendering. |
| `acquireCommandBuffer()` | Creates a command buffer associated with this device. |
| `submitCommandBuffer(commandBuffer)` | Currently empty on all backends. Call `commandBuffer->commit()` instead. |
| `getDefaultFramebuffer()` | Lazily returns a framebuffer representing the window/swapchain. Initial dimensions are zero; do not use these initial values as a viewport size. |
| `getDeviceInfo()` | Returns `DeviceInfo` strings: `deviceName`, `vendorName`, `driverVersion`, `renderingVersion`, `opalVersion`. Backend reporting may vary. |
| `supportsRayTracing()` | Reports hardware/backend ray-tracing support; false on OpenGL. Does not imply support for Vulkan-style ray dispatch on Metal. |
| `getMetalDevice()` | Metal-only borrowed `MTL::Device *`; ownership remains with Opal. |
| `~Device()` | Releases backend device state and clears `globalInstance` when it refers to this device. |

Exposed fields: `frameCount = 0`, `context = nullptr`, and static raw pointer `globalInstance`. Frame counting differs by backend; do not use it as application simulation time. See [lifetime guidance](getting-started.md#ownership-and-threading).

## Textures

### Texture enums

| Enum | Values |
| --- | --- |
| `TextureType` | `Texture2D`, `TextureCubeMap`, `Texture3D`, `Texture2DArray`, `Texture2DMultisample`. Use the dedicated 3D/MSAA factories where appropriate. |
| `TextureFormat` | `Rgba8`, `sRgba8`, `Rgb8`, `sRgb8`, `Rgba16F`, `Rgb16F`, `Depth24Stencil8`, `DepthComponent24`, `Depth32F`, `Red8`, `Red16F`. These describe GPU storage; exact native mapping varies. |
| `TextureDataFormat` | `Rgba`, `Rgb`, `Red`, `Bgr`, `Bgra`, `DepthComponent`. Describes CPU data channel order, not a complete CPU element type. |
| `TextureWrapMode` | `Repeat`, `MirroredRepeat`, `ClampToEdge`, `ClampToBorder`. |
| `TextureFilterMode` | `Nearest`, `Linear`, `NearestMipmapNearest`, `LinearMipmapLinear`. Use `Nearest` or `Linear` for magnification. |
| `TextureAxis` | `S`, `T`, `R`: horizontal, vertical, depth texture coordinates. |

### Texture methods

| Method | Behavior and input |
| --- | --- |
| `create(type, format, width, height, dataFormat = Rgba, data = nullptr, mipLevels = 1)` | Allocates storage with optional CPU data. Use positive pixel dimensions. No CPU depth/layer parameter is supplied by this overload. |
| `createMultisampled(format, width, height, samples = 4)` | Allocates a multisampled attachment. Resolve into a single-sample texture before normal sampling/readback. Sample support is device-dependent. |
| `createDepthCubemap(format, resolution)` | Allocates six square depth faces; use `DepthComponent24` or `Depth32F`. |
| `create3D(format, width, height, depth, dataFormat = Rgba, data = nullptr)` | Allocates a volume texture. The API's 3D upload path expects float data; match format/backend layout. |
| `updateFace(faceIndex, data, width, height, dataFormat = Rgba)` | Uploads a cube face; indices 0–5 are +X, -X, +Y, -Y, +Z, -Z. |
| `updateData3D(data, width, height, depth, dataFormat = Rgba)` | Updates volume data. Supply a complete valid CPU allocation for the volume. |
| `updateData(data, width, height, dataFormat = Rgba)` | Uploads 2D data. Storage changes vary by backend; recreate the texture when resizing for predictable ownership. |
| `changeFormat(newFormat)` | Changes the format field and backend format mapping; do not treat it as a portable conversion preserving existing pixels. |
| `changeBorderColor(borderColor)` | Sets the border color for border addressing where supported. |
| `readData(buffer, dataFormat = Rgba)` | Copies GPU texture data to caller-owned memory. No output capacity is supplied; allocate the full compatible image and synchronize GPU writes first. See [readback limits](backends.md#known-boundaries). |
| `generateMipmaps(levels)` | Requests mip generation; allocation and interpretation of `levels` vary by backend. |
| `automaticallyGenerateMipmaps()` | Computes a full chain length from dimensions and generates it. |
| `setWrapMode(axis, mode)` | Sets one coordinate axis's addressing mode. |
| `setFilterMode(minFilter, magFilter)` | Sets minification/magnification filtering. A mip filter needs valid mip levels. |
| `setParameters(wrapS, wrapT, minFilter, magFilter)` | Sets 2D addressing/filtering together. |
| `setParameters3D(wrapS, wrapT, wrapR, minFilter, magFilter)` | Sets all three axes and filtering together. |
| `getMetalTexture()` | Metal-only borrowed `MTL::Texture *`. |
| `~Texture()` | Releases backend texture state. |

Exposed metadata: `textureID = 0`, `type = Texture2D`, `format = Rgba8`, `width = 0`, `height = 0`, `mipLevels = 1`, `samples = 1`. Factories fill these fields; do not assign a foreign native handle to `textureID` on Metal/Vulkan.

## Shaders and programs

`ShaderType` values: `Vertex`, `Fragment`, `Geometry`, `TessellationControl`, `TessellationEvaluation`, `Compute`, `RayGeneration`, `Miss`, `ClosestHit`, `AnyHit`, `Intersection`, `Callable`. Available stages depend on backend and hardware.

| `Shader` method | Behavior |
| --- | --- |
| `createFromSource(source, type, entry = "main0")` | Copies a null-terminated source string. Does not compile. Supply GLSL, MSL, or Slang for the selected backend. |
| `compile()` | Compiles against the current device/context. Metal/Vulkan failures throw; OpenGL status must be checked. |
| `getShaderStatus()` | Whether compilation succeeded. |
| `getShaderLog(logBuffer, bufferSize)` | Writes diagnostics into caller storage. Use a non-null, positive-size buffer; zero-initialize it and force its last byte to zero before treating it as a C string. Metal primarily reports errors through exceptions. |
| `forFunction(functionName, type)` | Creates a new shader from the same copied source with a different function name/type. The new shader still needs compilation. |
| `~Shader()` | Releases backend state and the copied source. |

Exposed fields: `shaderID`, `type`, owned `source = nullptr`, `functionName`; static `currentId` on Metal/Vulkan. Do not free or replace `source`, or modify IDs/counters.

| `ShaderProgram` method | Behavior |
| --- | --- |
| `create()` | Creates an empty program. |
| `attachShader(shader, callerId = -1)` | Attaches and retains a non-null shader. Compile it before linking. |
| `link()` | Links/reflects attached shaders and determines compute/ray-tracing program kind. Vulkan validates stage combinations. |
| `use()` | Declared but currently undefined; use a built pipeline's bind path. |
| `getProgramStatus()` | Whether linking succeeded. |
| `getProgramLog(logBuffer, bufferSize)` | Writes program diagnostics; apply the same buffer precautions as shader logs. Exceptions can carry more useful backend errors. |
| `isComputeProgram()`, `isRayTracingProgram()` | Return program-kind flags set by linking. |
| `~ShaderProgram()` | Releases backend program state. |

Exposed fields: `programID`, `attachedShaders`, and static `currentId` on Metal/Vulkan. Modify attachments through `attachShader()` and link before building a pipeline.

## Vertex layouts and drawing state

| Type | Values or fields |
| --- | --- |
| `VertexAttributeType` | `Float`, `Double`, `Int`, `UnsignedInt`, `Short`, `UnsignedShort`, `Byte`, `UnsignedByte`. Native support varies. |
| `VertexBindingInputRate` | `Vertex`, `Instance`. |
| `VertexAttribute` | `name` matches shader input; `type` is component type; `offset` is byte offset; `location` is shader location; `normalized = false`; `size` is component count; `stride` is bytes per vertex; `inputRate = Vertex`; `divisor = 0` (per-vertex, or one by default for instance inputs). Initialize all fields without defaults. Equality compares every field. |
| `VertexBinding` | `stride = 0` bytes, `inputRate = Vertex`. Describes the pipeline vertex stream. |
| `VertexAttributeBinding` | A `VertexAttribute attribute` and optional `sourceBuffer = nullptr`; null uses the drawing state's primary vertex buffer. |

`DrawingState` methods:

| Method | Behavior |
| --- | --- |
| `create(vertexBuffer, indexBuffer = nullptr)` | Stores geometry buffers and creates a VAO on OpenGL. Index data for indexed draws is 32-bit unsigned integers. |
| `setBuffers(vertexBuffer, indexBuffer = nullptr)` | Replaces primary buffers; reconfigure attributes if the OpenGL layout/source changes. |
| `configureAttributes(bindings)` | Configures OpenGL VAO attributes; on Metal/Vulkan identifies the optional instance buffer. Also call `Pipeline::setVertexAttributes()` for pipeline layout. |
| `bind()`, `unbind()` | Low-level OpenGL VAO/buffer binding. Metal/Vulkan drawing uses the command buffer's drawing state instead. |

Exposed fields: `vertexBuffer`, `indexBuffer`, `instanceBuffer` (all initially null), and `index` (OpenGL VAO identifier; not portable).

## Buffers

`BufferUsage` values:

- `VertexBuffer`: vertex stream.
- `IndexArray`: uint32 index stream.
- `GeneralPurpose`: general buffer (OpenGL maps this to array-buffer storage).
- `UniformBuffer`: shader constants; not supported by OpenGL buffer methods.
- `ShaderRead`: shader-readable storage; not supported by OpenGL buffer methods.
- `ShaderReadWrite`: writable shader storage; OpenGL dispatch remains unsupported.

`MemoryUsageType`: `GPUOnly`, `CPUToGPU`, `GPUToCPU`. These are intent hints; Metal currently uses shared storage and Vulkan uses host-visible/coherent storage in this implementation. No public buffer mapping or readback method is exposed.

| `Buffer` method | Behavior |
| --- | --- |
| `create(usage, size, data = nullptr, memoryUsage = GPUOnly, callerId = -1)` | Allocates `size` bytes, optionally initialized from CPU data. Create after device acquisition. |
| `updateData(offset, size, data)` | Copies `size` bytes from CPU memory at byte `offset`. Stay within the original allocation for portability; Metal/Vulkan can reallocate, while OpenGL uses subdata. Synchronize before overwriting in-flight data. |
| `bind(callerId = -1)`, `unbind(callerId = -1)` | Low-level OpenGL target binding and resource events where implemented. Use pipeline buffer binding for shader resources. |
| `~Buffer()` | Releases backend buffer state. |

Exposed fields: `bufferID`, `usage`, `memoryUsage`. IDs are backend-specific; no portable native buffer accessor exists.

## Pipeline

A pipeline combines a linked program, vertex layout, rasterization/depth/blend state, and shader resource bindings. Configure it before `build()`. Backend pipeline rebuilding/caching varies; rebuilding after fixed-state changes is the conservative choice.

### State enums

| Enum | Values |
| --- | --- |
| `PrimitiveStyle` | `Points`, `Lines`, `LineStrip`, `Triangles`, `TriangleStrip`, `TriangleFan`, `Patches`. |
| `RasterizerMode` | `Fill`, `Line`, `Point`. |
| `CullMode` | `None`, `Front`, `Back`, `FrontAndBack`. |
| `FrontFace` | `Clockwise`, `CounterClockwise`. |
| `CompareOp` | `Never`, `Less`, `Equal`, `LessEqual`, `Greater`, `NotEqual`, `GreaterEqual`, `Always`. |
| `BlendFunc` | `Zero`, `One`, `SrcColor`, `OneMinusSrcColor`, `DstColor`, `OneMinusDstColor`, `SrcAlpha`, `OneMinusSrcAlpha`, `DstAlpha`, `OneMinusDstAlpha`. |
| `BlendEquation` | `Add`, `Subtract`, `ReverseSubtract`, `Min`, `Max`. |
| `LogicOp` | `Clear`, `And`, `AndReverse`, `Copy`, `AndInverted`, `NoOp`, `Xor`, `Or`, `Nor`, `Equivalent`, `Invert`, `OrReverse`, `CopyInverted`, `OrInverted`, `Nand`, `Set`. Metal rejects enabled logic operations. |

### Construction and fixed state

| Method | Behavior; initial state |
| --- | --- |
| `create()` | Creates an unbuilt pipeline. |
| `setShaderProgram(program)` | Associates a linked program; exposed as `shaderProgram`. |
| `setVertexAttributes(attributes, binding)` | Sets shader locations, component formats, offsets, stride, and stream rate. |
| `setPrimitiveStyle(style)` | Topology; initially `Triangles`. |
| `setPatchVertices(count)`, `getPatchVertices()` | Control points per tessellation patch; initially 4. Configure with `Patches`. |
| `setViewport(x, y, width, height)` | Pixel viewport; initial fields all zero. Use positive target dimensions. |
| `setRasterizerMode(mode)` | Initially `Fill`. |
| `setCullMode(mode)` | Initially `Back`. |
| `setFrontFace(face)` | Initially `CounterClockwise`. |
| `setLineWidth(width)` | Initially 1.0; native limits apply. |
| `setComputeThreadgroupSize(x, y = 1, z = 1)` | Host dispatch group dimensions; initial state is 8 × 8 × 1. Match shader local size on Vulkan. |
| `getComputeThreadgroupSizeX()`, `getComputeThreadgroupSizeY()`, `getComputeThreadgroupSizeZ()` | Return the configured group dimensions. These are three separate getters. |
| `enableDepthTest(enabled)` | Initially false. Needs a depth attachment for meaningful testing. |
| `setDepthCompareOp(op)` | Initially `Less`. |
| `enableDepthWrite(enabled)` | Initially true. |
| `enableBlending(enabled)` | Initially false. |
| `setBlendFunc(srcFactor, dstFactor)` | Initially `One`, `Zero`. Straight-alpha blending typically uses `SrcAlpha`, `OneMinusSrcAlpha`. |
| `setBlendEquation(equation)` | Initially `Add`. |
| `enableLogicOp(enabled)`, `setLogicOp(op)` | Initially false and `Copy`. |
| `setColorWriteMask(red, green, blue, alpha)` | Initially all true. |
| `enableMultisampling(enabled)` | Initially false; match framebuffer samples. |
| `enablePolygonOffset(enabled)` | Initially false. |
| `setPolygonOffset(factor, units)` | Depth bias parameters; initially zero. |
| `enableClipDistance(index, enabled)` | Controls a shader clip distance; shader/backend support required. |
| `build()` | Builds backend pipeline state from the linked program; OpenGL defers vertex layout to the VAO. Vulkan can defer target-specific graphics pipeline creation until draw. |
| `bind()` | Applies pipeline state directly. Prefer `CommandBuffer::bindPipeline()` so draws/dispatches track the bound pipeline. |
| `operator==(shared_ptr<Pipeline>)` | Compares selected pipeline state; not a general identity or exhaustive resource comparison. |
| `~Pipeline()` | Releases backend pipeline state. |

Public state fields: `shaderProgram`; `multisamplingEnabled = false`; `viewportX`, `viewportY`, `viewportWidth`, `viewportHeight` (zero); `polygonOffsetEnabled = false`, `polygonOffsetFactor = 0`, `polygonOffsetUnits = 0`. Prefer setters.

### Uniforms and resources

| Method | Behavior |
| --- | --- |
| `setUniform1f(name, value)`, `setUniform1i(name, value)` | Set named float/int uniforms. |
| `setUniformBool(name, value)` | Sets a named boolean; host/shader layout remains backend-specific. |
| `setUniform2f(name, x, y)`, `setUniform3f(name, x, y, z)`, `setUniform4f(name, x, y, z, w)` | Set named float vectors. |
| `setUniformMat4f(name, matrix)` | Sets a named GLM 4 × 4 matrix. Match shader convention/layout. |
| `bindBuffer<T>(name, const vector<T> &)` | Forwards `data()` and `size() * sizeof(T)` to `bindBufferData()`. No serialization or layout conversion. |
| `bindBuffer<T>(name, shared_ptr<vector<T>>)` | Same forwarding for a non-null vector; no work for null. Does not retain the vector as a GPU resource. |
| `bindBufferData(name, data, size)` | Updates named uniform data in bytes for Metal/Vulkan; currently no work on OpenGL. Null/empty data or missing program does no work. |
| `bindBuffer(name, shared_ptr<Buffer>, callerId = -1)` | Binds a named uniform/storage buffer. Metal requires a shader-readable usage; Vulkan validates the reflected descriptor type. Throws on OpenGL. |
| `bindShaderReadWriteBuffer(name, buffer, callerId = -1)` | Binds compute-stage writable shader storage on Metal/Vulkan; throws on OpenGL. Use `ShaderReadWrite` buffers and the matching shader declaration. |
| `bindTexture(name, texture, unit, callerId = -1)` | Binds a typed texture by shader resource name. `unit` is an OpenGL texture unit; Metal can resolve it from source annotations; Vulkan resolves by reflected name. |
| `bindTexture2D(name, textureId, unit, callerId = -1)` | Binds a 2D handle; native GL name on OpenGL, Opal registry ID on Metal/Vulkan. |
| `bindTexture3D(name, textureId, unit, callerId = -1)` | Equivalent handle path for 3D textures. |
| `bindTextureCubemap(name, textureId, unit, callerId = -1)` | Equivalent handle path for cube textures. |
| `bindTextureArray(textures, bufferIndex)` | Metal compute argument-buffer array or Vulkan set-zero image descriptor array. No OpenGL implementation. |

For OpenGL, bind the pipeline before uniform/texture setters so `glUniform` affects the intended program. With Metal/Vulkan configure resource bindings before the draw or dispatch that consumes them. Host structs must match shader alignment and padding; `glm::vec3` and C++ `bool` are not portable packed shader layouts.

## Attachments, framebuffers, and passes

`Attachment::Type`: `Color`, `Depth`, `Stencil`, `DepthStencil`. An `Attachment` has `type` and `shared_ptr<Texture> texture`. Initialize both. Equality compares attachment type and texture type/format, not texture identity or dimensions; both textures must be non-null.

| `DepthStencilBuffer` member | Behavior |
| --- | --- |
| `create(width, height, format = Depth24Stencil8)` | Creates a depth/stencil attachment wrapper. |
| `texture` | Texture representation used by texture-based backends. |
| `renderbufferID` | OpenGL renderbuffer identifier; not a portable handle. |

| `Framebuffer` method | Behavior |
| --- | --- |
| `create(width, height)` | Creates an offscreen framebuffer with recorded dimensions; add compatible attachments. |
| `create()` | Creates a framebuffer without a supplied size. Prefer explicit dimensions for offscreen work. |
| `addAttachment(attachment)` | Adds a typed texture attachment. Keep formats, dimensions, and sample counts compatible. |
| `attachDepthStencilBuffer(buffer)` | Adds a depth/stencil wrapper. |
| `attachTexture(texture, attachmentIndex)` | Attaches a color texture at a zero-based color index. |
| `attachCubemap(texture, attachmentType)` | Attaches a cube texture for layered rendering where supported. |
| `attachCubemapFace(texture, face, attachmentType)` | Attaches one cube face, indices 0–5 (+X, -X, +Y, -Y, +Z, -Z). |
| `disableColorBuffer()` | Configures depth-only rendering without color reads/writes. |
| `setViewport()` | Uses this framebuffer's stored dimensions. |
| `setViewport(x, y, viewWidth, viewHeight)` | Applies an explicit pixel viewport. Prefer the pipeline viewport for portable draw setup. |
| `bind()`, `unbind()` | Low-level framebuffer binding; render passes manage render targets for Metal/Vulkan. |
| `bindForRead()`, `bindForDraw()` | Selects low-level read/draw framebuffer roles where implemented. |
| `setDrawBuffers(attachmentCount)` | Enables color output slots 0 through count minus one. Match fragment outputs. |
| `getDrawBufferCount()` | Returns the configured count, initially -1 (unspecified). |
| `getStatus()` | Backend completeness/status check; not exhaustive validation of every future pipeline/target combination. |
| `~Framebuffer()` | Releases backend framebuffer state. |

Exposed fields: `framebufferID`, `width`, `height`, `attachments`, `isDefaultFramebuffer = false`, `colorBufferDisabled = false`. Use the device accessor for the actual default framebuffer; do not manufacture one by changing the flag.

`RenderPass::create()` makes a pass; `setFramebuffer(framebuffer)` assigns its target, exposed as `framebuffer`. Assign a non-null target before `beginPass()`.

| `ResolveAction` factory | Behavior |
| --- | --- |
| `create(source, destination)` | Creates a resolve/copy description for all color attachments and depth. |
| `createForColorAttachment(source, destination, colorAttachmentIndex)` | Selects one zero-based color attachment. |
| `createForDepth(source, destination)` | Selects depth only. |

Resolve fields are public: `source`, `destination`, `colorAttachmentIndex = -1` (all), `resolveDepth = true`, `resolveColor = true`. Factories adjust selectors; execute with `performResolve()`. Use compatible dimensions and formats, a multisampled source and single-sample destination for MSAA resolving, and verify backend support for the selected attachment operation.

## Command buffer

Acquire from `Device`; do not construct directly. Keep its device alive. Metal/Vulkan record commands, whereas OpenGL generally executes them immediately.

| Method | Behavior and ordering |
| --- | --- |
| `start()` | Starts/resets recording and bound pipeline/drawing/pass/scissor state. Call before recording each frame/batch. |
| `beginPass(renderPass)` | Begins rendering to the pass's non-null framebuffer. End an existing pass before beginning another. |
| `beginSampled(readFramebuffer, writeFramebuffer)` | Calls low-level read/draw framebuffer binds when non-null. This is not a portable substitute for `beginPass()`. |
| `endPass()` | Ends the current render pass/encoder. Required before Vulkan `commit()` or compute work. |
| `commit()` | Submits recorded Metal/Vulkan commands and presents a used default target. OpenGL requires application window swapping. |
| `waitForSubmittedWork()` | Waits for submitted Metal/Vulkan GPU work. Call after commit, before resource readback/destruction or CPU overwrites. |
| `bindPipeline(pipeline)`, `unbindPipeline()` | Sets/clears the command buffer's tracked pipeline. Bind a built pipeline before drawing/dispatching. |
| `setScissor(x, y, width, height)` | Sets a pixel scissor; rejects negative sizes. Backend coordinate orientation follows its render target conventions. |
| `resetScissor()` | Removes the custom scissor. |
| `bindDrawingState(state)`, `unbindDrawingState()` | Sets/clears tracked vertex/index/instance buffers. OpenGL draws bind its VAO automatically. |
| `draw(vertexCount, instanceCount = 1, firstVertex = 0, firstInstance = 0, objectId = -1)` | Draws non-indexed primitives inside a pass. `firstVertex` is a vertex offset. OpenGL currently uses triangle topology here and does not forward `firstInstance`. |
| `drawIndexed(indexCount, instanceCount = 1, firstIndex = 0, vertexOffset = 0, firstInstance = 0, objectId = -1)` | Draws uint32 indices from the bound index buffer. `firstIndex` counts indices, not bytes. Requires vertex and index buffers. OpenGL currently uses triangles and ignores `vertexOffset`/`firstInstance`. |
| `drawPatches(vertexCount, firstVertex = 0, objectId = -1)` | Patch draw; configure `Patches`, patch size, and supported tessellation stages. Metal's path is not a complete tessellation pipeline. |
| `dispatch(threadCountX, threadCountY = 1, threadCountZ = 1)` | Dispatches a compute program outside a render pass. Counts are total logical threads, rounded up by configured group size. Bounds-check in the shader. Throws on OpenGL. |
| `dispatchRays(width, height = 1, depth = 1)` | Dispatches a Vulkan ray-tracing program outside rendering. Dimensions define the ray launch grid. Throws on Metal/OpenGL. |
| `computeBarrier()` | Orders preceding compute memory writes before dependent work. Use between dependent dispatches. Does not replace a CPU completion wait. |
| `generateMipmaps(texture)` | Generates mipmaps using the command recording path where available. Allocate compatible mip storage first. |
| `performResolve(action)` | Executes a resolve/copy action. End rendering first for transfer operations. |
| `performSpatialUpscale(sourceTexture)` | Attempts Metal spatial upscaling to the current presentation path; returns false when unavailable. |
| `clearColor(r, g, b, a)` | Sets/applies a color clear. Metal: call before pass creation; OpenGL: after target binding; Vulkan: active-pass clear is supported. |
| `clearDepth(depth)` | Equivalent depth clear, typically 1.0 with `Less` depth testing. Needs a depth target. |
| `clear(r, g, b, a, depth)` | Combined color/depth clear with the same ordering constraints. |
| `getAndResetDrawCallCount()` | Returns and resets the local draw counter; compute/ray dispatches are not ordinary draw calls. |
| `~CommandBuffer()` | Releases command recording state. Explicitly wait before tearing down dependent resources. |

### Acceleration-structure commands

The following are compiled only under `METAL` or `VULKAN`:

| Method | Behavior |
| --- | --- |
| `buildPrimitiveAccelerationStructure(blas)` | Records/builds a BLAS from geometry created by its factory. |
| `buildInstanceAccelerationStructure(tlas)` | Records/builds a TLAS referencing built BLAS objects. |
| `buildAccelerationStructures(blases, instances)` | Builds the supplied BLAS objects and creates/builds a TLAS, returning it. |
| `bindInstanceAccelerationStructure(tlas, binding)` | Binds a TLAS at a numeric shader binding; bind the consuming pipeline first. |
| `bindPrimitiveAccelerationStructure(blas, binding)` | Metal only; binds a BLAS at a numeric shader binding. |

## Ray-tracing data

`PrimitiveVertex` contains float arrays: `position[3]`, `normal[3]`, `tangent[3]`, `bitangent[3]`, `uv[2]`. Initialize the fields you supply; position drives acceleration geometry.

`AccelerationStructureInstance` fields have no explicit defaults: `shared_ptr<PrimitiveAccelerationStructure> blas`, `glm::mat4 transform`, `uint32_t instanceId`, `uint32_t mask`, `bool cullDisable`. Set all fields explicitly. An identity transform is `glm::mat4(1.0f)`; a typical visibility mask is `0xff`.

`PrimitiveAccelerationStructure` and `InstanceAccelerationStructure` definitions are present only for Metal/Vulkan builds.

| Member | Behavior |
| --- | --- |
| `PrimitiveAccelerationStructure::create(vector<PrimitiveVertex>, vector<uint32_t>)` | Creates triangle geometry with uint32 indices. |
| `PrimitiveAccelerationStructure::create(vector<float>, vector<uint32_t>)` | Creates geometry from flat xyz positions and uint32 indices. |
| `PrimitiveAccelerationStructure::create(vector<vector<float>>, vector<vector<uint32_t>>)` | Creates multiple geometry groups from corresponding position/index arrays. |
| `PrimitiveAccelerationStructure::isBuilt` | Initially false; backend build state. It is not a CPU/GPU synchronization primitive. |
| `PrimitiveAccelerationStructure::nativeHandle()` | Vulkan only, returns an opaque `uint64_t` acceleration-structure handle. Ownership stays with Opal. |
| `InstanceAccelerationStructure::create(instances)` | Creates instance data referencing BLAS objects. Record its build before tracing. |
| `InstanceAccelerationStructure::isBuilt` | Initially false; same synchronization caveat. |
| `InstanceAccelerationStructure::nativeHandle()` | Vulkan-only opaque `uint64_t` TLAS handle. |
| Both destructors | Release backend acceleration resources; keep referenced geometry alive through GPU completion. |
| `writeMetalTransform3x4(matrix, out3x4)` | Metal-only helper declaration in the public header; its definition is local to `ray_tracing.cpp`. Not usable as a consumer API. Use `AccelerationStructureInstance::transform`. |

For a complete host build sequence and the distinction between Metal compute tracing and Vulkan ray dispatch, see [ray-tracing recipes](recipes.md#ray-tracing).
