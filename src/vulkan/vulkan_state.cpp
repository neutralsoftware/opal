//
// vulkan_state.cpp
// As part of the Opal project
// Created by Max Van den Eynde in 2026
// --------------------------------------------------
// Description: State for vulkan
// Copyright (c) 2026 Max Van den Eynde
//

#include "diagnostics.h"
#ifdef VULKAN

#include "opal/opal.h"
#include "vulkan_state.h"
#include <unordered_map>

namespace opal::vulkan {

std::unordered_map<Context *, ContextState> &contextStatesStorage() {
    static auto *states = new std::unordered_map<Context *, ContextState>();
    return *states;
}

std::unordered_map<Device *, DeviceState> &deviceStatesStorage() {
    static auto *states = new std::unordered_map<Device *, DeviceState>();
    return *states;
}

std::unordered_map<CommandBuffer *, CommandBufferState> &
commandStatesStorage() {
    static auto *states =
        new std::unordered_map<CommandBuffer *, CommandBufferState>();
    return *states;
}

std::unordered_map<Texture *, TextureState> &textureStatesStorage() {
    static auto *states = new std::unordered_map<Texture *, TextureState>();
    return *states;
}

std::unordered_map<uint32_t, std::weak_ptr<Texture>> &textureHandlesStorage() {
    static auto *textures =
        new std::unordered_map<uint32_t, std::weak_ptr<Texture>>();
    return *textures;
}

ContextState &contextState(Context *context) {
    return contextStatesStorage()[context];
}

DeviceState &deviceState(Device *device) {
    return deviceStatesStorage()[device];
}

CommandBufferState &commandBufferState(CommandBuffer *commandBuffer) {
    return commandStatesStorage()[commandBuffer];
}

FramebufferState &framebufferState(Framebuffer *framebuffer) {
    static auto *states =
        new std::unordered_map<Framebuffer *, FramebufferState>();
    return (*states)[framebuffer];
}

TextureState &textureState(Texture *texture) {
    return textureStatesStorage()[texture];
}

ShaderState &shaderState(Shader *shader) {
    static auto *states = new std::unordered_map<Shader *, ShaderState>();
    return (*states)[shader];
}

ProgramState &programState(ShaderProgram *program) {
    static auto *states =
        new std::unordered_map<ShaderProgram *, ProgramState>();
    return (*states)[program];
}

PipelineState &pipelineState(Pipeline *pipeline) {
    static auto *states = new std::unordered_map<Pipeline *, PipelineState>();
    return (*states)[pipeline];
}

uint32_t registerTextureHandle(const std::shared_ptr<Texture> &texture) {
    static uint32_t nextHandle = 1;
    auto &handles = textureHandlesStorage();
    uint32_t handle = 0;
    do {
        handle = nextHandle++;
    } while (handle == 0 || handles.contains(handle));
    handles[handle] = texture;
    textureState(texture.get()).handle = handle;
    return handle;
}

std::shared_ptr<Texture> getTextureFromHandle(uint32_t handle) {
    auto &textures = textureHandlesStorage();
    auto iterator = textures.find(handle);
    if (iterator == textures.end()) {
        return nullptr;
    }
    auto texture = iterator->second.lock();
    if (texture == nullptr) {
        textures.erase(iterator);
    }
    return texture;
}

void releaseContextState(Context *context) {
    if (context == nullptr) {
        return;
    }
    auto &states = contextStatesStorage();
    auto it = states.find(context);
    if (it == states.end()) {
        return;
    }
    vkDestroyInstance(it->second.instance, nullptr);
    it->second.instance = VK_NULL_HANDLE;
    states.erase(it);
}

void releaseDeviceState(Device *device) {
    if (device == nullptr) {
        return;
    }
    auto &states = deviceStatesStorage();
    auto it = states.find(device);
    if (it == states.end()) {
        return;
    }
    if (it->second.device != VK_NULL_HANDLE) {
        vkDestroyDevice(it->second.device, nullptr);
        it->second.device = VK_NULL_HANDLE;
    }
    states.erase(it);
}

void releaseCommandBufferState(CommandBuffer *commandBuffer) {
    if (commandBuffer == nullptr) {
        return;
    }
    auto &states = commandStatesStorage();
    auto it = states.find(commandBuffer);
    if (it == states.end()) {
        return;
    }
    if (it->second.commandBuffer != VK_NULL_HANDLE) {
        it->second.commandBuffer = VK_NULL_HANDLE;
    }
    states.erase(it);
}

void releaseFramebufferState(Framebuffer *framebuffer) {
    if (framebuffer == nullptr) {
        return;
    }
    auto &states = framebufferState(framebuffer);
    states.dirty = false;
}

void releaseTextureState(Texture *texture) {
    if (texture == nullptr) {
        return;
    }
    auto &states = textureStatesStorage();
    auto iterator = states.find(texture);
    if (iterator == states.end()) {
        return;
    }
    VkDevice device = VK_NULL_HANDLE;
    if (Device::globalInstance != nullptr) {
        device = deviceState(Device::globalInstance).device;
    }
    auto &state = iterator->second;
    if (state.handle != 0) {
        textureHandlesStorage().erase(state.handle);
    }
    if (device != VK_NULL_HANDLE) {
        if (state.imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(device, state.imageView, nullptr);
        }
        if (state.sampler != VK_NULL_HANDLE) {
            vkDestroySampler(device, state.sampler, nullptr);
        }
        if (state.image != VK_NULL_HANDLE && state.ownsImage) {
            vkDestroyImage(device, state.image, nullptr);
        }
        if (state.memory != VK_NULL_HANDLE && state.ownsImage) {
            vkFreeMemory(device, state.memory, nullptr);
        }
    }
    states.erase(iterator);
}

void releaseShaderState(Shader *shader) {
    if (shader == nullptr) {
        return;
    }
    VkDevice device = VK_NULL_HANDLE;
    if (Device::globalInstance != nullptr) {
        device = deviceState(Device::globalInstance).device;
    }
    auto &state = shaderState(shader);
    if (device != VK_NULL_HANDLE && state.shaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, state.shaderModule, nullptr);
        state.shaderModule = VK_NULL_HANDLE;
    }
}

