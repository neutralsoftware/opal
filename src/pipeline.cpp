//
// pipeline.cpp
// As part of the Atlas project
// Created by Max Van den Eynde in 2025
// --------------------------------------------------
// Description: Pipeline functions for the core renderer
// Copyright (c) 2025 maxvdec
//

#include "diagnostics.h"
#include "opal/opal.h"
#include <algorithm>
#include <cstring>
#include <glad/glad.h>
#include <memory>
#include <stdexcept>
#include <unordered_set>
#include <utility>
#include <vector>
#ifdef METAL
#include "metal_state.h"
#endif

namespace opal {


#ifdef METAL
namespace {

constexpr NS::UInteger kVertexStreamBufferIndex = 24;
constexpr NS::UInteger kInstanceStreamBufferIndex = 25;

template <typename T> inline T alignUp(T value, T alignment) {
    if (alignment <= 1) {
        return value;
    }
    return (value + alignment - 1) / alignment * alignment;
}

MTL::BlendFactor toMetalBlendFactor(BlendFunc factor) {
    switch (factor) {
    case BlendFunc::Zero:
        return MTL::BlendFactorZero;
    case BlendFunc::One:
        return MTL::BlendFactorOne;
    case BlendFunc::SrcColor:
        return MTL::BlendFactorSourceColor;
    case BlendFunc::OneMinusSrcColor:
        return MTL::BlendFactorOneMinusSourceColor;
    case BlendFunc::DstColor:
        return MTL::BlendFactorDestinationColor;
    case BlendFunc::OneMinusDstColor:
        return MTL::BlendFactorOneMinusDestinationColor;
    case BlendFunc::SrcAlpha:
        return MTL::BlendFactorSourceAlpha;
    case BlendFunc::OneMinusSrcAlpha:
        return MTL::BlendFactorOneMinusSourceAlpha;
    case BlendFunc::DstAlpha:
        return MTL::BlendFactorDestinationAlpha;
    case BlendFunc::OneMinusDstAlpha:
        return MTL::BlendFactorOneMinusDestinationAlpha;
    default:
        return MTL::BlendFactorOne;
    }
}

MTL::BlendOperation toMetalBlendOperation(BlendEquation equation) {
    switch (equation) {
    case BlendEquation::Add:
        return MTL::BlendOperationAdd;
    case BlendEquation::Subtract:
        return MTL::BlendOperationSubtract;
    case BlendEquation::ReverseSubtract:
        return MTL::BlendOperationReverseSubtract;
    case BlendEquation::Min:
        return MTL::BlendOperationMin;
    case BlendEquation::Max:
        return MTL::BlendOperationMax;
    default:
        return MTL::BlendOperationAdd;
    }
}

MTL::CompareFunction toMetalCompare(CompareOp op) {
    switch (op) {
    case CompareOp::Never:
        return MTL::CompareFunctionNever;
    case CompareOp::Less:
        return MTL::CompareFunctionLess;
    case CompareOp::Equal:
        return MTL::CompareFunctionEqual;
    case CompareOp::LessEqual:
        return MTL::CompareFunctionLessEqual;
    case CompareOp::Greater:
        return MTL::CompareFunctionGreater;
    case CompareOp::NotEqual:
        return MTL::CompareFunctionNotEqual;
    case CompareOp::GreaterEqual:
        return MTL::CompareFunctionGreaterEqual;
    case CompareOp::Always:
        return MTL::CompareFunctionAlways;
    default:
        return MTL::CompareFunctionLess;
    }
}

MTL::PrimitiveType toMetalPrimitive(PrimitiveStyle style) {
    switch (style) {
    case PrimitiveStyle::Points:
        return MTL::PrimitiveTypePoint;
    case PrimitiveStyle::Lines:
        return MTL::PrimitiveTypeLine;
    case PrimitiveStyle::LineStrip:
        return MTL::PrimitiveTypeLineStrip;
    case PrimitiveStyle::Triangles:
        return MTL::PrimitiveTypeTriangle;
    case PrimitiveStyle::TriangleStrip:
        return MTL::PrimitiveTypeTriangleStrip;
    case PrimitiveStyle::TriangleFan:
        return MTL::PrimitiveTypeTriangle;
    case PrimitiveStyle::Patches:
        return MTL::PrimitiveTypeTriangle;
    default:
        return MTL::PrimitiveTypeTriangle;
    }
}

MTL::CullMode toMetalCull(CullMode mode) {
    switch (mode) {
    case CullMode::None:
        return MTL::CullModeNone;
    case CullMode::Front:
        return MTL::CullModeFront;
    case CullMode::Back:
        return MTL::CullModeBack;
    case CullMode::FrontAndBack:
        return MTL::CullModeFront;
    default:
        return MTL::CullModeNone;
    }
}

MTL::Winding toMetalWinding(FrontFace face) {
    switch (face) {
    case FrontFace::Clockwise:
        return MTL::WindingClockwise;
    case FrontFace::CounterClockwise:
        return MTL::WindingCounterClockwise;
    default:
        return MTL::WindingCounterClockwise;
    }
}

MTL::TriangleFillMode toMetalFillMode(RasterizerMode mode) {
    switch (mode) {
    case RasterizerMode::Fill:
        return MTL::TriangleFillModeFill;
    case RasterizerMode::Line:
        return MTL::TriangleFillModeLines;
    case RasterizerMode::Point:
        return MTL::TriangleFillModeFill;
    default:
        return MTL::TriangleFillModeFill;
    }
}

void ensureMetalDepthStencilState(metal::PipelineState &state,
                                  MTL::Device *device, bool depthTestEnabled,
                                  bool depthWriteEnabled,
                                  MTL::CompareFunction depthCompare) {
    if (device == nullptr) {
        return;
    }

    bool needsRebuild = state.depthStencilState == nullptr ||
                        state.depthTestEnabled != depthTestEnabled ||
                        state.depthWriteEnabled != depthWriteEnabled ||
                        state.depthCompare != depthCompare;
    if (!needsRebuild) {
        return;
    }

    MTL::DepthStencilDescriptor *depthDescriptor =
        MTL::DepthStencilDescriptor::alloc()->init();
    depthDescriptor->setDepthCompareFunction(depthCompare);
    depthDescriptor->setDepthWriteEnabled(depthWriteEnabled);

    MTL::DepthStencilState *newState =
        device->newDepthStencilState(depthDescriptor);
    depthDescriptor->release();
    if (newState == nullptr) {
        throw std::runtime_error("Failed to create Metal depth stencil state");
    }

    if (state.depthStencilState != nullptr) {
        state.depthStencilState->release();
    }
    state.depthStencilState = newState;
}

MTL::VertexFormat toMetalVertexFormat(VertexAttributeType type, uint size,
                                      bool normalized) {
    switch (type) {
    case VertexAttributeType::Float:
        if (size == 1)
            return MTL::VertexFormatFloat;
        if (size == 2)
            return MTL::VertexFormatFloat2;
        if (size == 3)
            return MTL::VertexFormatFloat3;
        if (size == 4)
            return MTL::VertexFormatFloat4;
        return MTL::VertexFormatFloat4;
    case VertexAttributeType::Int:
        if (size == 1)
            return MTL::VertexFormatInt;
        if (size == 2)
            return MTL::VertexFormatInt2;
        if (size == 3)
            return MTL::VertexFormatInt3;
        if (size == 4)
            return MTL::VertexFormatInt4;
        return MTL::VertexFormatInt4;
    case VertexAttributeType::UnsignedInt:
        if (size == 1)
            return MTL::VertexFormatUInt;
        if (size == 2)
            return MTL::VertexFormatUInt2;
        if (size == 3)
            return MTL::VertexFormatUInt3;
        if (size == 4)
            return MTL::VertexFormatUInt4;
        return MTL::VertexFormatUInt4;
    case VertexAttributeType::Short:
        if (size == 2)
            return normalized ? MTL::VertexFormatShort2Normalized
                              : MTL::VertexFormatShort2;
        if (size == 3)
            return normalized ? MTL::VertexFormatShort3Normalized
                              : MTL::VertexFormatShort3;
        if (size == 4)
            return normalized ? MTL::VertexFormatShort4Normalized
                              : MTL::VertexFormatShort4;
        return normalized ? MTL::VertexFormatShort2Normalized
                          : MTL::VertexFormatShort2;
    case VertexAttributeType::UnsignedShort:
        if (size == 2)
            return normalized ? MTL::VertexFormatUShort2Normalized
                              : MTL::VertexFormatUShort2;
        if (size == 3)
            return normalized ? MTL::VertexFormatUShort3Normalized
                              : MTL::VertexFormatUShort3;
        if (size == 4)
            return normalized ? MTL::VertexFormatUShort4Normalized
                              : MTL::VertexFormatUShort4;
        return normalized ? MTL::VertexFormatUShort2Normalized
                          : MTL::VertexFormatUShort2;
    case VertexAttributeType::Byte:
        if (size == 2)
            return normalized ? MTL::VertexFormatChar2Normalized
                              : MTL::VertexFormatChar2;
        if (size == 3)
            return normalized ? MTL::VertexFormatChar3Normalized
                              : MTL::VertexFormatChar3;
        if (size == 4)
            return normalized ? MTL::VertexFormatChar4Normalized
                              : MTL::VertexFormatChar4;
        return normalized ? MTL::VertexFormatCharNormalized
                          : MTL::VertexFormatChar;
    case VertexAttributeType::UnsignedByte:
        if (size == 2)
            return normalized ? MTL::VertexFormatUChar2Normalized
                              : MTL::VertexFormatUChar2;
        if (size == 3)
            return normalized ? MTL::VertexFormatUChar3Normalized
                              : MTL::VertexFormatUChar3;
        if (size == 4)
            return normalized ? MTL::VertexFormatUChar4Normalized
                              : MTL::VertexFormatUChar4;
        return normalized ? MTL::VertexFormatUCharNormalized
                          : MTL::VertexFormatUChar;
    case VertexAttributeType::Double:
        return MTL::VertexFormatFloat4;
    default:
        return MTL::VertexFormatFloat4;
    }
}

void updateMetalUniform(Pipeline *pipeline, const std::string &name,
                        const void *data, size_t size,
                        bool clampToDeclaredSize) {
    if (pipeline == nullptr || pipeline->shaderProgram == nullptr ||
        data == nullptr || size == 0) {
        return;
    }

    auto &programState = metal::programState(pipeline->shaderProgram.get());
    auto locations = metal::resolveUniformLocations(programState, name);
    if (locations.empty()) {
        return;
    }

    auto &pipelineState = metal::pipelineState(pipeline);
    for (const auto &location : locations) {
        auto writeStage = [&](metal::MetalProgramStage stage) {
            uint32_t key = metal::stageBindingKey(location.bufferIndex, stage);
            auto &bytes = pipelineState.uniformData[key];

            size_t declaredSize = 0;
            auto bindingIt = programState.bindingSize.find(key);
            if (bindingIt != programState.bindingSize.end()) {
                declaredSize = bindingIt->second;
            }

            size_t writeSize =
                clampToDeclaredSize ? std::min(size, location.size) : size;
            if (writeSize == 0) {
                return;
            }
            size_t requiredSize = location.offset + writeSize;
            requiredSize = std::max(requiredSize, declaredSize);
            if (bytes.size() < requiredSize) {
                bytes.resize(alignUp(requiredSize, static_cast<size_t>(16)), 0);
            }
            std::memcpy(bytes.data() + location.offset, data, writeSize);
        };

        if (location.vertexStage) {
            writeStage(metal::MetalProgramStage::Vertex);
        }
        if (location.fragmentStage) {
            writeStage(metal::MetalProgramStage::Fragment);
        }
        if (location.computeStage) {
            writeStage(metal::MetalProgramStage::Compute);
        }
    }
}

} // namespace
#endif

std::shared_ptr<Pipeline> Pipeline::create() {
    auto pipeline = std::make_shared<Pipeline>();
    return pipeline;
}

Pipeline::~Pipeline() {
#ifdef METAL
    metal::releasePipelineState(this);
#endif
}


void Pipeline::setShaderProgram(std::shared_ptr<ShaderProgram> program) {
    this->shaderProgram = std::move(program);
}

void Pipeline::setVertexAttributes(
    const std::vector<VertexAttribute> &attributes,
    const VertexBinding &binding) {
    this->vertexAttributes = attributes;
    this->vertexBinding = binding;
}

void Pipeline::setPrimitiveStyle(PrimitiveStyle style) {
    this->primitiveStyle = style;
}

void Pipeline::setPatchVertices(int count) { this->patchVertices = count; }

void Pipeline::setViewport(int x, int y, int width, int height) {
    this->viewportX = x;
    this->viewportY = y;
    this->viewportWidth = width;
    this->viewportHeight = height;
}

void Pipeline::setRasterizerMode(RasterizerMode mode) {
    this->rasterizerMode = mode;
}

void Pipeline::setCullMode(CullMode mode) { this->cullMode = mode; }

void Pipeline::setFrontFace(FrontFace face) { this->frontFace = face; }

void Pipeline::setComputeThreadgroupSize(uint x, uint y, uint z) {
    this->computeThreadgroupX = std::max<uint>(1, x);
    this->computeThreadgroupY = std::max<uint>(1, y);
    this->computeThreadgroupZ = std::max<uint>(1, z);
}

void Pipeline::enableDepthTest(bool enabled) {
    this->depthTestEnabled = enabled;
}

void Pipeline::setDepthCompareOp(CompareOp op) { this->depthCompareOp = op; }

void Pipeline::enableDepthWrite(bool enabled) {
    this->depthWriteEnabled = enabled;
}

void Pipeline::enableBlending(bool enabled) { this->blendingEnabled = enabled; }

void Pipeline::setBlendFunc(BlendFunc srcFactor, BlendFunc dstFactor) {
    this->blendSrcFactor = srcFactor;
    this->blendDstFactor = dstFactor;
}

void Pipeline::setBlendEquation(BlendEquation equation) {
    this->blendEquation = equation;
}

void Pipeline::enableMultisampling(bool enabled) {
    this->multisamplingEnabled = enabled;
}

void Pipeline::enablePolygonOffset(bool enabled) {
    this->polygonOffsetEnabled = enabled;
}

void Pipeline::setPolygonOffset(float factor, float units) {
    this->polygonOffsetFactor = factor;
    this->polygonOffsetUnits = units;
}

void Pipeline::enableClipDistance(int index, bool enabled) {
    if (enabled) {
        if (std::find(this->enabledClipDistances.begin(),
                      this->enabledClipDistances.end(),
                      index) == this->enabledClipDistances.end()) {
            this->enabledClipDistances.push_back(index);
        }
    } else {
        auto it = std::find(this->enabledClipDistances.begin(),
                            this->enabledClipDistances.end(), index);
        if (it != this->enabledClipDistances.end()) {
            this->enabledClipDistances.erase(it);
        }
    }
}

uint Pipeline::getGLBlendFactor(BlendFunc factor) const {
    switch (factor) {
    case BlendFunc::Zero:
        return GL_ZERO;
    case BlendFunc::One:
        return GL_ONE;
    case BlendFunc::SrcColor:
        return GL_SRC_COLOR;
    case BlendFunc::OneMinusSrcColor:
        return GL_ONE_MINUS_SRC_COLOR;
    case BlendFunc::DstColor:
        return GL_DST_COLOR;
    case BlendFunc::OneMinusDstColor:
        return GL_ONE_MINUS_DST_COLOR;
    case BlendFunc::SrcAlpha:
        return GL_SRC_ALPHA;
    case BlendFunc::OneMinusSrcAlpha:
        return GL_ONE_MINUS_SRC_ALPHA;
    case BlendFunc::DstAlpha:
        return GL_DST_ALPHA;
    case BlendFunc::OneMinusDstAlpha:
        return GL_ONE_MINUS_DST_ALPHA;
    default:
        return GL_ONE;
    }
}

uint Pipeline::getGLBlendEquation(BlendEquation equation) const {
    switch (equation) {
    case BlendEquation::Add:
        return GL_FUNC_ADD;
    case BlendEquation::Subtract:
        return GL_FUNC_SUBTRACT;
    case BlendEquation::ReverseSubtract:
        return GL_FUNC_REVERSE_SUBTRACT;
    case BlendEquation::Min:
        return GL_MIN;
    case BlendEquation::Max:
        return GL_MAX;
    default:
        return GL_FUNC_ADD;
    }
}

uint Pipeline::getGLCompareOp(CompareOp op) const {
    switch (op) {
    case CompareOp::Never:
        return GL_NEVER;
    case CompareOp::Less:
        return GL_LESS;
    case CompareOp::Equal:
        return GL_EQUAL;
    case CompareOp::LessEqual:
        return GL_LEQUAL;
    case CompareOp::Greater:
        return GL_GREATER;
    case CompareOp::NotEqual:
        return GL_NOTEQUAL;
    case CompareOp::GreaterEqual:
        return GL_GEQUAL;
    case CompareOp::Always:
        return GL_ALWAYS;
    default:
        return GL_LESS;
    }
}

uint Pipeline::getGLPrimitiveStyle(PrimitiveStyle style) const {
    switch (style) {
    case PrimitiveStyle::Points:
        return GL_POINTS;
    case PrimitiveStyle::Lines:
        return GL_LINES;
    case PrimitiveStyle::LineStrip:
        return GL_LINE_STRIP;
    case PrimitiveStyle::Triangles:
        return GL_TRIANGLES;
    case PrimitiveStyle::TriangleStrip:
        return GL_TRIANGLE_STRIP;
    case PrimitiveStyle::TriangleFan:
        return GL_TRIANGLE_FAN;
    case PrimitiveStyle::Patches:
        return GL_PATCHES;
    default:
        return GL_TRIANGLES;
    }
}

uint Pipeline::getGLRasterizerMode(RasterizerMode mode) const {
    switch (mode) {
    case RasterizerMode::Fill:
        return GL_FILL;
    case RasterizerMode::Line:
        return GL_LINE;
    case RasterizerMode::Point:
        return GL_POINT;
    default:
        return GL_FILL;
    }
}

uint Pipeline::getGLCullMode(CullMode mode) const {
    switch (mode) {
    case CullMode::None:
        return 0;
    case CullMode::Front:
        return GL_FRONT;
    case CullMode::Back:
        return GL_BACK;
    case CullMode::FrontAndBack:
        return GL_FRONT_AND_BACK;
    default:
        return GL_BACK;
    }
}

uint Pipeline::getGLFrontFace(FrontFace face) const {
    switch (face) {
    case FrontFace::Clockwise:
        return GL_CW;
    case FrontFace::CounterClockwise:
        return GL_CCW;
    default:
        return GL_CCW;
    }
}

uint Pipeline::getGLVertexAttributeType(VertexAttributeType type) const {
    switch (type) {
    case VertexAttributeType::Float:
        return GL_FLOAT;
    case VertexAttributeType::Int:
        return GL_INT;
    case VertexAttributeType::UnsignedInt:
        return GL_UNSIGNED_INT;
    case VertexAttributeType::Short:
        return GL_SHORT;
    case VertexAttributeType::UnsignedShort:
        return GL_UNSIGNED_SHORT;
    case VertexAttributeType::Byte:
        return GL_BYTE;
    case VertexAttributeType::UnsignedByte:
        return GL_UNSIGNED_BYTE;
    default:
        return GL_FLOAT;
    }
}

void Pipeline::build() {
#ifdef OPENGL
    (void)this; // Vertex layout applied explicitly per VAO.
#elif defined(METAL)
    if (Device::globalInstance == nullptr) {
        throw std::runtime_error("Cannot build Metal pipeline without device");
    }
    auto &deviceState = metal::deviceState(Device::globalInstance);
    if (deviceState.device == nullptr) {
        throw std::runtime_error("Metal device is not initialized");
    }

    if (shaderProgram == nullptr) {
        throw std::runtime_error("Pipeline::build() requires a shader program");
    }

    auto &state = metal::pipelineState(this);

    state.primitiveType = toMetalPrimitive(this->primitiveStyle);
    state.cullMode = toMetalCull(this->cullMode);
    state.frontFace = toMetalWinding(this->frontFace);
    state.fillMode = toMetalFillMode(this->rasterizerMode);
    MTL::CompareFunction desiredDepthCompare =
        this->depthTestEnabled ? toMetalCompare(this->depthCompareOp)
                               : MTL::CompareFunctionAlways;
    state.blendingEnabled = this->blendingEnabled;
    state.blendSrc = toMetalBlendFactor(this->blendSrcFactor);
    state.blendDst = toMetalBlendFactor(this->blendDstFactor);
    state.blendOp = toMetalBlendOperation(this->blendEquation);
    state.polygonOffsetEnabled = this->polygonOffsetEnabled;
    state.polygonOffsetFactor = this->polygonOffsetFactor;
    state.polygonOffsetUnits = this->polygonOffsetUnits;
    state.viewportX = this->viewportX;
    state.viewportY = this->viewportY;
    state.viewportWidth = this->viewportWidth;
    state.viewportHeight = this->viewportHeight;
    if (shaderProgram->isComputeProgram()) {
        return;
    }

    if (state.vertexDescriptor != nullptr) {
        state.vertexDescriptor->release();
    }
    state.vertexDescriptor = MTL::VertexDescriptor::alloc()->init();

    auto *layouts = state.vertexDescriptor->layouts();
    auto *attributes = state.vertexDescriptor->attributes();

    NS::UInteger instanceStride = 0;
    NS::UInteger instanceDivisor = 1;
    bool hasInstance = false;

    for (const auto &attribute : this->vertexAttributes) {
        auto *descriptor = attributes->object(attribute.location);
        if (descriptor == nullptr) {
            continue;
        }

        bool isInstance =
            attribute.inputRate == VertexBindingInputRate::Instance;
        NS::UInteger bufferIndex =
            isInstance ? kInstanceStreamBufferIndex : kVertexStreamBufferIndex;

        descriptor->setBufferIndex(bufferIndex);
        descriptor->setOffset(static_cast<NS::UInteger>(attribute.offset));
        descriptor->setFormat(toMetalVertexFormat(
            attribute.type, attribute.size, attribute.normalized));

        if (isInstance) {
            hasInstance = true;
            instanceStride = std::max(
                instanceStride, static_cast<NS::UInteger>(attribute.stride));
            unsigned int divisor =
                attribute.divisor == 0 ? 1 : attribute.divisor;
            instanceDivisor =
                std::max(instanceDivisor, static_cast<NS::UInteger>(divisor));
        }
    }

    auto *vertexLayout = layouts->object(kVertexStreamBufferIndex);
    if (vertexLayout != nullptr) {
        vertexLayout->setStride(
            static_cast<NS::UInteger>(vertexBinding.stride));
        vertexLayout->setStepFunction(MTL::VertexStepFunctionPerVertex);
        vertexLayout->setStepRate(1);
    }

    if (hasInstance) {
        auto *instanceLayout = layouts->object(kInstanceStreamBufferIndex);
        if (instanceLayout != nullptr) {
            instanceLayout->setStride(instanceStride);
            instanceLayout->setStepFunction(MTL::VertexStepFunctionPerInstance);
            instanceLayout->setStepRate(instanceDivisor);
        }
    }

    ensureMetalDepthStencilState(state, deviceState.device,
                                 this->depthTestEnabled,
                                 this->depthWriteEnabled, desiredDepthCompare);
    state.depthTestEnabled = this->depthTestEnabled;
    state.depthWriteEnabled = this->depthWriteEnabled;
    state.depthCompare = desiredDepthCompare;
#endif
}

void Pipeline::bind() {
#ifdef OPENGL
    if (this->shaderProgram == nullptr) {
        throw std::runtime_error(
            "Pipeline::bind() called but no shader program is set. "
            "Call setShaderProgram() or refreshPipeline() first.");
    }
    glUseProgram(this->shaderProgram->programID);

    if (this->viewportWidth > 0 && this->viewportHeight > 0) {
        glViewport(this->viewportX, this->viewportY, this->viewportWidth,
                   this->viewportHeight);
    }

    glPolygonMode(GL_FRONT_AND_BACK,
                  this->getGLRasterizerMode(this->rasterizerMode));

    if (this->cullMode != CullMode::None) {
        glEnable(GL_CULL_FACE);
        glCullFace(this->getGLCullMode(this->cullMode));
    } else {
        glDisable(GL_CULL_FACE);
    }

    glFrontFace(this->getGLFrontFace(this->frontFace));

    if (this->depthTestEnabled) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(this->getGLCompareOp(this->depthCompareOp));
    } else {
        glDisable(GL_DEPTH_TEST);
    }

