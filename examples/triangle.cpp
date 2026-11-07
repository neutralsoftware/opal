#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <opal/opal.h>
#include <stdexcept>

#if defined(METAL)
const char *vertexSource = R"(
#include <metal_stdlib>
using namespace metal;
struct VertexInput { float2 position [[attribute(0)]]; };
vertex float4 vertexMain(VertexInput input [[stage_in]]) {
    return float4(input.position, 0.0, 1.0);
}
)";
const char *fragmentSource = R"(
#include <metal_stdlib>
using namespace metal;
fragment float4 fragmentMain() { return float4(1.0, 0.55, 0.15, 1.0); }
)";
#elif defined(VULKAN)
const char *vertexSource = R"(
struct VertexInput { float2 position : POSITION; };
[shader("vertex")]
float4 vertexMain(VertexInput input) : SV_Position {
    return float4(input.position, 0.0, 1.0);
}
)";
const char *fragmentSource = R"(
[shader("fragment")]
float4 fragmentMain() : SV_Target { return float4(1.0, 0.55, 0.15, 1.0); }
)";
#else
const char *vertexSource = R"(#version 410 core
layout(location = 0) in vec2 position;
void main() { gl_Position = vec4(position, 0.0, 1.0); }
)";
const char *fragmentSource = R"(#version 410 core
layout(location = 0) out vec4 color;
void main() { color = vec4(1.0, 0.55, 0.15, 1.0); }
)";
#endif

std::shared_ptr<opal::Shader> compile(const char *source, opal::ShaderType type,
                                      const char *entry) {
    auto shader = opal::Shader::createFromSource(source, type, entry);
    shader->compile();
    if (!shader->getShaderStatus()) {
        std::array<char, 4096> log{};
        shader->getShaderLog(log.data(), log.size());
        log.back() = '\0';
        throw std::runtime_error(log.data());
    }
    return shader;
}

void run() {
    auto context = opal::Context::create();
    context->makeWindow(960, 540, "Opal indexed triangle");
    auto device = opal::Device::acquire(context);
#ifdef OPENGL
    const char *vertexEntry = "main";
    const char *fragmentEntry = "main";
#else
    const char *vertexEntry = "vertexMain";
    const char *fragmentEntry = "fragmentMain";
#endif
    auto program = opal::ShaderProgram::create();
    program->attachShader(
        compile(vertexSource, opal::ShaderType::Vertex, vertexEntry));
    program->attachShader(
        compile(fragmentSource, opal::ShaderType::Fragment, fragmentEntry));
    program->link();
    if (!program->getProgramStatus()) {
        std::array<char, 4096> log{};
        program->getProgramLog(log.data(), log.size());
        log.back() = '\0';
        throw std::runtime_error(log.data());
    }
    struct Vertex {
        float position[2];
    };
    const std::array<Vertex, 3> vertices{
        {{{-0.6f, -0.6f}}, {{0.6f, -0.6f}}, {{0.0f, 0.6f}}}};
    const std::array<uint32_t, 3> indices{0, 1, 2};
    auto vertexBuffer = opal::Buffer::create(opal::BufferUsage::VertexBuffer,
                                             sizeof(vertices), vertices.data());
    auto indexBuffer = opal::Buffer::create(opal::BufferUsage::IndexArray,
                                            sizeof(indices), indices.data());
    opal::VertexAttribute position{};
    position.name = "position";
    position.type = opal::VertexAttributeType::Float;
    position.offset = offsetof(Vertex, position);
    position.location = 0;
    position.size = 2;
    position.stride = sizeof(Vertex);
    auto drawing = opal::DrawingState::create(vertexBuffer, indexBuffer);
    drawing->configureAttributes({{position, vertexBuffer}});
    auto pipeline = opal::Pipeline::create();
    pipeline->setShaderProgram(program);
    pipeline->setVertexAttributes(
        {position}, {sizeof(Vertex), opal::VertexBindingInputRate::Vertex});
    pipeline->setCullMode(opal::CullMode::None);
    pipeline->enableDepthWrite(false);
    pipeline->build();
    auto commands = device->acquireCommandBuffer();
    auto pass = opal::RenderPass::create();
    pass->setFramebuffer(device->getDefaultFramebuffer());
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT ||
                event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                running = false;
            }
        }
        if (!running) {
            break;
        }
        if (SDL_GetWindowFlags(context->getWindow()) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(16);
            continue;
        }
        int width = 0;
        int height = 0;
        if (!SDL_GetWindowSizeInPixels(context->getWindow(), &width, &height) ||
            width <= 0 || height <= 0) {
            SDL_Delay(16);
            continue;
        }
        pipeline->setViewport(0, 0, width, height);
        commands->start();
#ifdef METAL
        commands->clearColor(0.04f, 0.06f, 0.1f, 1.0f);
#endif
        commands->beginPass(pass);
#ifndef METAL
        commands->clearColor(0.04f, 0.06f, 0.1f, 1.0f);
#endif
        commands->bindPipeline(pipeline);
        drawing->bind();
        commands->bindDrawingState(drawing);
        commands->drawIndexed(3);
        commands->endPass();
        commands->commit();
#ifdef OPENGL
        SDL_GL_SwapWindow(context->getWindow());
#endif
        commands->waitForSubmittedWork();
    }
}

int main() {
    int result = 0;
    try {
        run();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    SDL_Quit();
    return result;
}
