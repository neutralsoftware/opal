//
// vulkan_image.cpp
// As part of the Opal project
// Created by Max Van den Eynde in 2026
// --------------------------------------------------
// Description: Image support for Vulkan
// Copyright (c) 2026 Max Van den Eynde
//

#include <cstdint>
#include <vector>
#ifdef VULKAN
#include "diagnostics.h"
#include "opal/opal.h"
#include "vulkan_state.h"
#include <vulkan/vulkan.h>

namespace opal::vulkan {

void createSwapchain(ContextState &contextState, DeviceState &deviceState,
                     uint32_t width, uint32_t height) {
    VkSurfaceCapabilitiesKHR capabilities;
    VULKAN_GUARD(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                     deviceState.physicalDeviceInfo.device,
                     contextState.surface, &capabilities),
                 "Failed to get surface capabilities");

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(deviceState.physicalDeviceInfo.device,
                                         contextState.surface, &formatCount,
                                         nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(deviceState.physicalDeviceInfo.device,
                                         contextState.surface, &formatCount,
                                         formats.data());

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        deviceState.physicalDeviceInfo.device, contextState.surface,
        &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        deviceState.physicalDeviceInfo.device, contextState.surface,
        &presentModeCount, presentModes.data());

    VkSurfaceFormatKHR chosenFormat = formats[0];
    for (const auto &format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosenFormat = format;
            break;
        }
    }

    VkPresentModeKHR chosenPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (const auto &presentMode : presentModes) {
        if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            chosenPresentMode = presentMode;
            break;
        }
    }

    VkExtent2D chosenExtent = capabilities.currentExtent;
    if (capabilities.currentExtent.width == UINT32_MAX) {
        chosenExtent.width = std::clamp(static_cast<uint32_t>(width),
                                        capabilities.minImageExtent.width,
                                        capabilities.maxImageExtent.width);

        chosenExtent.height = std::clamp(static_cast<uint32_t>(height),
                                         capabilities.minImageExtent.height,
                                         capabilities.maxImageExtent.height);
    }

    uint32_t imageCount = capabilities.minImageCount + 1;

    if (capabilities.maxImageCount > 0 &&
        imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = contextState.surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = chosenFormat.format;
    createInfo.imageColorSpace = chosenFormat.colorSpace;
    createInfo.imageExtent = chosenExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndices[] = {
        deviceState.physicalDeviceInfo.queueFamilies.graphicsQueueFamilyIndex,
        deviceState.physicalDeviceInfo.queueFamilies.presentQueueFamilyIndex};
    if (deviceState.physicalDeviceInfo.queueFamilies.graphicsQueueFamilyIndex !=
        deviceState.physicalDeviceInfo.queueFamilies.presentQueueFamilyIndex) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = chosenPresentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VULKAN_GUARD(vkCreateSwapchainKHR(deviceState.device, &createInfo, nullptr,
                                      &contextState.swapchain),
                 "Failed to create swapchain");
}

void createSwapchainImages(ContextState &contextState,
                           DeviceState &deviceState) {
    uint32_t swapchainImageCount = 0;

    vkGetSwapchainImagesKHR(deviceState.device, contextState.swapchain,
                            &swapchainImageCount, nullptr);
    contextState.swapchainImages.resize(swapchainImageCount);

    vkGetSwapchainImagesKHR(deviceState.device, contextState.swapchain,
                            &swapchainImageCount,
                            contextState.swapchainImages.data());

    contextState.swapchainImageViews.resize(swapchainImageCount);
    contextState.swapchainImageLayouts.resize(swapchainImageCount,
                                              VK_IMAGE_LAYOUT_UNDEFINED);

    for (size_t i = 0; i < swapchainImageCount; i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = contextState.swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = contextState.swapchainImageFormat;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        contextState.swapchainImageLayouts[i] = VK_IMAGE_LAYOUT_UNDEFINED;

        VULKAN_GUARD(vkCreateImageView(deviceState.device, &viewInfo, nullptr,
                                       &contextState.swapchainImageViews[i]),
                     "Failed to create image views for swapchain images");
    }
}

VkFormat textureFormatToVkFormat(TextureFormat format) {
    switch (format) {
    case opal::TextureFormat::Red8:
        return VK_FORMAT_R8_UNORM;
    case opal::TextureFormat::Red16F:
        return VK_FORMAT_R16_SFLOAT;
    case opal::TextureFormat::Depth24Stencil8:
        return VK_FORMAT_D24_UNORM_S8_UINT;
    case opal::TextureFormat::Depth32F:
        return VK_FORMAT_D32_SFLOAT;
    case opal::TextureFormat::Rgba8:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case opal::TextureFormat::Rgba16F:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case opal::TextureFormat::Rgb8:
        return VK_FORMAT_R8G8B8_UNORM;
    case opal::TextureFormat::Rgb16F:
        return VK_FORMAT_R16G16B16_SFLOAT;
    case opal::TextureFormat::sRgb8:
        return VK_FORMAT_R8G8B8_SRGB;
    case opal::TextureFormat::sRgba8:
        return VK_FORMAT_R8G8B8A8_SRGB;
    case opal::TextureFormat::DepthComponent24:
        return VK_FORMAT_X8_D24_UNORM_PACK32;
    default:
        throw std::runtime_error("Unsupported texture format for Vulkan");
    }
}

VkImageType textureTypeToVk(TextureType type) {
    switch (type) {
    case TextureType::Texture2D:
        return VK_IMAGE_TYPE_2D;
    case TextureType::Texture2DArray:
        return VK_IMAGE_TYPE_2D;
    case TextureType::Texture2DMultisample:
        return VK_IMAGE_TYPE_2D;
    case TextureType::Texture3D:
        return VK_IMAGE_TYPE_3D;
    case TextureType::TextureCubeMap:
        return VK_IMAGE_TYPE_2D;
    default:
        throw std::runtime_error("Unsupported texture type for Vulkan");
    }
}

