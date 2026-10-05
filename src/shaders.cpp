/*
 shaders.cpp
 As part of the Atlas project
 Created by Max Van den Eynde in 2025
 --------------------------------------------------
 Description: Shader creation functions
 Copyright (c) 2025 maxvdec
*/

#include "diagnostics.h"
#include "opal/opal.h"
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <glad/glad.h>
#include <memory>
#include <new>
#include <stdexcept>
#include <string.h>
#include <string>
#include <utility>
#include <vector>
#ifdef METAL
#include "metal_state.h"
#endif
#ifdef VULKAN
#include "vulkan_state.h"
#include <vulkan/vulkan_core.h>
#endif

namespace opal {

char *duplicateShaderSource(const char *source) {
    if (source == nullptr) {
        return nullptr;
    }
    const std::size_t length = std::strlen(source) + 1;
    auto *copy = static_cast<char *>(std::malloc(length));
    if (copy == nullptr) {
        throw std::bad_alloc();
    }
    std::memcpy(copy, source, length);
    return copy;
}

#ifdef VULKAN
namespace {
bool decodeHexSpirv(const char *source, std::vector<uint32_t> &words) {
    static constexpr char magic[] = "03022307"; // 0x07230203, little endian
    const std::size_t length = std::strlen(source);
    if (length < 40 || length % 8 != 0 ||
        std::strncmp(source, magic, sizeof(magic) - 1) != 0) {
        return false;
    }
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return -1;
    };
    std::vector<uint32_t> decoded(length / 8);
    for (std::size_t word = 0; word < decoded.size(); ++word) {
        uint32_t value = 0;
        for (std::size_t byte = 0; byte < 4; ++byte) {
            const int high = nibble(source[word * 8 + byte * 2]);
            const int low = nibble(source[word * 8 + byte * 2 + 1]);
            if (high < 0 || low < 0) {
                return false;
            }
            value |= static_cast<uint32_t>((high << 4) | low) << (byte * 8);
        }
        decoded[word] = value;
    }
    words = std::move(decoded);
    return true;
}

std::string spirvEntryPointName(const std::vector<uint32_t> &words,
                                const std::string &preferred) {
    constexpr uint32_t opEntryPoint = 15;
    std::string first;
    for (std::size_t index = 5; index < words.size();) {
        const uint32_t wordCount = words[index] >> 16;
        const uint32_t opcode = words[index] & 0xFFFFu;
        if (wordCount == 0 || index + wordCount > words.size()) {
            break;
        }
        if (opcode == opEntryPoint && wordCount > 3) {
            const char *name =
                reinterpret_cast<const char *>(&words[index + 3]);
            const std::size_t maxLength = (wordCount - 3) * sizeof(uint32_t);
            const std::string entry(name, strnlen(name, maxLength));
            if (entry == preferred) {
                return entry;
            }
            if (first.empty()) {
                first = entry;
            }
        }
        index += wordCount;
    }
    return first.empty() ? preferred : first;
}
} // namespace
#endif

const char *packedShaderSource(const char *const *parts, std::size_t count) {
    if (parts == nullptr || count == 0) {
        return "";
    }

    using OwnedShaderSource = std::unique_ptr<char, decltype(&std::free)>;
    struct CacheEntry {
        const char *const *parts;
        std::size_t count;
        OwnedShaderSource joined;
    };

    static std::vector<CacheEntry> cache;

    for (const auto &entry : cache) {
        if (entry.parts == parts && entry.count == count) {
            return entry.joined.get();
        }
    }

    std::size_t totalLength = 0;
    for (std::size_t i = 0; i < count; ++i) {
        if (parts[i] != nullptr) {
            totalLength += std::strlen(parts[i]);
        }
    }

    char *joined =
        static_cast<char *>(std::malloc((totalLength + 1) * sizeof(char)));
    if (joined == nullptr) {
        throw std::bad_alloc();
    }

    std::size_t offset = 0;
    for (std::size_t i = 0; i < count; ++i) {
        if (parts[i] == nullptr) {
            continue;
        }
        std::size_t partLength = std::strlen(parts[i]);
        std::memcpy(joined + offset, parts[i], partLength);
        offset += partLength;
    }
    joined[offset] = '\0';

    cache.push_back(CacheEntry{
        parts,
        count,
        OwnedShaderSource(joined, &std::free),
    });
    return cache.back().joined.get();
}

