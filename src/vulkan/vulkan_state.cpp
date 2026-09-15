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

std::unordered_map<Framebuffer *, FramebufferState> &
framebufferStatesStorage() {
    static auto *states =
        new std::unordered_map<Framebuffer *, FramebufferState>();
    return *states;
}

std::unordered_map<Shader *, ShaderState> &shaderStatesStorage() {
    static auto *states = new std::unordered_map<Shader *, ShaderState>();
    return *states;
}

std::unordered_map<ShaderProgram *, ProgramState> &programStatesStorage() {
    static auto *states =
        new std::unordered_map<ShaderProgram *, ProgramState>();
    return *states;
}

std::unordered_map<Pipeline *, PipelineState> &pipelineStatesStorage() {
    static auto *states = new std::unordered_map<Pipeline *, PipelineState>();
    return *states;
}

std::unordered_map<Buffer *, BufferState> &bufferStatesStorage() {
    static auto *states = new std::unordered_map<Buffer *, BufferState>();
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
    return framebufferStatesStorage()[framebuffer];
}

TextureState &textureState(Texture *texture) {
    return textureStatesStorage()[texture];
}

ShaderState &shaderState(Shader *shader) {
    return shaderStatesStorage()[shader];
}

ProgramState &programState(ShaderProgram *program) {
    return programStatesStorage()[program];
}

PipelineState &pipelineState(Pipeline *pipeline) {
    return pipelineStatesStorage()[pipeline];
}

