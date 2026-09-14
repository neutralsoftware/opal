//
// vulkan_slang.cpp
// As part of the Opal project
// Created by Max Van den Eynde in 2026
// --------------------------------------------------
// Description: Vulkan Slang implementation
// Copyright (c) 2026 Max Van den Eynde
//

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>
#ifdef VULKAN
#include "diagnostics.h"
#include "vulkan_state.h"
#include <slang-com-ptr.h>
#include <slang.h>
#include <spirv_cross/spirv_cross.hpp>

namespace opal::vulkan {
SlangCompilerState::SlangCompilerState() {
    if (SLANG_FAILED(slang::createGlobalSession(globalSession.writeRef()))) {
        throw std::runtime_error("Failed to create Slang global session");
    }

    slang::TargetDesc targetDesc{};
    targetDesc.format = SLANG_SPIRV;

    targetDesc.profile = globalSession->findProfile("spirv_1_5");

    slang::SessionDesc sessionDesc{};
    sessionDesc.targets = &targetDesc;
    sessionDesc.targetCount = 1;

    if (SLANG_FAILED(
            globalSession->createSession(sessionDesc, session.writeRef()))) {
        throw std::runtime_error("Failed to create Slang session");
    }
}

SlangCompilerState &slangCompiler() {
    static SlangCompilerState compilerState;
    return compilerState;
}

std::vector<uint32_t> compileSlangToSPIRV(const std::string &source,
                                          ShaderType type,
                                          const std::string &entryPoint) {
    auto &compiler = slangCompiler();

    Slang::ComPtr<slang::IBlob> diagnostics;
    Slang::ComPtr<slang::IModule> module;

    module = compiler.session->loadModuleFromSourceString(
        "opal_shader", "opal_shader.slang", source.c_str(),
        diagnostics.writeRef());

    if (!module) {
        const char *message =
            static_cast<const char *>(diagnostics->getBufferPointer());
        opal::detail::log(LogLevel::Error, message);
        throw std::runtime_error("Failed to load Slang module: " +
                                 std::string(message));
    }

    if (diagnostics) {
        const char *message =
            static_cast<const char *>(diagnostics->getBufferPointer());
        opal::detail::log(LogLevel::Warning, message);
    }

    Slang::ComPtr<slang::IEntryPoint> entryPointObj;

    SlangResult result = module->findEntryPointByName(entryPoint.c_str(),
                                                      entryPointObj.writeRef());

    if (SLANG_FAILED(result)) {
        throw std::runtime_error("Slang entry point not found: " + entryPoint);
    }

    std::array<slang::IComponentType *, 2> components = {module.get(),
                                                         entryPointObj.get()};

    Slang::ComPtr<slang::IComponentType> program;

    result = compiler.session->createCompositeComponentType(
        components.data(), components.size(), program.writeRef(),
        diagnostics.writeRef());

    if (diagnostics) {
        detail::log(LogLevel::Info,
                    static_cast<const char *>(diagnostics->getBufferPointer()));
    }

    if (SLANG_FAILED(result)) {
        throw std::runtime_error("Failed to compose Slang shader");
    }

    Slang::ComPtr<slang::IComponentType> linkedProgram;

    result = program->link(linkedProgram.writeRef(), diagnostics.writeRef());

    if (SLANG_FAILED(result)) {
        throw std::runtime_error("Failed to link Slang shader");
    }

    Slang::ComPtr<slang::IBlob> spirv;

    result = linkedProgram->getEntryPointCode(0, // entry point
                                              0, // target: SPIR-V
                                              spirv.writeRef(),
                                              diagnostics.writeRef());

    if (SLANG_FAILED(result)) {
        throw std::runtime_error("Failed to generate SPIR-V");
    }

    const size_t byteSize = spirv->getBufferSize();

    if (byteSize % sizeof(uint32_t) != 0) {
        throw std::runtime_error("Invalid SPIR-V byte size");
    }

    std::vector<uint32_t> spirvData(byteSize / sizeof(uint32_t));

    std::memcpy(spirvData.data(), spirv->getBufferPointer(), byteSize);

    return spirvData;
}

std::vector<ShaderBinding> reflectShaderBindings(ShaderState &state,
                                                 ProgramState &programState) {
    if (state.spirv.empty()) {
        throw std::runtime_error(
            "Cannot reflect bindings from empty SPIR-V");
    }

    spirv_cross::Compiler compiler(state.spirv);
    spirv_cross::ShaderResources resources = compiler.get_shader_resources();
    std::vector<ShaderBinding> bindings;

    auto descriptorCount = [&](const spirv_cross::Resource &resource) {
        const auto &type = compiler.get_type(resource.type_id);
        uint64_t count = 1;
        for (size_t index = 0; index < type.array.size(); ++index) {
            uint64_t dimension = type.array[index];
            if (index < type.array_size_literal.size() &&
                !type.array_size_literal[index]) {
                dimension = compiler.get_constant(type.array[index]).scalar();
            }
            if (dimension == 0) {
                dimension = 1;
            }
            if (count > std::numeric_limits<uint32_t>::max() / dimension) {
                throw std::runtime_error(
                    "Vulkan descriptor array is too large");
            }
            count *= dimension;
        }
        return static_cast<uint32_t>(count);
    };

    auto resourceName = [&](const spirv_cross::Resource &resource) {
        std::string name = compiler.get_name(resource.id);
        if (name.empty()) {
            name = resource.name;
        }
        if (name.empty()) {
            name = compiler.get_name(resource.base_type_id);
        }
        if (name.empty()) {
            name = compiler.get_fallback_name(resource.id);
        }
        return name;
    };

    auto addResources = [&](const auto &reflectedResources,
                            ShaderResourceType type) {
        for (const auto &resource : reflectedResources) {
            if (!compiler.has_decoration(resource.id,
                                         spv::DecorationBinding)) {
                throw std::runtime_error(
                    "Reflected Vulkan resource has no binding: " +
                    resourceName(resource));
            }

            ShaderBinding binding{};
            binding.set = compiler.has_decoration(
                              resource.id, spv::DecorationDescriptorSet)
                              ? compiler.get_decoration(
                                    resource.id,
                                    spv::DecorationDescriptorSet)
                              : 0;
            binding.binding =
                compiler.get_decoration(resource.id, spv::DecorationBinding);
            binding.name = resourceName(resource);
            binding.type = type;
            binding.count = descriptorCount(resource);
            binding.stages = state.stage;

            auto existing = std::find_if(
                bindings.begin(), bindings.end(),
                [&](const ShaderBinding &other) {
                    return other.set == binding.set &&
                           other.binding == binding.binding;
                });
            if (existing == bindings.end()) {
                bindings.push_back(std::move(binding));
                continue;
            }
            if (existing->type != binding.type ||
                existing->count != binding.count) {
                throw std::runtime_error(
                    "Conflicting Vulkan resources use descriptor set " +
                    std::to_string(binding.set) + " binding " +
                    std::to_string(binding.binding));
            }
            if (existing->name.empty()) {
                existing->name = std::move(binding.name);
            }
        }
    };

    addResources(resources.uniform_buffers,
                 ShaderResourceType::UniformBuffer);
    addResources(resources.storage_buffers,
                 ShaderResourceType::StorageBuffer);
    addResources(resources.sampled_images,
                 ShaderResourceType::CombinedImageSampler);
    addResources(resources.separate_images, ShaderResourceType::SampledImage);
    addResources(resources.separate_samplers, ShaderResourceType::Sampler);
    addResources(resources.storage_images, ShaderResourceType::StorageImage);

    for (const auto &resource : resources.uniform_buffers) {
        uint32_t set = compiler.has_decoration(
                           resource.id, spv::DecorationDescriptorSet)
                           ? compiler.get_decoration(
                                 resource.id, spv::DecorationDescriptorSet)
                           : 0;
        uint32_t binding =
            compiler.get_decoration(resource.id, spv::DecorationBinding);
        const auto &blockType = compiler.get_type(resource.base_type_id);

        UniformBlockReflection block{};
        block.name = resourceName(resource);
        block.set = set;
        block.binding = binding;
        block.size = compiler.get_declared_struct_size(blockType);
        block.members.reserve(blockType.member_types.size());

        for (uint32_t index = 0; index < blockType.member_types.size();
             ++index) {
            UniformMember member{};
            member.name = compiler.get_member_name(resource.base_type_id,
                                                   index);
            if (member.name.empty()) {
                member.name = block.name + "." + std::to_string(index);
            }
            member.set = set;
            member.binding = binding;
            member.offset = compiler.type_struct_member_offset(blockType,
                                                               index);
            member.size = compiler.get_declared_struct_member_size(blockType,
                                                                   index);
            block.members.push_back(member);

            auto existingMember = programState.uniformsByName.find(member.name);
            if (existingMember == programState.uniformsByName.end()) {
                programState.uniformsByName.emplace(member.name, member);
            } else if (existingMember->second.set != member.set ||
                       existingMember->second.binding != member.binding ||
                       existingMember->second.offset != member.offset ||
                       existingMember->second.size != member.size) {
                throw std::runtime_error(
                    "Conflicting Vulkan uniforms use the name: " +
                    member.name);
            }
        }

        auto existingBlock = std::find_if(
            programState.uniformBlocks.begin(),
            programState.uniformBlocks.end(),
            [&](const UniformBlockReflection &other) {
                return other.set == block.set &&
                       other.binding == block.binding;
            });
        if (existingBlock == programState.uniformBlocks.end()) {
            programState.uniformBlocks.push_back(std::move(block));
            continue;
        }
        if (existingBlock->size != block.size ||
            existingBlock->members.size() != block.members.size()) {
            throw std::runtime_error(
                "Vulkan uniform block layout mismatch at descriptor set " +
                std::to_string(block.set) + " binding " +
                std::to_string(block.binding));
        }
        for (size_t index = 0; index < block.members.size(); ++index) {
            if (existingBlock->members[index].offset !=
                    block.members[index].offset ||
                existingBlock->members[index].size !=
                    block.members[index].size) {
                throw std::runtime_error(
                    "Vulkan uniform block layout mismatch at descriptor set " +
                    std::to_string(block.set) + " binding " +
                    std::to_string(block.binding));
            }
        }
    }

    if (!resources.subpass_inputs.empty()) {
        throw std::runtime_error(
            "Vulkan input-attachment reflection is not supported");
    }
    if (!resources.acceleration_structures.empty()) {
        throw std::runtime_error(
            "Vulkan acceleration-structure reflection is not supported");
    }
    if (!resources.atomic_counters.empty()) {
        throw std::runtime_error(
            "Vulkan atomic-counter reflection is not supported");
    }

    std::sort(bindings.begin(), bindings.end(),
              [](const ShaderBinding &left, const ShaderBinding &right) {
                  if (left.set != right.set) {
                      return left.set < right.set;
                  }
                  return left.binding < right.binding;
              });
    return bindings;
}

VkDescriptorType descriptorTypeToVk(ShaderResourceType type) {
    switch (type) {
    case ShaderResourceType::UniformBuffer:
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    case ShaderResourceType::StorageBuffer:
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

    case ShaderResourceType::CombinedImageSampler:
        return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    case ShaderResourceType::SampledImage:
        return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

    case ShaderResourceType::Sampler:
        return VK_DESCRIPTOR_TYPE_SAMPLER;

    case ShaderResourceType::StorageImage:
        return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    }

    throw std::runtime_error("Unsupported Vulkan shader resource type");
}

} // namespace opal::vulkan

#endif