#ifdef OPENGL
uint Shader::getGLShaderType(ShaderType type) {
    switch (type) {
    case ShaderType::Vertex:
        return GL_VERTEX_SHADER;
    case ShaderType::Fragment:
        return GL_FRAGMENT_SHADER;
    case ShaderType::Geometry:
        return GL_GEOMETRY_SHADER;
    case ShaderType::TessellationControl:
        return GL_TESS_CONTROL_SHADER;
    case ShaderType::TessellationEvaluation:
        return GL_TESS_EVALUATION_SHADER;
    case ShaderType::Compute:
        return 0; // OpenGL compute shaders are not created with glCreateShader
    default:
        detail::log(LogLevel::Error, "Unknown shader type");
        throw std::runtime_error("Unknown shader type");
    }
}
#endif

#if defined(METAL) || defined(VULKAN)
int Shader::currentId = 1;
int ShaderProgram::currentId = 1;
#endif

Shader::~Shader() {
#ifdef METAL
    metal::releaseShaderState(this);
#elif VULKAN
    vulkan::releaseShaderState(this);
#endif
    if (source != nullptr) {
        std::free(source);
        source = nullptr;
    }
}

ShaderProgram::~ShaderProgram() {
#ifdef METAL
    metal::releaseProgramState(this);
#elif VULKAN
    vulkan::releaseProgramState(this);
#endif
}

std::shared_ptr<Shader>
Shader::createFromSource(const char *source, ShaderType type,
                         const std::string &entryPoint) {
#ifdef OPENGL
    GLenum shaderType = Shader::getGLShaderType(type);

    uint shaderId = glCreateShader(shaderType);
    glShaderSource(shaderId, 1, &source, nullptr);

    auto shader = std::make_shared<Shader>();
    shader->shaderID = shaderId;
    shader->type = type;
    shader->source = duplicateShaderSource(source);
    return shader;

#elif defined(METAL)
    auto shader = std::make_shared<Shader>();
    shader->shaderID = 0;
    shader->type = type;
    shader->source = duplicateShaderSource(source);
    shader->functionName = entryPoint;
    return shader;
#elif defined(VULKAN)
    auto shader = std::make_shared<Shader>();

    shader->shaderID = 0;
    shader->type = type;
    shader->source = duplicateShaderSource(source);
    shader->functionName = entryPoint;

    auto &state = vulkan::shaderState(shader.get());

    state.stage = vulkan::shaderTypeToVk(type);
    state.entryPoint = entryPoint.empty() ? "main" : entryPoint;

    return shader;
#else
    throw std::runtime_error("Shader creation not implemented for this API");
#endif
}

std::shared_ptr<Shader> Shader::forFunction(const std::string &functionName,
                                            ShaderType type) const {
    if (source == nullptr) {
        throw std::runtime_error("Shader source is null");
    }

    auto shader = Shader::createFromSource(source, type);
    shader->functionName = functionName;
    return shader;
}