VkImageAspectFlags textureAspectFlagsFor(TextureFormat format) {
    switch (format) {
    case opal::TextureFormat::Depth24Stencil8:
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    case opal::TextureFormat::Depth32F:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    case opal::TextureFormat::DepthComponent24:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    default:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

VkImageUsageFlags textureUsageFlagsFor(TextureType type, TextureFormat format) {
    switch (format) {
    case opal::TextureFormat::Depth24Stencil8:
    case opal::TextureFormat::Depth32F:
    case opal::TextureFormat::DepthComponent24:
        return VK_IMAGE_USAGE_SAMPLED_BIT |
               VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
               VK_IMAGE_USAGE_TRANSFER_DST_BIT |
               VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    default: {
        VkImageUsageFlags flags =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        if (type != TextureType::Texture2DMultisample) {
            flags |= VK_IMAGE_USAGE_STORAGE_BIT;
        }
        return flags;
    }
    }
}

VkSampleCountFlagBits sampleCountFlagBitsFor(int samples) {
    switch (samples) {
    case 1:
        return VK_SAMPLE_COUNT_1_BIT;
    case 2:
        return VK_SAMPLE_COUNT_2_BIT;
    case 4:
        return VK_SAMPLE_COUNT_4_BIT;
    case 8:
        return VK_SAMPLE_COUNT_8_BIT;
    case 16:
        return VK_SAMPLE_COUNT_16_BIT;
    case 32:
        return VK_SAMPLE_COUNT_32_BIT;
    case 64:
        return VK_SAMPLE_COUNT_64_BIT;
    default:
        throw std::runtime_error("Unsupported sample count for Vulkan");
    }
}

RenderTargetSignature
getRenderTargetSignature(const std::shared_ptr<Framebuffer> &framebuffer,
                         Device *device) {
    if (framebuffer == nullptr || device == nullptr) {
        throw std::runtime_error(
            "Cannot determine Vulkan render target without framebuffer/device");
    }

    RenderTargetSignature signature{};

    if (framebuffer->isDefaultFramebuffer) {
        auto &context = vulkan::contextState(device->context.get());

        signature.colorFormats.push_back(context.swapchainImageFormat);

        signature.depthFormat = VK_FORMAT_UNDEFINED;
        signature.stencilFormat = VK_FORMAT_UNDEFINED;
        signature.samples = VK_SAMPLE_COUNT_1_BIT;

        return signature;
    }

    const int drawLimit = framebuffer->getDrawBufferCount();

    int colorIndex = 0;

    for (const auto &attachment : framebuffer->attachments) {
        if (attachment.texture == nullptr) {
            continue;
        }

        auto &texture = vulkan::textureState(attachment.texture.get());

        switch (attachment.type) {
        case Attachment::Type::Color:
            if (!framebuffer->colorBufferDisabled &&
                (drawLimit < 0 || colorIndex < drawLimit)) {
                signature.colorFormats.push_back(texture.format);
            }

            ++colorIndex;

            signature.samples =
                std::max(signature.samples, texture.sampleCount);

            break;

        case Attachment::Type::Depth:
            signature.depthFormat = texture.format;
            signature.samples =
                std::max(signature.samples, texture.sampleCount);
            break;

        case Attachment::Type::Stencil:
            signature.stencilFormat = texture.format;
            signature.samples =
                std::max(signature.samples, texture.sampleCount);
            break;

        case Attachment::Type::DepthStencil:
            signature.depthFormat = texture.format;
            signature.stencilFormat = texture.format;
            signature.samples =
                std::max(signature.samples, texture.sampleCount);
            break;

        default:
            break;
        }
    }

    return signature;
}

VkExtent2D getRenderExtent(const std::shared_ptr<Framebuffer> &framebuffer,
                           Device *device) {
    if (framebuffer->isDefaultFramebuffer) {
        return vulkan::contextState(device->context.get()).swapchainExtent;
    }

    return {static_cast<uint32_t>(std::max(framebuffer->width, 1)),
            static_cast<uint32_t>(std::max(framebuffer->height, 1))};
}

void bindVulkanDrawingState(CommandBuffer *commandBuffer,
                            const std::shared_ptr<DrawingState> &drawingState,
                            const std::shared_ptr<Pipeline> &pipeline) {

    if (drawingState == nullptr) {
        return;
    }

    auto &cmd = vulkan::commandBufferState(commandBuffer);

    if (drawingState->vertexBuffer != nullptr) {
        auto &vertex = vulkan::bufferState(drawingState->vertexBuffer.get());
        if (vertex.buffer == VK_NULL_HANDLE) {
            throw std::runtime_error("Vulkan vertex buffer is not initialized");
        }

        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd.commandBuffer, 0, 1, &vertex.buffer,
                               &offset);
    }

    auto &pipelineState = vulkan::pipelineState(pipeline.get());
    const bool expectsInstanceBuffer = pipelineState.vertexBindings.size() > 1;

    if (drawingState->instanceBuffer != nullptr) {
        auto &instance =
            vulkan::bufferState(drawingState->instanceBuffer.get());
        if (instance.buffer == VK_NULL_HANDLE) {
            throw std::runtime_error(
                "Vulkan instance buffer is not initialized");
        }

        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd.commandBuffer, 1, 1, &instance.buffer,
                               &offset);

    } else if (expectsInstanceBuffer) {
        throw std::runtime_error("Pipeline expects instance vertex data, "
                                 "but DrawingState has no instance buffer");
    }
}

} // namespace opal::vulkan

#endif
