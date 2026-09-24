//
// vulkan_pipeline.cpp
// As part of the Opal project
// Created by Max Van den Eynde in 2026
// --------------------------------------------------
// Description: Pipeline functions
// Copyright (c) 2026 Max Van den Eynde
//

#ifdef VULKAN
#include "diagnostics.h"
#include "opal/opal.h"
#include "vulkan_state.h"
#include <algorithm>
#include <cstring>
#include <vulkan/vulkan.h>

namespace opal::vulkan {
VkBlendFactor blenderFuncToVk(BlendFunc func) {
    switch (func) {
    case BlendFunc::Zero:
        return VK_BLEND_FACTOR_ZERO;
    case BlendFunc::One:
        return VK_BLEND_FACTOR_ONE;
    case BlendFunc::SrcColor:
        return VK_BLEND_FACTOR_SRC_COLOR;
    case BlendFunc::OneMinusSrcColor:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case BlendFunc::DstColor:
        return VK_BLEND_FACTOR_DST_COLOR;
    case BlendFunc::OneMinusDstColor:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case BlendFunc::SrcAlpha:
        return VK_BLEND_FACTOR_SRC_ALPHA;
    case BlendFunc::OneMinusSrcAlpha:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case BlendFunc::DstAlpha:
        return VK_BLEND_FACTOR_DST_ALPHA;
    case BlendFunc::OneMinusDstAlpha:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    default:
        return VK_BLEND_FACTOR_ONE;
    }
}

VkBlendOp blenderOpToVk(BlendEquation op) {
    switch (op) {
    case BlendEquation::Add:
        return VK_BLEND_OP_ADD;
    case BlendEquation::Subtract:
        return VK_BLEND_OP_SUBTRACT;
    case BlendEquation::ReverseSubtract:
        return VK_BLEND_OP_REVERSE_SUBTRACT;
    case BlendEquation::Min:
        return VK_BLEND_OP_MIN;
    case BlendEquation::Max:
        return VK_BLEND_OP_MAX;
    default:
        return VK_BLEND_OP_ADD;
    }
}

VkLogicOp logicOpToVk(LogicOp op) {
    switch (op) {
    case LogicOp::Clear:
        return VK_LOGIC_OP_CLEAR;
    case LogicOp::And:
        return VK_LOGIC_OP_AND;
    case LogicOp::AndReverse:
        return VK_LOGIC_OP_AND_REVERSE;
    case LogicOp::Copy:
        return VK_LOGIC_OP_COPY;
    case LogicOp::AndInverted:
        return VK_LOGIC_OP_AND_INVERTED;
    case LogicOp::NoOp:
        return VK_LOGIC_OP_NO_OP;
    case LogicOp::Xor:
        return VK_LOGIC_OP_XOR;
    case LogicOp::Or:
        return VK_LOGIC_OP_OR;
    case LogicOp::Nor:
        return VK_LOGIC_OP_NOR;
    case LogicOp::Equivalent:
        return VK_LOGIC_OP_EQUIVALENT;
    case LogicOp::Invert:
        return VK_LOGIC_OP_INVERT;
    case LogicOp::OrReverse:
        return VK_LOGIC_OP_OR_REVERSE;
    case LogicOp::CopyInverted:
        return VK_LOGIC_OP_COPY_INVERTED;
    case LogicOp::OrInverted:
        return VK_LOGIC_OP_OR_INVERTED;
    case LogicOp::Nand:
        return VK_LOGIC_OP_NAND;
    case LogicOp::Set:
        return VK_LOGIC_OP_SET;
    default:
        return VK_LOGIC_OP_COPY;
    }
}

VkCompareOp compareOpToVk(CompareOp op) {
    switch (op) {
    case CompareOp::Never:
        return VK_COMPARE_OP_NEVER;
    case CompareOp::Less:
        return VK_COMPARE_OP_LESS;
    case CompareOp::Equal:
        return VK_COMPARE_OP_EQUAL;
    case CompareOp::LessEqual:
        return VK_COMPARE_OP_LESS_OR_EQUAL;
    case CompareOp::Greater:
        return VK_COMPARE_OP_GREATER;
    case CompareOp::NotEqual:
        return VK_COMPARE_OP_NOT_EQUAL;
    case CompareOp::GreaterEqual:
        return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case CompareOp::Always:
        return VK_COMPARE_OP_ALWAYS;
    default:
        return VK_COMPARE_OP_LESS;
    }
}

VkPrimitiveTopology primitiveStyleToVk(PrimitiveStyle style) {
    switch (style) {
    case PrimitiveStyle::Points:
        return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    case PrimitiveStyle::Lines:
        return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case PrimitiveStyle::LineStrip:
        return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    case PrimitiveStyle::Triangles:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case PrimitiveStyle::TriangleStrip:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    case PrimitiveStyle::TriangleFan:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
    case PrimitiveStyle::Patches:
        return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
    default:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }
}

VkPolygonMode rasterizerModeToVk(RasterizerMode mode) {
    switch (mode) {
    case RasterizerMode::Fill:
        return VK_POLYGON_MODE_FILL;
    case RasterizerMode::Line:
        return VK_POLYGON_MODE_LINE;
    case RasterizerMode::Point:
        return VK_POLYGON_MODE_POINT;
    default:
        return VK_POLYGON_MODE_FILL;
    }
}

VkCullModeFlags cullModeToVk(CullMode mode) {
    switch (mode) {
    case CullMode::None:
        return VK_CULL_MODE_NONE;
    case CullMode::Front:
        return VK_CULL_MODE_FRONT_BIT;
    case CullMode::Back:
        return VK_CULL_MODE_BACK_BIT;
    case CullMode::FrontAndBack:
        return VK_CULL_MODE_FRONT_AND_BACK;
    default:
        return VK_CULL_MODE_BACK_BIT;
    }
}

VkFrontFace frontFaceToVk(FrontFace face) {
    switch (face) {
    case FrontFace::Clockwise:
        return VK_FRONT_FACE_CLOCKWISE;
    case FrontFace::CounterClockwise:
        return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    default:
        return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    }
}

VkFormat vertexAttributeFormatToVk(VertexAttributeType type, uint size,
                                   bool normalized) {
    switch (type) {
    case VertexAttributeType::Float:
        switch (size) {
        case 1:
            return VK_FORMAT_R32_SFLOAT;
        case 2:
            return VK_FORMAT_R32G32_SFLOAT;
        case 3:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case 4:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        default:
            throw std::runtime_error(
                "Unsupported vertex attribute size for float type");
        }
    case VertexAttributeType::Int:
        switch (size) {
        case 1:
            return VK_FORMAT_R32_SINT;
        case 2:
            return VK_FORMAT_R32G32_SINT;
        case 3:
            return VK_FORMAT_R32G32B32_SINT;
        case 4:
            return VK_FORMAT_R32G32B32A32_SINT;
        default:
            throw std::runtime_error(
                "Unsupported vertex attribute size for int type");
        }
    case VertexAttributeType::UnsignedInt:
        switch (size) {
        case 1:
            return VK_FORMAT_R32_UINT;
        case 2:
            return VK_FORMAT_R32G32_UINT;
        case 3:
            return VK_FORMAT_R32G32B32_UINT;
        case 4:
            return VK_FORMAT_R32G32B32A32_UINT;
        default:
            throw std::runtime_error(
                "Unsupported vertex attribute size for unsigned int type");
        }
    case VertexAttributeType::Short:
        if (normalized) {
            switch (size) {
            case 2:
                return VK_FORMAT_R16G16_SNORM;
            case 4:
                return VK_FORMAT_R16G16B16A16_SNORM;
            default:
                throw std::runtime_error("Unsupported vertex attribute size "
                                         "for normalized short type");
            }
        } else {
            switch (size) {
            case 2:
                return VK_FORMAT_R16G16_SINT;
            case 4:
                return VK_FORMAT_R16G16B16A16_SINT;
            default:
                throw std::runtime_error(
                    "Unsupported vertex attribute size for short type");
            }
        }
    case VertexAttributeType::UnsignedShort:
        if (normalized) {
            switch (size) {
            case 2:
                return VK_FORMAT_R16G16_UNORM;
            case 4:
                return VK_FORMAT_R16G16B16A16_UNORM;
            default:
                throw std::runtime_error("Unsupported vertex attribute size "
                                         "for normalized unsigned short type");
            }
        } else {
            switch (size) {
            case 2:
                return VK_FORMAT_R16G16_UINT;
            case 4:
                return VK_FORMAT_R16G16B16A16_UINT;
            default:
                throw std::runtime_error("Unsupported vertex attribute size "
                                         "for unsigned short type");
            }
        }
    case VertexAttributeType::Byte:
        if (normalized) {
            switch (size) {
            case 4:
                return VK_FORMAT_R8G8B8A8_SNORM;
            default:
                throw std::runtime_error("Unsupported vertex attribute size "
                                         "for normalized byte type");
            }
        } else {
            switch (size) {
            case 4:
                return VK_FORMAT_R8G8B8A8_SINT;
            default:
                throw std::runtime_error(
                    "Unsupported vertex attribute size for byte type");
            }
        }
    case VertexAttributeType::UnsignedByte:
        if (normalized) {
            switch (size) {
            case 4:
                return VK_FORMAT_R8G8B8A8_UNORM;
            default:
                throw std::runtime_error("Unsupported vertex attribute size "
                                         "for normalized unsigned byte type");
            }
        } else {
            switch (size) {
            case 4:
                return VK_FORMAT_R8G8B8A8_UINT;
            default:
                throw std::runtime_error(
                    "Unsupported vertex attribute size for unsigned byte type");
            }
        }
    case VertexAttributeType::Double:
        switch (size) {
        case 1:
            return VK_FORMAT_R64_SFLOAT;
        case 2:
            return VK_FORMAT_R64G64_SFLOAT;
        case 3:
            return VK_FORMAT_R64G64B64_SFLOAT;
        case 4:
            return VK_FORMAT_R64G64B64A64_SFLOAT;
        default:
            throw std::runtime_error(
                "Unsupported vertex attribute size for double type");
        }
    default:
        throw std::runtime_error("Unsupported vertex attribute type");
    }
}

VkPipeline createOrGetGraphicsPipeline(Pipeline *pipeline,
                                       const RenderTargetSignature &target) {
    if (pipeline == nullptr) {
        throw std::runtime_error("Pipeline pointer is null");
    }

    auto &state = pipelineState(pipeline);

    if (!state.built) {
        throw std::runtime_error("Pipeline must be built before creating or "
                                 "retrieving Vulkan pipeline");
    }

    if (pipeline->shaderProgram == nullptr) {
        throw std::runtime_error("Pipeline must have a shader program before "
                                 "creating or retrieving Vulkan pipeline");
    }

    if (pipeline->shaderProgram->isComputeProgram()) {
        throw std::runtime_error("Pipeline is a compute program; use "
                                 "createOrGetComputePipeline instead");
    }

    auto existing = state.graphicsPipelines.find(target);

    if (existing != state.graphicsPipelines.end()) {
        return existing->second;
    }

    auto &program = programState(pipeline->shaderProgram.get());
    auto &device = deviceState(Device::globalInstance);

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount =
        static_cast<uint32_t>(state.vertexBindings.size());
    vertexInput.pVertexBindingDescriptions =
        state.vertexBindings.empty() ? nullptr : state.vertexBindings.data();
    vertexInput.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(state.vertexAttributes.size());
    vertexInput.pVertexAttributeDescriptions =
        state.vertexAttributes.empty() ? nullptr
                                       : state.vertexAttributes.data();

    VkPipelineVertexInputDivisorStateCreateInfoEXT divisorInfo{};
    if (!state.vertexDivisors.empty()) {
        divisorInfo.sType =
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT;
        divisorInfo.vertexBindingDivisorCount =
            static_cast<uint32_t>(state.vertexDivisors.size());
        divisorInfo.pVertexBindingDivisors = state.vertexDivisors.data();
        vertexInput.pNext = &divisorInfo;
    }

    VkPipelineDynamicStateCreateInfo dynamicInfo{};
    dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicInfo.dynamicStateCount =
        static_cast<uint32_t>(state.dynamicStates.size());
    dynamicInfo.pDynamicStates =
        state.dynamicStates.empty() ? nullptr : state.dynamicStates.data();

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = target.samples;
    multisampling.sampleShadingEnable = VK_FALSE;

    std::vector<VkPipelineColorBlendAttachmentState> blendAttachments(
        target.colorFormats.size(), state.colorBlendAttachment);

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.logicOpEnable = state.logicOpEnabled ? VK_TRUE : VK_FALSE;
    colorBlend.logicOp = state.logicOp;
    colorBlend.attachmentCount = static_cast<uint32_t>(blendAttachments.size());
    colorBlend.pAttachments =
        blendAttachments.empty() ? nullptr : blendAttachments.data();

    VkPipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.viewMask = target.viewMask;
    renderingInfo.colorAttachmentCount =
        static_cast<uint32_t>(target.colorFormats.size());
    renderingInfo.pColorAttachmentFormats =
        target.colorFormats.empty() ? nullptr : target.colorFormats.data();
    renderingInfo.depthAttachmentFormat = target.depthFormat;
    renderingInfo.stencilAttachmentFormat = target.stencilFormat;

    VkGraphicsPipelineCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    createInfo.pNext = &renderingInfo;
    createInfo.stageCount = static_cast<uint32_t>(program.shaderStages.size());
    createInfo.pStages = program.shaderStages.data();
    createInfo.pVertexInputState = &vertexInput;
    createInfo.pInputAssemblyState = &state.inputAssembly;
    createInfo.pTessellationState =
        state.hasTessellation ? &state.tessellation : nullptr;
    createInfo.pViewportState = &state.viewport;
    createInfo.pRasterizationState = &state.rasterization;
    createInfo.pMultisampleState = &multisampling;
    createInfo.pDepthStencilState = &state.depthStencil;
    createInfo.pColorBlendState = &colorBlend;
    createInfo.pDynamicState = &dynamicInfo;
    createInfo.layout = program.pipelineLayout;
    createInfo.renderPass = VK_NULL_HANDLE;
    createInfo.subpass = 0;
    createInfo.basePipelineHandle = VK_NULL_HANDLE;
    createInfo.basePipelineIndex = -1;

    VkPipeline vkPipeline = VK_NULL_HANDLE;

    VULKAN_GUARD(vkCreateGraphicsPipelines(device.device, VK_NULL_HANDLE, 1,
                                           &createInfo, nullptr, &vkPipeline),
                 "Failed to create Vulkan graphics pipeline");

    state.graphicsPipelines.emplace(target, vkPipeline);

    return vkPipeline;
}

void bindPipeline(CommandBuffer *commandBuffer, Pipeline *pipeline,
                  const RenderTargetSignature &target,
                  VkExtent2D renderExtent) {
    if (commandBuffer == nullptr || pipeline == nullptr) {
        throw std::runtime_error(
            "bindPipeline requires a command buffer and pipeline");
    }

    auto &cmdState = commandBufferState(commandBuffer);
    auto &pipelineState = vulkan::pipelineState(pipeline);

    if (!pipelineState.built) {
        throw std::runtime_error(
            "Cannot bind Vulkan pipeline before Pipeline::build()");
    }

    if (pipeline->shaderProgram == nullptr) {
        throw std::runtime_error(
            "Cannot bind Vulkan pipeline without shader program");
    }

    auto &programState = vulkan::programState(pipeline->shaderProgram.get());

    const bool compute = pipeline->shaderProgram->isComputeProgram();

    VkPipeline vkPipeline = VK_NULL_HANDLE;
    VkPipelineBindPoint bindPoint;

    if (compute) {
        vkPipeline = pipelineState.computePipeline;
        bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
    } else {
        vkPipeline = createOrGetGraphicsPipeline(pipeline, target);

        bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    }

    if (vkPipeline == VK_NULL_HANDLE) {
        throw std::runtime_error("Attempted to bind null Vulkan pipeline");
    }

    vkCmdBindPipeline(cmdState.commandBuffer, bindPoint, vkPipeline);

    ensureDescriptorSets(pipeline);
    updateDescriptors(commandBuffer, pipeline);

    if (!pipelineState.descriptorSets.empty()) {
        vkCmdBindDescriptorSets(
            cmdState.commandBuffer, bindPoint, programState.pipelineLayout, 0,
            static_cast<uint32_t>(pipelineState.descriptorSets.size()),
            pipelineState.descriptorSets.data(), 0, nullptr);
    }

    cmdState.activePipeline = pipeline;
    cmdState.boundPipeline = vkPipeline;
    cmdState.boundPipelineLayout = programState.pipelineLayout;
    cmdState.boundPipelineBindPoint = bindPoint;

    if (!compute) {
        applyDynamicPipelineState(commandBuffer, pipeline, renderExtent);
    }
}

void applyDynamicPipelineState(CommandBuffer *commandBuffer, Pipeline *pipeline,
                               VkExtent2D renderExtent) {
    auto &state = commandBufferState(commandBuffer);

    if (pipeline->shaderProgram->isComputeProgram()) {
        return;
    }

    VkViewport viewport{};

    viewport.x = static_cast<float>(pipeline->viewportX);

    viewport.y = static_cast<float>(pipeline->viewportY);

    viewport.width = pipeline->viewportWidth > 0
                         ? static_cast<float>(pipeline->viewportWidth)
                         : static_cast<float>(renderExtent.width);

    viewport.height = pipeline->viewportHeight > 0
                          ? static_cast<float>(pipeline->viewportHeight)
                          : static_cast<float>(renderExtent.height);

    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vkCmdSetViewport(state.commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};

    scissor.offset = {0, 0};
    scissor.extent = renderExtent;

    vkCmdSetScissor(state.commandBuffer, 0, 1, &scissor);

    if (pipeline->polygonOffsetEnabled) {
        vkCmdSetDepthBias(state.commandBuffer, pipeline->polygonOffsetUnits,
                          0.0f, pipeline->polygonOffsetFactor);
    }
}

void ensureDescriptorSets(Pipeline *pipeline) {
    auto &state = pipelineState(pipeline);
    if (state.descriptorsAllocated) {
        return;
    }

    auto &program = programState(pipeline->shaderProgram.get());
    if (program.descriptorSetLayouts.empty()) {
        state.descriptorsAllocated = true;
        return;
    }

    auto &device = deviceState(Device::globalInstance);

    std::unordered_map<VkDescriptorType, uint32_t> descriptorCounts;
    for (const auto &binding : program.bindings) {
        VkDescriptorType type = descriptorTypeToVk(binding.type);

        descriptorCounts[type] += binding.count;
    }

    std::vector<VkDescriptorPoolSize> poolSizes;
    poolSizes.reserve(descriptorCounts.size());

    for (const auto &[type, count] : descriptorCounts) {
        poolSizes.push_back({.type = type, .descriptorCount = count});
    }

    VkDescriptorPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .maxSets = static_cast<uint32_t>(program.descriptorSetLayouts.size()),
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()};