void Shader::compile() {
#ifdef OPENGL
    glCompileShader(shaderID);
#elif defined(METAL)
    if (Device::globalInstance == nullptr) {
        throw std::runtime_error("Cannot compile Metal shader without device");
    }

    auto &deviceState = metal::deviceState(Device::globalInstance);
    if (deviceState.device == nullptr) {
        throw std::runtime_error("Metal device is not initialized");
    }

    auto &shaderState = metal::shaderState(this);

    NS::Error *error = nullptr;
    auto *compileOptions = MTL::CompileOptions::alloc()->init();
    compileOptions->setLanguageVersion(MTL::LanguageVersion4_0);
    compileOptions->setFastMathEnabled(true);

    NS::String *sourceString =
        NS::String::string(source, NS::UTF8StringEncoding);
    shaderState.library =
        deviceState.device->newLibrary(sourceString, compileOptions, &error);
    compileOptions->release();
    if (shaderState.library == nullptr) {
        std::string message = "Metal shader compilation failed";
        if (error != nullptr && error->localizedDescription() != nullptr) {
            message += ": ";
            message += error->localizedDescription()->utf8String();
        }
        throw std::runtime_error(message);
    }

    if (functionName.empty()) {
        throw std::runtime_error(
            "Metal shader function name must be specified before compilation");
    }
    NS::String *entryName =
        NS::String::string(functionName.c_str(), NS::UTF8StringEncoding);
    shaderState.function = shaderState.library->newFunction(entryName);
    if (shaderState.function == nullptr) {
        NS::Array *functions = shaderState.library->functionNames();
        if (functions != nullptr && functions->count() > 0) {
            NS::String *firstName = functions->object<NS::String>(0);
            shaderState.function = shaderState.library->newFunction(firstName);
        }
    }

    if (shaderState.function == nullptr) {
        throw std::runtime_error("Unable to load function " + functionName +
                                 " from Metal shader library");
    }

    this->shaderID = Shader::currentId++;
#elif VULKAN
    if (Device::globalInstance == nullptr) {
        throw std::runtime_error("Cannot compile Vulkan shader without device");
    }

    auto &deviceState = vulkan::deviceState(Device::globalInstance);

    if (deviceState.device == VK_NULL_HANDLE) {
        throw std::runtime_error("Vulkan device is not initialized");
    }

    if (source == nullptr) {
        throw std::runtime_error("Vulkan shader source is null");
    }

    if (functionName.empty()) {
        throw std::runtime_error(
            "Vulkan shader function name must be specified before compilation");
    }

    auto &state = vulkan::shaderState(this);
    state.stage = vulkan::shaderTypeToVk(type);
    state.entryPoint = functionName;

    state.compiled = false;
    state.log.clear();
    try {
        if (decodeHexSpirv(source, state.spirv)) {
            state.entryPoint = spirvEntryPointName(state.spirv, functionName);
        } else {
            state.spirv =
                vulkan::compileSlangToSPIRV(source, type, functionName);
        }
    } catch (const std::exception &error) {
        state.log = error.what();
        throw;
    }

    if (state.spirv.empty()) {
        throw std::runtime_error(
            "Vulkan shader compilation produced no SPIR-V");
    }

    if (state.shaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(deviceState.device, state.shaderModule, nullptr);
        state.shaderModule = VK_NULL_HANDLE;
    }

    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

    info.codeSize = state.spirv.size() * sizeof(uint32_t);

    info.pCode = state.spirv.data();

    VULKAN_GUARD(vkCreateShaderModule(deviceState.device, &info, nullptr,
                                      &state.shaderModule),
                 "Failed to create Vulkan shader module");

    state.compiled = true;

    shaderID = Shader::currentId++;

#endif
}

bool Shader::getShaderStatus() const {
#ifdef OPENGL
    GLint success;
    glGetShaderiv(shaderID, GL_COMPILE_STATUS, &success);
    return success == GL_TRUE;
#elif defined(METAL)
    auto &shaderState = metal::shaderState(const_cast<Shader *>(this));
    return shaderState.function != nullptr;
#elif defined(VULKAN)
    auto &state = vulkan::shaderState(const_cast<Shader *>(this));
    return state.compiled && state.shaderModule != VK_NULL_HANDLE;
#else
    throw std::runtime_error(
        "Shader status retrieval not implemented for this API");
#endif
}