void releaseProgramState(ShaderProgram *program) {
    if (program == nullptr) {
        return;
    }
    VkDevice device = VK_NULL_HANDLE;
    if (Device::globalInstance != nullptr) {
        device = deviceState(Device::globalInstance).device;
    }
    auto &state = programState(program);
    if (device != VK_NULL_HANDLE) {
        if (state.pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, state.pipelineLayout, nullptr);
            state.pipelineLayout = VK_NULL_HANDLE;
        }
    }
}

void releasePipelineState(Pipeline *pipeline) {
    if (pipeline == nullptr) {
        return;
    }
    VkDevice device = VK_NULL_HANDLE;
    if (Device::globalInstance != nullptr) {
        device = deviceState(Device::globalInstance).device;
    }
    auto &state = pipelineState(pipeline);
    if (device != VK_NULL_HANDLE) {
        for (auto &pair : state.graphicsPipelines) {
            if (pair.second != VK_NULL_HANDLE) {
                vkDestroyPipeline(device, pair.second, nullptr);
            }
        }
        state.graphicsPipelines.clear();
        if (state.computePipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, state.computePipeline, nullptr);
            state.computePipeline = VK_NULL_HANDLE;
        }
    }
}

VkCommandBuffer beginSingleTimeCommands(DeviceState &deviceState) {
    if (deviceState.device == VK_NULL_HANDLE ||
        deviceState.graphicsPool == VK_NULL_HANDLE) {
        throw std::runtime_error("Vulkan graphics command pool is unavailable");
    }

    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = deviceState.graphicsPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VULKAN_GUARD(vkAllocateCommandBuffers(deviceState.device, &allocateInfo,
                                          &commandBuffer),
                 "Failed to allocate one-time Vulkan command buffer");

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VkResult result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (result != VK_SUCCESS) {
        vkFreeCommandBuffers(deviceState.device, deviceState.graphicsPool, 1,
                             &commandBuffer);
        VULKAN_GUARD(result, "Failed to begin one-time Vulkan command buffer");
    }

    return commandBuffer;
}