    glDepthMask(this->depthWriteEnabled ? GL_TRUE : GL_FALSE);

    if (this->blendingEnabled) {
        glEnable(GL_BLEND);
        glBlendFunc(this->getGLBlendFactor(this->blendSrcFactor),
                    this->getGLBlendFactor(this->blendDstFactor));
        glBlendEquation(this->getGLBlendEquation(this->blendEquation));
    } else {
        glDisable(GL_BLEND);
    }

    if (this->multisamplingEnabled) {
        glEnable(GL_MULTISAMPLE);
    } else {
        glDisable(GL_MULTISAMPLE);
    }

    if (this->polygonOffsetEnabled) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(this->polygonOffsetFactor, this->polygonOffsetUnits);
    } else {
        glDisable(GL_POLYGON_OFFSET_FILL);
    }

    // Handle clip distances (up to 8 supported)
    for (int i = 0; i < 8; ++i) {
        bool shouldEnable = std::find(this->enabledClipDistances.begin(),
                                      this->enabledClipDistances.end(),
                                      i) != this->enabledClipDistances.end();
        if (shouldEnable) {
            glEnable(GL_CLIP_DISTANCE0 + i);
        } else {
            glDisable(GL_CLIP_DISTANCE0 + i);
        }
    }
#elif defined(METAL)
    auto &state = metal::pipelineState(this);
    state.primitiveType = toMetalPrimitive(this->primitiveStyle);
    state.cullMode = toMetalCull(this->cullMode);
    state.frontFace = toMetalWinding(this->frontFace);
    state.fillMode = toMetalFillMode(this->rasterizerMode);
    MTL::CompareFunction desiredDepthCompare =
        this->depthTestEnabled ? toMetalCompare(this->depthCompareOp)
                               : MTL::CompareFunctionAlways;
    state.blendingEnabled = this->blendingEnabled;
    state.blendSrc = toMetalBlendFactor(this->blendSrcFactor);
    state.blendDst = toMetalBlendFactor(this->blendDstFactor);
    state.blendOp = toMetalBlendOperation(this->blendEquation);
    state.polygonOffsetEnabled = this->polygonOffsetEnabled;
    state.polygonOffsetFactor = this->polygonOffsetFactor;
    state.polygonOffsetUnits = this->polygonOffsetUnits;
    state.viewportX = this->viewportX;
    state.viewportY = this->viewportY;
    state.viewportWidth = this->viewportWidth;
    state.viewportHeight = this->viewportHeight;
    if (!state.suppressTextureReset) {
        state.texturesByUnit.clear();
    }
    state.suppressTextureReset = false;
    if (shaderProgram != nullptr && shaderProgram->isComputeProgram()) {
        return;
    }

    if (Device::globalInstance != nullptr) {
        auto &deviceState = metal::deviceState(Device::globalInstance);
        if (deviceState.device != nullptr) {
            ensureMetalDepthStencilState(
                state, deviceState.device, this->depthTestEnabled,
                this->depthWriteEnabled, desiredDepthCompare);
        }
    }
    state.depthTestEnabled = this->depthTestEnabled;
    state.depthWriteEnabled = this->depthWriteEnabled;
    state.depthCompare = desiredDepthCompare;