void Shader::getShaderLog(char *logBuffer, size_t bufferSize) const {
#ifdef OPENGL
    glGetShaderInfoLog(shaderID, static_cast<GLsizei>(bufferSize), nullptr,
                       logBuffer);
#elif defined(METAL)
    strncpy(logBuffer, "Metal shader compile status available via exceptions.",
            bufferSize);
#elif defined(VULKAN)
    if (logBuffer == nullptr || bufferSize == 0) {
        return;
    }
    const auto &state = vulkan::shaderState(const_cast<Shader *>(this));
    std::strncpy(logBuffer, state.log.c_str(), bufferSize - 1);
    logBuffer[bufferSize - 1] = '\0';
#else
    throw std::runtime_error(
        "Shader log retrieval not implemented for this API");
#endif
}

std::shared_ptr<ShaderProgram> ShaderProgram::create() {
#ifdef OPENGL
    uint programId = glCreateProgram();
    auto program = std::make_shared<ShaderProgram>();
    program->programID = programId;
    program->attachedShaders = {};
    return program;
#elif defined(METAL)
    auto program = std::make_shared<ShaderProgram>();
    program->attachedShaders = {};
    return program;
#elif defined(VULKAN)
    auto program = std::make_shared<ShaderProgram>();
    program->attachedShaders = {};
    return program;
#else
    throw std::runtime_error(
        "Shader program creation not implemented for this API");
#endif
}

void ShaderProgram::attachShader(const std::shared_ptr<Shader> &shader,
                                 int callerId) {
    if (shader == nullptr) {
        throw std::runtime_error("Cannot attach null shader");
    }
#ifdef OPENGL
    glAttachShader(programID, shader->shaderID);
    attachedShaders.push_back(shader);

    (void)callerId;
#elif defined(METAL)
    attachedShaders.push_back(shader);
    detail::emit(ResourceEvent{
        std::to_string(callerId), ResourceType::Shader,
        ResourceOperation::Loaded,
        Device::globalInstance
            ? static_cast<unsigned int>(Device::globalInstance->frameCount)
            : 0,
        static_cast<float>(shader->source ? strlen(shader->source) : 0) /
            (1024.0f * 1024.0f)});
#elif defined(VULKAN)
    attachedShaders.push_back(shader);
    detail::emit(ResourceEvent{
        std::to_string(callerId), ResourceType::Shader,
        ResourceOperation::Loaded,
        Device::globalInstance
            ? static_cast<unsigned int>(Device::globalInstance->frameCount)
            : 0,
        static_cast<float>(shader->source ? strlen(shader->source) : 0) /
            (1024.0f * 1024.0f)});
#else
    throw std::runtime_error("Shader attachment not implemented for this API");
#endif
}

