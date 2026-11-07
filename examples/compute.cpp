#include <array>
#include <exception>
#include <iostream>
#include <opal/opal.h>
#include <stdexcept>

#ifdef METAL
const char *computeSource = R"(
#include <metal_stdlib>
using namespace metal;
struct Values { float data[256]; };
kernel void computeMain(device Values& values [[buffer(0)]],
                        uint index [[thread_position_in_grid]]) {
    if (index < 256) { values.data[index] = float(index) * 2.0; }
}
)";
#elif defined(VULKAN)
const char *computeSource = R"(
[[vk::binding(0, 0)]] RWStructuredBuffer<float> values;
[shader("compute")]
[numthreads(64, 1, 1)]
void computeMain(uint3 id : SV_DispatchThreadID) {
    if (id.x < 256) { values[id.x] = float(id.x) * 2.0; }
}
)";
#endif

void run() {
    auto context = opal::Context::create();
    context->setHidden(true);
    context->makeWindow(64, 64, "Opal compute example");
    auto device = opal::Device::acquire(context);
    auto shader = opal::Shader::createFromSource(
        computeSource, opal::ShaderType::Compute, "computeMain");
    shader->compile();
    if (!shader->getShaderStatus()) {
        throw std::runtime_error("Compute shader compilation failed");
    }
    auto program = opal::ShaderProgram::create();
    program->attachShader(shader);
    program->link();
    if (!program->getProgramStatus()) {
        throw std::runtime_error("Compute program linking failed");
    }
    auto pipeline = opal::Pipeline::create();
    pipeline->setShaderProgram(program);
    pipeline->setComputeThreadgroupSize(64, 1, 1);
    pipeline->build();
    std::array<float, 256> initial{};
    auto values = opal::Buffer::create(opal::BufferUsage::ShaderReadWrite,
                                       sizeof(initial), initial.data(),
                                       opal::MemoryUsageType::CPUToGPU);
    pipeline->bindShaderReadWriteBuffer("values", values);
    auto commands = device->acquireCommandBuffer();
    commands->start();
    commands->bindPipeline(pipeline);
    commands->dispatch(256);
    commands->computeBarrier();
    commands->commit();
    commands->waitForSubmittedWork();
    std::cout
        << "Compute work completed; the output remains in the GPU buffer.\n";
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