#endif
}

bool Pipeline::operator==(const std::shared_ptr<Pipeline> &pipeline) const {
    if (this->primitiveStyle != pipeline->primitiveStyle) {
        return false;
    }
    if (this->rasterizerMode != pipeline->rasterizerMode) {
        return false;
    }
    if (this->cullMode != pipeline->cullMode) {
        return false;
    }
    if (this->frontFace != pipeline->frontFace) {
        return false;
    }
    if (this->blendingEnabled != pipeline->blendingEnabled) {
        return false;
    }
    if (this->blendSrcFactor != pipeline->blendSrcFactor) {
        return false;
    }
    if (this->blendDstFactor != pipeline->blendDstFactor) {
        return false;
    }
    if (this->depthTestEnabled != pipeline->depthTestEnabled) {
        return false;
    }
    if (this->depthCompareOp != pipeline->depthCompareOp) {
        return false;
    }
    if (this->shaderProgram != pipeline->shaderProgram) {
        return false;
    }
    if (this->vertexAttributes != pipeline->vertexAttributes) {
        return false;
    }
    if (this->vertexBinding.inputRate != pipeline->vertexBinding.inputRate) {
        return false;
    }
    if (this->vertexBinding.stride != pipeline->vertexBinding.stride) {
        return false;
    }
    if (this->computeThreadgroupX != pipeline->computeThreadgroupX) {
        return false;
    }
    if (this->computeThreadgroupY != pipeline->computeThreadgroupY) {
        return false;
    }
    if (this->computeThreadgroupZ != pipeline->computeThreadgroupZ) {
        return false;
    }
    return true;
}