BufferState &bufferState(Buffer *buffer) {
    return bufferStatesStorage()[buffer];
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
    auto &state = it->second;
    if (state.surface != VK_NULL_HANDLE && state.instance != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(state.instance, state.surface, nullptr);
        state.surface = VK_NULL_HANDLE;
    }
    if (state.debugMessenger != VK_NULL_HANDLE &&
        state.instance != VK_NULL_HANDLE) {
        auto destroyDebugMessenger =
            reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(state.instance,
                                      "vkDestroyDebugUtilsMessengerEXT"));
        if (destroyDebugMessenger != nullptr) {
            destroyDebugMessenger(state.instance, state.debugMessenger,
                                  nullptr);
        }
        state.debugMessenger = VK_NULL_HANDLE;
    }
    if (state.instance != VK_NULL_HANDLE) {
        vkDestroyInstance(state.instance, nullptr);
        state.instance = VK_NULL_HANDLE;
    }
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
    auto &state = it->second;
    if (state.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(state.device);

        for (auto &[pipeline, pipelineState] : pipelineStatesStorage()) {
            for (auto &[signature, handle] : pipelineState.graphicsPipelines) {
                if (handle != VK_NULL_HANDLE) {
                    vkDestroyPipeline(state.device, handle, nullptr);
                }
            }
            if (pipelineState.computePipeline != VK_NULL_HANDLE) {
                vkDestroyPipeline(state.device, pipelineState.computePipeline,
                                  nullptr);
            }
            if (pipelineState.descriptorPool != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(state.device,
                                        pipelineState.descriptorPool, nullptr);
            }
        }
        pipelineStatesStorage().clear();

        for (auto &[program, programState] : programStatesStorage()) {
            if (programState.pipelineLayout != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(state.device,
                                        programState.pipelineLayout, nullptr);
            }
            for (VkDescriptorSetLayout layout :
                 programState.descriptorSetLayouts) {
                if (layout != VK_NULL_HANDLE) {
                    vkDestroyDescriptorSetLayout(state.device, layout, nullptr);
                }
            }
        }
        programStatesStorage().clear();

        for (auto &[shader, shaderState] : shaderStatesStorage()) {
            if (shaderState.shaderModule != VK_NULL_HANDLE) {
                vkDestroyShaderModule(state.device, shaderState.shaderModule,
                                      nullptr);
            }
        }
        shaderStatesStorage().clear();

        for (auto &[texture, textureState] : textureStatesStorage()) {
            if (textureState.imageView != VK_NULL_HANDLE) {
                vkDestroyImageView(state.device, textureState.imageView,
                                   nullptr);
            }
            if (textureState.sampler != VK_NULL_HANDLE) {
                vkDestroySampler(state.device, textureState.sampler, nullptr);
            }
            if (textureState.image != VK_NULL_HANDLE &&
                textureState.ownsImage) {
                vkDestroyImage(state.device, textureState.image, nullptr);
            }
            if (textureState.memory != VK_NULL_HANDLE &&
                textureState.ownsImage) {
                vkFreeMemory(state.device, textureState.memory, nullptr);
            }
        }
        textureStatesStorage().clear();
        textureHandlesStorage().clear();

        for (auto &[buffer, bufferState] : bufferStatesStorage()) {
            if (bufferState.mapped != nullptr) {
                vkUnmapMemory(state.device, bufferState.memory);
            }
            if (bufferState.buffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(state.device, bufferState.buffer, nullptr);
            }
            if (bufferState.memory != VK_NULL_HANDLE) {
                vkFreeMemory(state.device, bufferState.memory, nullptr);
            }
        }
        bufferStatesStorage().clear();

        for (auto &[commandBuffer, commandState] : commandStatesStorage()) {
            if (commandState.imageAvailableSemaphore != VK_NULL_HANDLE) {
                vkDestroySemaphore(state.device,
                                   commandState.imageAvailableSemaphore,
                                   nullptr);
            }
            if (commandState.renderFinishedSemaphore != VK_NULL_HANDLE) {
                vkDestroySemaphore(state.device,
                                   commandState.renderFinishedSemaphore,
                                   nullptr);
            }
            if (commandState.inFlightFence != VK_NULL_HANDLE) {
                vkDestroyFence(state.device, commandState.inFlightFence,
                               nullptr);
            }
            if (commandState.commandBuffer != VK_NULL_HANDLE &&
                commandState.commandPool != VK_NULL_HANDLE) {
                vkFreeCommandBuffers(state.device, commandState.commandPool, 1,
                                     &commandState.commandBuffer);
            }
        }
        commandStatesStorage().clear();

        if (device->context != nullptr) {
            destroySwapchain(contextState(device->context.get()), state);
        }
        if (state.computePool != VK_NULL_HANDLE &&
            state.computePool != state.graphicsPool) {
            vkDestroyCommandPool(state.device, state.computePool, nullptr);
        }
        if (state.graphicsPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(state.device, state.graphicsPool, nullptr);
        }
        vkDestroyDevice(state.device, nullptr);
        state.device = VK_NULL_HANDLE;
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
    auto &state = it->second;
    if (state.device != VK_NULL_HANDLE) {
        if (state.inFlightFence != VK_NULL_HANDLE && state.submitted) {
            vkWaitForFences(state.device, 1, &state.inFlightFence, VK_TRUE,
                            UINT64_MAX);
        }
        if (state.imageAvailableSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(state.device, state.imageAvailableSemaphore,
                               nullptr);
        }
        if (state.renderFinishedSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(state.device, state.renderFinishedSemaphore,
                               nullptr);
        }
        if (state.inFlightFence != VK_NULL_HANDLE) {
            vkDestroyFence(state.device, state.inFlightFence, nullptr);
        }
        if (state.commandBuffer != VK_NULL_HANDLE &&
            state.commandPool != VK_NULL_HANDLE) {
            vkFreeCommandBuffers(state.device, state.commandPool, 1,
                                 &state.commandBuffer);
        }
    }
    states.erase(it);
}

void releaseFramebufferState(Framebuffer *framebuffer) {
    if (framebuffer == nullptr) {
        return;
    }
    framebufferStatesStorage().erase(framebuffer);
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
    auto &states = shaderStatesStorage();
    auto iterator = states.find(shader);
    if (iterator == states.end()) {
        return;
    }
    if (device != VK_NULL_HANDLE &&
        iterator->second.shaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, iterator->second.shaderModule, nullptr);
    }
    states.erase(iterator);
}

void releaseProgramState(ShaderProgram *program) {
    if (program == nullptr) {
        return;
    }
    VkDevice device = VK_NULL_HANDLE;
    if (Device::globalInstance != nullptr) {
        device = deviceState(Device::globalInstance).device;
    }
    auto &states = programStatesStorage();
    auto iterator = states.find(program);
    if (iterator == states.end()) {
        return;
    }
    auto &state = iterator->second;
    if (device != VK_NULL_HANDLE) {
        if (state.pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, state.pipelineLayout, nullptr);
            state.pipelineLayout = VK_NULL_HANDLE;
        }
        for (VkDescriptorSetLayout layout : state.descriptorSetLayouts) {
            if (layout != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(device, layout, nullptr);
            }
        }
    }
    states.erase(iterator);
}