void endSingleTimeCommands(DeviceState &deviceState,
                           VkCommandBuffer commandBuffer) {
    if (commandBuffer == VK_NULL_HANDLE) {
        return;
    }

    VkResult result = vkEndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS) {
        vkFreeCommandBuffers(deviceState.device, deviceState.graphicsPool, 1,
                             &commandBuffer);
        VULKAN_GUARD(result, "Failed to end one-time Vulkan command buffer");
    }

    VkCommandBufferSubmitInfo commandInfo{};
    commandInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    commandInfo.commandBuffer = commandBuffer;

    VkSubmitInfo2 submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &commandInfo;

    result = vkQueueSubmit2(deviceState.graphicsQueue, 1, &submitInfo,
                            VK_NULL_HANDLE);
    if (result == VK_SUCCESS) {
        result = vkQueueWaitIdle(deviceState.graphicsQueue);
    }

    vkFreeCommandBuffers(deviceState.device, deviceState.graphicsPool, 1,
                         &commandBuffer);
    VULKAN_GUARD(result, "Failed to submit one-time Vulkan command buffer");
}

bool checkValidationLayerSupport() {
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const auto &layer : availableLayers) {
        if (std::strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
            return true;
        }
    }

    return false;
}

VKAPI_ATTR VkBool32 VKAPI_CALL vulkanDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT *callbackData, void *userData) {
    LogLevel level = LogLevel::Info;

    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        level = LogLevel::Error;
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        level = LogLevel::Warning;
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
        level = LogLevel::Info;

    detail::log(level, callbackData->pMessage);

    return VK_FALSE;
}

void configureDebugMessenger(VkDebugUtilsMessengerCreateInfoEXT &info) {
    info = {};

    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

    info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

    info.pfnUserCallback = vulkanDebugCallback;
    info.pUserData = nullptr;
}

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter,
                        VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memoryProperties{};

    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {

        const bool supported = (typeFilter & (1u << i)) != 0;

        const bool hasProperties =
            (memoryProperties.memoryTypes[i].propertyFlags & properties) ==
            properties;

        if (supported && hasProperties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable Vulkan memory type");
}

size_t bytesPerPixel(TextureFormat format) {
    switch (format) {
    case TextureFormat::Rgba8:
    case TextureFormat::sRgba8:
        return 4;

    case TextureFormat::Rgb8:
    case TextureFormat::sRgb8:
        return 3;

    case TextureFormat::Rgba16F:
        return 8;

    case TextureFormat::Rgb16F:
        return 6;

    case TextureFormat::Red8:
        return 1;

    case TextureFormat::Red16F:
        return 2;

    case TextureFormat::Depth24Stencil8:
        return 4;

    case TextureFormat::Depth32F:
        return 4;

    case TextureFormat::DepthComponent24:
        return 4;
    }

    throw std::runtime_error("Unsupported texture format");
}

VkShaderStageFlagBits shaderTypeToVk(ShaderType type) {
    switch (type) {
    case ShaderType::Vertex:
        return VK_SHADER_STAGE_VERTEX_BIT;
    case ShaderType::Fragment:
        return VK_SHADER_STAGE_FRAGMENT_BIT;
    case ShaderType::Geometry:
        return VK_SHADER_STAGE_GEOMETRY_BIT;
    case ShaderType::TessellationControl:
        return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    case ShaderType::TessellationEvaluation:
        return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    case ShaderType::Compute:
        return VK_SHADER_STAGE_COMPUTE_BIT;
    default:
        throw std::runtime_error("Unsupported shader type for Vulkan");
    }
}

} // namespace opal::vulkan

#endif