void Pipeline::setUniform1f(const std::string &name, float v0) {
#ifdef OPENGL
    glUniform1f(
        glGetUniformLocation(this->shaderProgram->programID, name.c_str()), v0);
#elif defined(METAL)
    updateMetalUniform(this, name, &v0, sizeof(float), true);
#endif
}

void Pipeline::setUniformMat4f(const std::string &name,
                               const glm::mat4 &matrix) {
#ifdef OPENGL
    glUniformMatrix4fv(
        glGetUniformLocation(this->shaderProgram->programID, name.c_str()), 1,
        GL_FALSE, &matrix[0][0]);
#elif defined(METAL)
    updateMetalUniform(this, name, &matrix[0][0], sizeof(glm::mat4), true);
#endif
}

void Pipeline::setUniform3f(const std::string &name, float v0, float v1,
                            float v2) {
#ifdef OPENGL
    glUniform3f(
        glGetUniformLocation(this->shaderProgram->programID, name.c_str()), v0,
        v1, v2);
#elif defined(METAL)
    float data[3] = {v0, v1, v2};
    updateMetalUniform(this, name, data, sizeof(data), true);
#endif
}

void Pipeline::setUniform1i(const std::string &name, int v0) {
#ifdef OPENGL
    glUniform1i(
        glGetUniformLocation(this->shaderProgram->programID, name.c_str()), v0);
#elif defined(METAL)
    updateMetalUniform(this, name, &v0, sizeof(int), true);
#endif
}