void releasePipelineState(Pipeline *pipeline) {
    if (pipeline == nullptr) {
        return;
    }
    VkDevice device = VK_NULL_HANDLE;
    if (Device::globalInstance != nullptr) {
        device = deviceState(Device::globalInstance).device;
    }
    auto &states = pipelineStatesStorage();
    auto iterator = states.find(pipeline);
    if (iterator == states.end()) {
        return;
    }
    auto &state = iterator->second;
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
        if (state.descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, state.descriptorPool, nullptr);
        }
    }
    states.erase(iterator);
}

void releaseBufferState(Buffer *buffer) {
    if (buffer == nullptr) {
        return;
    }
    VkDevice device = VK_NULL_HANDLE;
    if (Device::globalInstance != nullptr) {
        device = deviceState(Device::globalInstance).device;
    }
    auto &states = bufferStatesStorage();
    auto iterator = states.find(buffer);
    if (iterator == states.end()) {
        return;
    }
    auto &state = iterator->second;
    if (device != VK_NULL_HANDLE) {
        if (state.mapped != nullptr) {
            vkUnmapMemory(device, state.memory);
            state.mapped = nullptr;
        }
        if (state.buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, state.buffer, nullptr);
            state.buffer = VK_NULL_HANDLE;
        }
        if (state.memory != VK_NULL_HANDLE) {
            vkFreeMemory(device, state.memory, nullptr);
            state.memory = VK_NULL_HANDLE;
        }
    }
    states.erase(iterator);
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

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    const char *failure = "Failed to create one-time Vulkan fence";
    result = vkCreateFence(deviceState.device, &fenceInfo, nullptr, &fence);
    if (result == VK_SUCCESS) {
        failure = "Failed to submit one-time Vulkan command buffer";
        result =
            vkQueueSubmit2(deviceState.graphicsQueue, 1, &submitInfo, fence);
    }
    if (result == VK_SUCCESS) {
        failure = "Failed waiting for one-time Vulkan command buffer";
        result =
            vkWaitForFences(deviceState.device, 1, &fence, VK_TRUE, UINT64_MAX);
    }

    if (fence != VK_NULL_HANDLE) {
        vkDestroyFence(deviceState.device, fence, nullptr);
    }
    vkFreeCommandBuffers(deviceState.device, deviceState.graphicsPool, 1,
                         &commandBuffer);
    VULKAN_GUARD(result, failure);
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

void transitionTexture(VkCommandBuffer commandBuffer, TextureState &state,
                       VkImageLayout newLayout) {
    if (commandBuffer == VK_NULL_HANDLE || state.image == VK_NULL_HANDLE ||
        state.layout == newLayout) {
        return;
    }

    auto layoutState = [](VkImageLayout layout, VkPipelineStageFlags2 &stage,
                          VkAccessFlags2 &access) {
        switch (layout) {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            stage = VK_PIPELINE_STAGE_2_NONE;
            access = VK_ACCESS_2_NONE;
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            access = VK_ACCESS_2_TRANSFER_READ_BIT;
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            access = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            access = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
                     VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
        case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL:
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            stage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                    VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
            access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                     VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_GENERAL:
            stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            access = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
            break;
        default:
            stage = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            access = VK_ACCESS_2_SHADER_READ_BIT;
            break;
        }
    };

    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    layoutState(state.layout, barrier.srcStageMask, barrier.srcAccessMask);
    layoutState(newLayout, barrier.dstStageMask, barrier.dstAccessMask);
    barrier.oldLayout = state.layout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = state.image;
    barrier.subresourceRange.aspectMask = state.aspectMask;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = state.mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = state.arrayLayers;

    VkDependencyInfo dependency{};
    dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency.imageMemoryBarrierCount = 1;
    dependency.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(commandBuffer, &dependency);
    state.layout = newLayout;
}

size_t bytesPerPixel(TextureFormat format) {
    switch (format) {
    case TextureFormat::Rgba8:
    case TextureFormat::sRgba8:
        return 4;

    case TextureFormat::Rgb8:
    case TextureFormat::sRgb8:
        return 4;

    case TextureFormat::Rgba16F:
        return 8;

    case TextureFormat::Rgb16F:
        return 8;

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