void ShaderProgram::link() {
    this->computeProgram = false;
    this->rayTracingProgram = false;
    for (const auto &shader : attachedShaders) {
        if (shader != nullptr && shader->type == ShaderType::Compute) {
            this->computeProgram = true;
            break;
        }
    }

#ifdef OPENGL
    glLinkProgram(programID);
#elif defined(METAL)
    auto &state = metal::programState(this);
    state = {};

    const char *vertexSource = nullptr;
    const char *fragmentSource = nullptr;
    const char *computeSource = nullptr;

    for (const auto &shader : attachedShaders) {
        auto &shaderState = metal::shaderState(shader.get());
        if (shader->type == ShaderType::Vertex) {
            state.vertexFunction = shaderState.function;
            vertexSource = shader->source;
        } else if (shader->type == ShaderType::Fragment) {
            state.fragmentFunction = shaderState.function;
            fragmentSource = shader->source;
        } else if (shader->type == ShaderType::Compute) {
            state.computeFunction = shaderState.function;
            computeSource = shader->source;
        }
    }

    this->computeProgram = state.computeFunction != nullptr;
    if (this->computeProgram) {
        if (state.vertexFunction != nullptr ||
            state.fragmentFunction != nullptr) {
            throw std::runtime_error("Metal compute programs must not include "
                                     "vertex or fragment shaders");
        }
        if (computeSource == nullptr) {
            throw std::runtime_error("Metal compute shader source is missing");
        }
        if (!metal::parseComputeProgramLayouts(computeSource, state)) {
            throw std::runtime_error(
                "Failed to parse Metal compute shader buffer layouts");
        }
        state.fragmentColorOutputs = 0;
    } else {
        if (state.vertexFunction == nullptr ||
            state.fragmentFunction == nullptr) {
            throw std::runtime_error(
                "Metal shader program requires vertex and fragment shaders");
        }

        if (vertexSource == nullptr || fragmentSource == nullptr) {
            throw std::runtime_error("Metal shader program source is missing");
        }

        if (!metal::parseProgramLayouts(vertexSource, fragmentSource, state)) {
            throw std::runtime_error(
                "Failed to parse Metal shader buffer layouts");
        }
        uint32_t colorOutputs = metal::fragmentColorOutputCount(fragmentSource);
        state.fragmentColorOutputs = colorOutputs > 0 ? colorOutputs : 1;
    }

    this->programID = ShaderProgram::currentId++;
#elif defined(VULKAN)
    auto &state = vulkan::programState(this);

    if (Device::globalInstance == nullptr) {
        throw std::runtime_error(
            "Cannot link Vulkan shader program without device");
    }

    auto &deviceState = vulkan::deviceState(Device::globalInstance);

    for (VkDescriptorSetLayout layout : state.descriptorSetLayouts) {
        if (layout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(deviceState.device, layout, nullptr);
        }
    }

    state.descriptorSetLayouts.clear();

    if (state.pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(deviceState.device, state.pipelineLayout,
                                nullptr);
        state.pipelineLayout = VK_NULL_HANDLE;
    }

    state.shaderStages.clear();
    state.bindings.clear();
    state.bindingsByName.clear();
    state.uniformsByName.clear();
    state.uniformBlocks.clear();
    state.pushConstantSize = 0;
    state.pushConstantStages = 0;
    state.linked = false;
    state.log.clear();

    this->computeProgram = false;

    for (const auto &shader : attachedShaders) {
        if (shader != nullptr && shader->type == ShaderType::Compute) {
            this->computeProgram = true;
            break;
        }
    }

    state.computeProgram = this->computeProgram;

    bool hasVertex = false;
    bool hasFragment = false;
    bool hasCompute = false;
    bool hasGeometry = false;
    bool hasTessellationControl = false;
    bool hasTessellationEvaluation = false;
    bool hasRayGeneration = false;
    bool hasRayTracingStage = false;

    for (const auto &shader : attachedShaders) {

        if (shader == nullptr) {
            continue;
        }

        auto &shaderState = vulkan::shaderState(shader.get());

        if (!shaderState.compiled ||
            shaderState.shaderModule == VK_NULL_HANDLE) {

            throw std::runtime_error(
                "All Vulkan shaders must be compiled before linking");
        }

        switch (shader->type) {
        case ShaderType::Vertex:
            if (hasVertex) {
                throw std::runtime_error("Duplicate Vulkan vertex shader");
            }
            hasVertex = true;
            break;

        case ShaderType::Fragment:
            if (hasFragment) {
                throw std::runtime_error("Duplicate Vulkan fragment shader");
            }
            hasFragment = true;
            break;

        case ShaderType::Compute:
            if (hasCompute) {
                throw std::runtime_error("Duplicate Vulkan compute shader");
            }
            hasCompute = true;
            break;

        case ShaderType::Geometry:
            if (hasGeometry) {
                throw std::runtime_error("Duplicate Vulkan geometry shader");
            }
            hasGeometry = true;
            break;

        case ShaderType::TessellationControl:
            if (hasTessellationControl) {
                throw std::runtime_error(
                    "Duplicate Vulkan tessellation control shader");
            }
            hasTessellationControl = true;
            break;

        case ShaderType::TessellationEvaluation:
            if (hasTessellationEvaluation) {
                throw std::runtime_error(
                    "Duplicate Vulkan tessellation evaluation shader");
            }
            hasTessellationEvaluation = true;
            break;

        case ShaderType::RayGeneration:
            if (hasRayGeneration) {
                throw std::runtime_error(
                    "Duplicate Vulkan ray-generation shader");
            }
            hasRayGeneration = true;
            hasRayTracingStage = true;
            break;

        case ShaderType::Miss:
        case ShaderType::ClosestHit:
        case ShaderType::AnyHit:
        case ShaderType::Intersection:
        case ShaderType::Callable:
            hasRayTracingStage = true;
            break;

        default:
            break;
        }
    }

    if (hasRayTracingStage) {
        if (hasCompute || hasVertex || hasFragment || hasGeometry ||
            hasTessellationControl || hasTessellationEvaluation ||
            !hasRayGeneration) {
            throw std::runtime_error(
                "Vulkan ray-tracing programs require one ray-generation "
                "shader and no graphics or compute stages");
        }
        this->rayTracingProgram = true;
    } else if (hasCompute) {
        if (attachedShaders.size() != 1 || !hasCompute) {
            throw std::runtime_error(
                "Vulkan compute program must contain only a compute shader");
        }
    } else {
        if (!hasVertex || !hasFragment) {
            throw std::runtime_error(
                "Vulkan graphics program requires vertex and fragment shaders");
        }
        if (hasTessellationControl != hasTessellationEvaluation) {
            throw std::runtime_error(
                "Vulkan tessellation control and evaluation shaders must be "
                "linked together");
        }
        if (hasGeometry &&
            !deviceState.physicalDeviceInfo.features.features.geometryShader) {
            throw std::runtime_error(
                "Selected Vulkan device does not support geometry shaders");
        }
        if (hasTessellationControl && !deviceState.physicalDeviceInfo.features
                                           .features.tessellationShader) {
            throw std::runtime_error(
                "Selected Vulkan device does not support tessellation shaders");
        }
    }

    for (const auto &shader : attachedShaders) {
        auto &shaderState = vulkan::shaderState(shader.get());

        VkPipelineShaderStageCreateInfo stage{};
        stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage.stage = shaderState.stage;
        stage.module = shaderState.shaderModule;
        stage.pName = shaderState.entryPoint.c_str();
        state.shaderStages.push_back(stage);
    }

    for (const auto &shader : attachedShaders) {
        auto &shaderState = vulkan::shaderState(shader.get());

        auto reflected = vulkan::reflectShaderBindings(shaderState, state);

        for (auto binding : reflected) {
            binding.stages = shaderState.stage;
            auto existing =
                std::find_if(state.bindings.begin(), state.bindings.end(),
                             [&](const vulkan::ShaderBinding &other) {
                                 return other.set == binding.set &&
                                        other.binding == binding.binding;
                             });
            if (existing != state.bindings.end()) {
                if (existing->type != binding.type ||
                    existing->count != binding.count) {
                    throw std::runtime_error("Vulkan descriptor binding type "
                                             "mismatch between shader stages");
                }
                existing->stages |= binding.stages;
                state.bindingsByName[binding.name] = *existing;
            } else {
                state.bindings.push_back(binding);
                state.bindingsByName[binding.name] = binding;
            }
        }
    }

    std::sort(state.bindings.begin(), state.bindings.end(),
              [](const vulkan::ShaderBinding &left,
                 const vulkan::ShaderBinding &right) {
                  if (left.set != right.set) {
                      return left.set < right.set;
                  }
                  return left.binding < right.binding;
              });

    uint32_t maxSet = 0;

    for (const auto &binding : state.bindings) {
        maxSet = std::max(maxSet, binding.set);
    }

    const auto &limits = deviceState.physicalDeviceInfo.properties.limits;
    const size_t setCount = state.bindings.empty() ? 0 : size_t{maxSet} + 1;
    if (setCount > limits.maxBoundDescriptorSets) {
        throw std::runtime_error("Vulkan shader requires more descriptor sets "
                                 "than this GPU supports");
    }
    if (state.pushConstantSize > limits.maxPushConstantsSize) {
        throw std::runtime_error(
            "Vulkan shader push constants exceed this GPU's supported size");
    }
    std::vector<std::vector<VkDescriptorSetLayoutBinding>> bindingsPerSet(
        setCount);

    for (const auto &binding : state.bindings) {
        VkDescriptorSetLayoutBinding vkBinding{};
        vkBinding.binding = binding.binding;
        vkBinding.descriptorType = vulkan::descriptorTypeToVk(binding.type);
        vkBinding.descriptorCount = binding.count;
        vkBinding.stageFlags = binding.stages;
        vkBinding.pImmutableSamplers = nullptr;
        bindingsPerSet[binding.set].push_back(vkBinding);
    }

    state.descriptorSetLayouts.resize(bindingsPerSet.size(), VK_NULL_HANDLE);

    for (size_t set = 0; set < bindingsPerSet.size(); ++set) {
        auto &setBindings = bindingsPerSet[set];

        VkDescriptorSetLayoutCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = static_cast<uint32_t>(setBindings.size());
        info.pBindings = setBindings.empty() ? nullptr : setBindings.data();

        VULKAN_GUARD(
            vkCreateDescriptorSetLayout(deviceState.device, &info, nullptr,
                                        &state.descriptorSetLayouts[set]),
            "Failed to create Vulkan descriptor set layout");
    }

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount =
        static_cast<uint32_t>(state.descriptorSetLayouts.size());
    layoutInfo.pSetLayouts = state.descriptorSetLayouts.empty()
                                 ? nullptr
                                 : state.descriptorSetLayouts.data();
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = state.pushConstantStages;
    pushConstantRange.offset = 0;
    pushConstantRange.size = state.pushConstantSize;
    const bool hasPushConstants = state.pushConstantSize > 0;
    layoutInfo.pushConstantRangeCount = hasPushConstants ? 1 : 0;
    layoutInfo.pPushConstantRanges =
        hasPushConstants ? &pushConstantRange : nullptr;

    VULKAN_GUARD(vkCreatePipelineLayout(deviceState.device, &layoutInfo,
                                        nullptr, &state.pipelineLayout),
                 "Failed to create Vulkan pipeline layout");

    state.linked = true;

    programID = ShaderProgram::currentId++;

#else
    throw std::runtime_error(
        "Shader program linking not implemented for this API");
#endif
}