void Pipeline::setUniformBool(const std::string &name, bool value) {
#ifdef OPENGL
    glUniform1i(
        glGetUniformLocation(this->shaderProgram->programID, name.c_str()),
        (int)value);
#elif defined(METAL)
    int intValue = value ? 1 : 0;
    updateMetalUniform(this, name, &intValue, sizeof(int), true);
#endif
}

void Pipeline::setUniform4f(const std::string &name, float v0, float v1,
                            float v2, float v3) {
#ifdef OPENGL
    glUniform4f(
        glGetUniformLocation(this->shaderProgram->programID, name.c_str()), v0,
        v1, v2, v3);
#elif defined(METAL)
    float data[4] = {v0, v1, v2, v3};
    updateMetalUniform(this, name, data, sizeof(data), true);
#endif
}

void Pipeline::setUniform2f(const std::string &name, float v0, float v1) {
#ifdef OPENGL
    glUniform2f(
        glGetUniformLocation(this->shaderProgram->programID, name.c_str()), v0,
        v1);
#elif defined(METAL)
    float data[2] = {v0, v1};
    updateMetalUniform(this, name, data, sizeof(data), true);
#endif
}

void Pipeline::bindBufferData(const std::string &name, const void *data,
                              size_t size) {
    if (!shaderProgram || !data || size == 0) {
        return;
    }

#ifdef OPENGL
    (void)name;
    (void)data;
    (void)size;
#elif defined(METAL)
    updateMetalUniform(this, name, data, size, false);
#endif
}