    VULKAN_GUARD(vkCreateDescriptorPool(device.device, &poolInfo, nullptr,
                                        &state.descriptorPool),
                 "Failed to create Vulkan descriptor pool");

    state.descriptorSets.resize(program.descriptorSetLayouts.size());

    VkDescriptorSetAllocateInfo allocationInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = state.descriptorPool,
        .descriptorSetCount =
            static_cast<uint32_t>(program.descriptorSetLayouts.size()),
        .pSetLayouts = program.descriptorSetLayouts.data()};

    VULKAN_GUARD(vkAllocateDescriptorSets(device.device, &allocationInfo,
                                          state.descriptorSets.data()),
                 "Failed to allocate Vulkan descriptor sets");

    state.descriptorsAllocated = true;
    state.descriptorsDirty = true;
}

void updateDescriptors(CommandBuffer *commandBuffer, Pipeline *pipeline) {
    auto &state = pipelineState(pipeline);
    auto &program = programState(pipeline->shaderProgram.get());
    auto &device = deviceState(Device::globalInstance);
    auto &command = commandBufferState(commandBuffer);

    for (auto &[key, block] : state.uniformBlocks) {
        if (!block.dirty || block.buffers.empty() ||
            block.buffers[0] == nullptr) {
            continue;
        }
        block.buffers[0]->updateData(0, block.data.size(), block.data.data());
        block.dirty = false;
    }

    if (!state.descriptorsAllocated) {
        return;
    }

    std::vector<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkDescriptorImageInfo> imageInfos;
    std::vector<VkWriteDescriptorSet> writes;

    size_t descriptorCount = 0;
    for (const auto &binding : program.bindings) {
        descriptorCount += binding.count;
    }
    bufferInfos.reserve(descriptorCount);
    imageInfos.reserve(descriptorCount);
    writes.reserve(program.bindings.size());

    for (const auto &binding : program.bindings) {
        uint64_t key = bindingKey(binding.set, binding.binding);
        if (binding.type == ShaderResourceType::UniformBuffer ||
            binding.type == ShaderResourceType::StorageBuffer) {
            auto resourceIt = state.boundBuffers.find(key);
            if (resourceIt == state.boundBuffers.end() ||
                resourceIt->second.buffer == nullptr) {
                continue;
            }
            const BoundBufferResource &resource = resourceIt->second;
            auto &buffer = bufferState(resource.buffer.get());
            if (buffer.buffer == VK_NULL_HANDLE ||
                resource.offset >= buffer.size) {
                throw std::runtime_error(
                    "Invalid Vulkan buffer descriptor range");
            }
            VkDeviceSize range = resource.range;
            if (range == VK_WHOLE_SIZE) {
                range = buffer.size - resource.offset;
            }
            const size_t firstInfo = bufferInfos.size();
            for (uint32_t index = 0; index < binding.count; ++index) {
                bufferInfos.push_back({.buffer = buffer.buffer,
                                       .offset = resource.offset,
                                       .range = range});
            }
            writes.push_back(
                {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                 .pNext = nullptr,
                 .dstSet = state.descriptorSets.at(binding.set),
                 .dstBinding = binding.binding,
                 .dstArrayElement = 0,
                 .descriptorCount = binding.count,
                 .descriptorType = descriptorTypeToVk(binding.type),
                 .pImageInfo = nullptr,
                 .pBufferInfo = &bufferInfos[firstInfo],
                 .pTexelBufferView = nullptr});
            continue;
        }

        auto imageIt = state.boundImages.find(key);
        if (imageIt == state.boundImages.end() ||
            imageIt->second.textures.empty()) {
            continue;
        }
        auto fallback = std::find_if(
            imageIt->second.textures.begin(), imageIt->second.textures.end(),
            [](const std::shared_ptr<Texture> &texture) {
                return texture != nullptr;
            });
        if (fallback == imageIt->second.textures.end()) {
            continue;
        }
        const size_t firstInfo = imageInfos.size();
        for (uint32_t index = 0; index < binding.count; ++index) {
            const std::shared_ptr<Texture> &texture =
                index < imageIt->second.textures.size() &&
                        imageIt->second.textures[index] != nullptr
                    ? imageIt->second.textures[index]
                    : *fallback;
            auto &textureState = vulkan::textureState(texture.get());
            const bool usesImage = binding.type != ShaderResourceType::Sampler;
            const bool usesSampler =
                binding.type == ShaderResourceType::Sampler ||
                binding.type == ShaderResourceType::CombinedImageSampler;
            if (usesImage && textureState.imageView == VK_NULL_HANDLE) {
                throw std::runtime_error(
                    "Vulkan texture descriptor has no image view");
            }
            if (usesSampler && textureState.sampler == VK_NULL_HANDLE) {
                throw std::runtime_error(
                    "Vulkan texture descriptor has no sampler");
            }
            VkImageLayout layout =
                binding.type == ShaderResourceType::StorageImage
                    ? VK_IMAGE_LAYOUT_GENERAL
                    : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            if (usesImage) {
                transitionTexture(command.commandBuffer, textureState, layout);
            }
            imageInfos.push_back(
                {.sampler = usesSampler ? textureState.sampler : VK_NULL_HANDLE,
                 .imageView =
                     usesImage ? textureState.imageView : VK_NULL_HANDLE,
                 .imageLayout =
                     usesImage ? layout : VK_IMAGE_LAYOUT_UNDEFINED});
        }
        writes.push_back({.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                          .pNext = nullptr,
                          .dstSet = state.descriptorSets.at(binding.set),
                          .dstBinding = binding.binding,
                          .dstArrayElement = 0,
                          .descriptorCount = binding.count,
                          .descriptorType = descriptorTypeToVk(binding.type),
                          .pImageInfo = &imageInfos[firstInfo],
                          .pBufferInfo = nullptr,
                          .pTexelBufferView = nullptr});
    }

    if (!writes.empty()) {
        vkUpdateDescriptorSets(device.device,
                               static_cast<uint32_t>(writes.size()),
                               writes.data(), 0, nullptr);
    }

    state.descriptorsDirty = false;
}

