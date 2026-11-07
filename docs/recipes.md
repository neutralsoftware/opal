# Rendering recipes

These fragments build on the [triangle example](../examples/triangle.cpp). Unless stated otherwise, they assume a live `context`, `device`, built `pipeline`, and command buffer named `commands`. They illustrate host-side setup; shader declarations must match the selected backend. Keep resources alive through GPU completion.

## Upload and sample a texture

A small RGBA8 texture avoids ambiguous float/packed upload formats:

```cpp
const std::array<unsigned char, 16> pixels{
    255, 0, 0, 255, 0, 255, 0, 255,
    0, 0, 255, 255, 255, 255, 255, 255
};
auto texture = opal::Texture::create(
    opal::TextureType::Texture2D, opal::TextureFormat::Rgba8,
    2, 2, opal::TextureDataFormat::Rgba, pixels.data());
texture->setParameters(opal::TextureWrapMode::ClampToEdge,
                       opal::TextureWrapMode::ClampToEdge,
                       opal::TextureFilterMode::Nearest,
                       opal::TextureFilterMode::Nearest);
commands->bindPipeline(pipeline);
pipeline->bindTexture("albedo", texture, 0);
```

Include `<array>`. The shader must declare and sample `albedo` with the matching coordinate input and sampler. On OpenGL the integer is a texture unit. Vulkan matches the reflected resource name; Metal matches the annotated texture parameter when available. Bind before the consuming draw. For color art use `sRgba8` if the rendering workflow expects sRGB decoding; for masks/data use a linear format.

To read this particular RGBA8 2 × 2 texture after submitted work completes:

```cpp
commands->waitForSubmittedWork();
std::array<unsigned char, 16> result{};
texture->readData(result.data(), opal::TextureDataFormat::Rgba);
```

