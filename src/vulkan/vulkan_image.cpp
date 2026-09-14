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

        VULKAN_GUARD(vkCreateImageView(deviceState.device, &viewInfo, nullptr,
                                       &contextState.swapchainImageViews[i]),
                     "Failed to create image views for swapchain images");
    }
}

} // namespace opal::vulkan

#endif