void Pipeline::bindBuffer(const std::string &name,
                          const std::shared_ptr<Buffer> &buffer, int callerId) {
#ifdef OPENGL
    (void)name;
    (void)buffer;
    (void)callerId;
    throw std::runtime_error(
        "bindBuffer(opal::Buffer) is not supported on OpenGL");
#elif defined(METAL)
    (void)callerId;
    if (!shaderProgram) {
        return;
    }
    if (buffer != nullptr && buffer->usage != BufferUsage::UniformBuffer &&
        buffer->usage != BufferUsage::ShaderRead &&
        buffer->usage != BufferUsage::ShaderReadWrite &&
        buffer->usage != BufferUsage::GeneralPurpose) {
        throw std::runtime_error("bindBuffer(opal::Buffer) requires a "
                                 "shader-readable buffer usage");
    }

    auto &programState = metal::programState(shaderProgram.get());
    auto bindings = metal::resolveBufferBindings(programState, name);
    if (bindings.empty()) {
        throw std::runtime_error("Metal buffer binding not found: " + name);
    }

    auto &pipelineState = metal::pipelineState(this);
    bool matchedStage = false;
    for (const auto &binding : bindings) {
        if (binding.vertexStage) {
            matchedStage = true;
            uint32_t key = metal::stageBindingKey(
                binding.index, metal::MetalProgramStage::Vertex);
            if (buffer != nullptr) {
                pipelineState.shaderBuffers[key] = buffer;
            } else {
                pipelineState.shaderBuffers.erase(key);
            }
        }
        if (binding.fragmentStage) {
            matchedStage = true;
            uint32_t key = metal::stageBindingKey(
                binding.index, metal::MetalProgramStage::Fragment);
            if (buffer != nullptr) {
                pipelineState.shaderBuffers[key] = buffer;
            } else {
                pipelineState.shaderBuffers.erase(key);
            }
        }
        if (binding.computeStage) {
            matchedStage = true;
            uint32_t key = metal::stageBindingKey(
                binding.index, metal::MetalProgramStage::Compute);
            if (buffer != nullptr) {
                pipelineState.shaderBuffers[key] = buffer;
            } else {
                pipelineState.shaderBuffers.erase(key);
            }
        }
    }

    if (!matchedStage) {
        throw std::runtime_error("Metal buffer binding not found: " + name);
    }
#endif
}

