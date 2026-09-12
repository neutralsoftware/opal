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
#include <string>
#include <utility>
#include <vector>
#ifdef METAL
#include "metal_state.h"
#endif

namespace opal {

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

#if defined(METAL)
int Shader::currentId = 1;
int ShaderProgram::currentId = 1;
#endif

Shader::~Shader() {
#ifdef METAL
    metal::releaseShaderState(this);
#endif
    if (source != nullptr) {
        std::free(source);
        source = nullptr;
    }
}

ShaderProgram::~ShaderProgram() {
#ifdef METAL
    metal::releaseProgramState(this);
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
    shader->source = strdup(source);
    return shader;

#elif defined(METAL)
    auto shader = std::make_shared<Shader>();
    shader->shaderID = 0;
    shader->type = type;
    shader->source = strdup(source);
    shader->functionName = entryPoint;
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
#else
    throw std::runtime_error("Shader attachment not implemented for this API");
#endif
}

void ShaderProgram::link() {
    this->computeProgram = false;
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
#else
    throw std::runtime_error(
        "Shader program log retrieval not implemented for this API");
#endif
}


} // namespace opal