Do not generalize the 4-byte pixel allocation to floating-point, depth/stencil, multisample, or compressed/native formats. See [backend readback limits](backends.md#known-boundaries).

## Render to an offscreen texture

```cpp
const int width = 512;
const int height = 512;
auto color = opal::Texture::create(
    opal::TextureType::Texture2D, opal::TextureFormat::Rgba8,
    width, height);
auto depth = opal::Texture::create(
    opal::TextureType::Texture2D, opal::TextureFormat::Depth32F,
    width, height, opal::TextureDataFormat::DepthComponent);
auto target = opal::Framebuffer::create(width, height);
target->addAttachment({opal::Attachment::Type::Color, color});
target->addAttachment({opal::Attachment::Type::Depth, depth});
target->setDrawBuffers(1);
if (!target->getStatus()) {
    throw std::runtime_error("Offscreen framebuffer is incomplete");
}
auto pass = opal::RenderPass::create();
pass->setFramebuffer(target);
pipeline->setViewport(0, 0, width, height);
pipeline->enableDepthTest(true);
pipeline->enableDepthWrite(true);
pipeline->build();
```

Include `<stdexcept>`. Render with the same `start()`/clear/`beginPass()`/draw/`endPass()` ordering as the examples, then commit. On a later pass bind `color` as a texture on a pipeline whose fragment shader samples it. Finish the producing pass before sampling; never read and write the same attachment simultaneously. Recreate the attachments and framebuffer when the target size changes.

For a shadow map, omit the color attachment, call `disableColorBuffer()`, and use a compatible depth-only rendering program/path. For point shadows use `createDepthCubemap()` and `attachCubemapFace()` for each face (0–5). Layered cube rendering requires backend/shader support.

## Resolve multisampling

```cpp
auto multisampled = opal::Texture::createMultisampled(
    opal::TextureFormat::Rgba8, 512, 512, 4);
auto resolved = opal::Texture::create(
    opal::TextureType::Texture2D, opal::TextureFormat::Rgba8, 512, 512);
auto source = opal::Framebuffer::create(512, 512);
source->addAttachment({opal::Attachment::Type::Color, multisampled});
auto destination = opal::Framebuffer::create(512, 512);
destination->addAttachment({opal::Attachment::Type::Color, resolved});
auto resolve = opal::ResolveAction::createForColorAttachment(
    source, destination, 0);
```

Render into `source` with a multisampling-enabled pipeline. Use matching sample counts for every source attachment. After `endPass()`, call `commands->performResolve(resolve)` before commit. Sample `resolved` in subsequent work. `createForDepth()` describes a depth-only resolve; depth format/sample combinations and actual resolve support vary by backend. `Context::setSamples()` alone configures only the OpenGL window setup, not these offscreen textures.

## Uniform data and blending

For a shader with named `tint` and `model` uniforms:

```cpp
commands->bindPipeline(pipeline);
pipeline->setUniform4f("tint", 1.0f, 0.6f, 0.2f, 1.0f);
pipeline->setUniformMat4f("model", glm::mat4(1.0f));
```

The names must exist in the GLSL uniforms, reflected Vulkan layout, or parsed Metal layout. To bind structured data, use `bindBufferData()` or a typed `Buffer` on Metal/Vulkan, with host padding/alignment matching the shader. The vector overload simply copies raw bytes; it does not convert a C++ struct into a shader layout. Raw/vector uniform buffer binding does no work on OpenGL.

For straight-alpha output, configure before building:

```cpp
pipeline->enableBlending(true);
pipeline->setBlendFunc(opal::BlendFunc::SrcAlpha,
                       opal::BlendFunc::OneMinusSrcAlpha);
pipeline->setBlendEquation(opal::BlendEquation::Add);
pipeline->build();
```

Choose factors matching your shader output (premultiplied alpha uses a different source factor), and order transparent geometry appropriately.

## Compute

The complete [compute example](../examples/compute.cpp) creates a separate compute-only program and writes 256 floats. `dispatch(256)` means 256 logical threads. With a 64 × 1 × 1 group size it produces four groups. The Vulkan shader's `[numthreads(64, 1, 1)]` must match the host configuration. Guard out-of-range threads when counts are not exact multiples.

Dispatch outside a render pass. Call `computeBarrier()` before dependent compute memory access and `waitForSubmittedWork()` after commit before CPU-side reuse/destruction. The public `Buffer` API currently has no method to map/read the computed values back to the CPU. Use the output in later GPU work, or implement a separately reviewed native interop/readback path.

## Ray tracing

Check `device->supportsRayTracing()` first. Acceleration-structure factories and commands are only defined in Metal/Vulkan builds. This host fragment builds a single triangle and one instance; it assumes a live device and command buffer and requires `<cstdint>` and `<vector>`:

```cpp
#if defined(METAL) || defined(VULKAN)
if (device->supportsRayTracing()) {
    const std::vector<float> positions{
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         0.0f,  1.0f, 0.0f
    };
    const std::vector<uint32_t> indices{0, 1, 2};
    auto blas = opal::PrimitiveAccelerationStructure::create(positions, indices);
    opal::AccelerationStructureInstance instance{};
    instance.blas = blas;
    instance.transform = glm::mat4(1.0f);
    instance.instanceId = 0;
    instance.mask = 0xff;
    instance.cullDisable = true;
    commands->start();
    auto tlas = commands->buildAccelerationStructures({blas}, {instance});
    commands->commit();
    commands->waitForSubmittedWork();
}
#endif
```

This demonstrates building only. To trace, retain `blas` and `tlas` outside that scope, create a matching shader program/pipeline and output resources, start new recording, bind the pipeline, bind the TLAS with `bindInstanceAccelerationStructure(tlas, binding)`, and dispatch:

- Metal: use an MSL compute kernel with ray-query/intersection code, then `dispatch(width, height)`. Metal's hardware capability query does not enable `dispatchRays()`.
- Vulkan: use a ray-tracing program containing ray-generation and any required miss/hit stages, then `dispatchRays(width, height)`. Bind output images and acceleration structures at the shader's reflected/numeric bindings.

End rendering before tracing. Add the necessary dependency barriers before consuming written output, commit, and wait before releasing structures. A visible ray-traced image additionally needs camera/ray generation, material and hit logic, output image binding, and a presentation pass; this build fragment does not supply those shaders.

## Diagnostics

```cpp
void logMessage(opal::LogLevel level, const char *message) {
    std::cerr << static_cast<int>(level) << ": " << message << '\n';
}

void drawEvent(const opal::DrawEvent &event) {
    std::cout << event.callerObject << " frame " << event.frameNumber << '\n';
}
```

Include `<iostream>`. Register with `setLogCallback(logMessage)` and `setDrawCallback(drawEvent)`. Pass `objectId` to draws or `callerId` to resource operations to identify events. Reset with `nullptr` before unloading callback code. Resource/draw events are useful instrumentation but do not provide a complete allocation or timing profiler.