void Pipeline::bindShaderReadWriteBuffer(const std::string &name,
                                         const std::shared_ptr<Buffer> &buffer,
                                         int callerId) {
#ifdef METAL
    (void)callerId;
    if (!shaderProgram) {
        return;
    }
    if (buffer != nullptr && buffer->usage != BufferUsage::ShaderReadWrite &&
        buffer->usage != BufferUsage::ShaderRead &&
        buffer->usage != BufferUsage::GeneralPurpose) {
        throw std::runtime_error("bindShaderReadWriteBuffer requires a "
                                 "shader-readable buffer usage");
    }

    auto &programState = metal::programState(shaderProgram.get());
    auto bindings = metal::resolveBufferBindings(programState, name);
    auto &pipelineState = metal::pipelineState(this);

    bool matchedComputeBinding = false;
    for (const auto &binding : bindings) {
        if (!binding.computeStage) {
            continue;
        }
        matchedComputeBinding = true;
        uint32_t key = metal::stageBindingKey(
            binding.index, metal::MetalProgramStage::Compute);
        if (buffer != nullptr) {
            pipelineState.shaderBuffers[key] = buffer;
        } else {
            pipelineState.shaderBuffers.erase(key);
        }
    }

    if (!matchedComputeBinding) {
        throw std::runtime_error("Metal compute buffer binding not found: " +
                                 name);
    }
#else
    (void)name;
    (void)buffer;
    (void)callerId;
    throw std::runtime_error(
        "bindShaderReadWriteBuffer is only supported on Metal");
#endif
}


} // namespace opal