void updateVulkanUniform(Pipeline *pipeline, const std::string &name,
                         const void *data, size_t size,
                         bool clampToDeclaredSize) {
    if (pipeline == nullptr || pipeline->shaderProgram == nullptr ||
        data == nullptr || size == 0) {
        return;
    }

    auto &programState = vulkan::programState(pipeline->shaderProgram.get());

    auto locationIt = programState.uniformsByName.find(name);
    if (locationIt == programState.uniformsByName.end()) {
        return;
    }

    const auto &member = locationIt->second;

    uint64_t key = vulkan::bindingKey(member.set, member.binding);
    auto &pipelineState = vulkan::pipelineState(pipeline);

    auto blockIt = pipelineState.uniformBlocks.find(key);
    if (blockIt == pipelineState.uniformBlocks.end()) {
        throw std::runtime_error("Uniform block not initialized for '" + name +
                                 "'");
    }

    auto &block = blockIt->second;

    size_t writeSize = clampToDeclaredSize ? std::min(size, member.size) : size;
    if (member.offset + writeSize > block.data.size()) {
        throw std::runtime_error("Uniform write exceeds block size: " + name);
    }

    std::memcpy(block.data.data() + member.offset, data, writeSize);

    block.dirty = true;
}

VkVertexInputRate vertexBindingRateToVk(VertexBindingInputRate rate) {
    switch (rate) {
    case VertexBindingInputRate::Vertex:
        return VK_VERTEX_INPUT_RATE_VERTEX;
    case VertexBindingInputRate::Instance:
        return VK_VERTEX_INPUT_RATE_INSTANCE;
    default:
        return VK_VERTEX_INPUT_RATE_VERTEX;
    }
}

} // namespace opal::vulkan
#endif