bool ShaderProgram::getProgramStatus() const {
#ifdef OPENGL
    GLint success;
    glGetProgramiv(programID, GL_LINK_STATUS, &success);
    return success == GL_TRUE;
#elif defined(METAL)
    auto &state = metal::programState(const_cast<ShaderProgram *>(this));
    if (this->computeProgram) {
        return state.computeFunction != nullptr;
    }
    return state.vertexFunction != nullptr && state.fragmentFunction != nullptr;
#elif defined(VULKAN)
    const auto &state = vulkan::programState(const_cast<ShaderProgram *>(this));

    return state.linked && state.pipelineLayout != VK_NULL_HANDLE;
#else
    throw std::runtime_error(
        "Shader program status retrieval not implemented for this API");
#endif
}

void ShaderProgram::getProgramLog(char *logBuffer, size_t bufferSize) const {
#ifdef OPENGL
    glGetProgramInfoLog(programID, static_cast<GLsizei>(bufferSize), nullptr,
                        logBuffer);
#elif defined(METAL)
    strncpy(logBuffer, "Metal program link status available via exceptions.",
            bufferSize);
#elif defined(VULKAN)
    const auto &state = vulkan::programState(const_cast<ShaderProgram *>(this));

    if (bufferSize == 0) {
        return;
    }

    std::strncpy(logBuffer, state.log.c_str(), bufferSize - 1);

    logBuffer[bufferSize - 1] = '\0';
#else
    throw std::runtime_error(
        "Shader program log retrieval not implemented for this API");
#endif
}

} // namespace opal
