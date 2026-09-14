//
// vulkan_state.h
// As part of the Opal project
// Created by Max Van den Eynde in 2026
// --------------------------------------------------
// Description: Vulkan backend state management for Opal rendering
// Copyright (c) 2026 Max Van den Eynde
//

#ifndef VULKAN_STATE_H
#define VULKAN_STATE_H

#ifdef VULKAN

#include "opal/opal.h"
#include <vulkan/vulkan.h>

#define VULKAN_GUARD(call, message)                                            \
    do {                                                                       \
        VkResult result = (call);                                              \
        if (result != VK_SUCCESS) {                                            \
            opal::detail::log(LogLevel::Error, (message));                     \
            throw std::runtime_error(message);                                 \
        }                                                                      \
    } while (false)

namespace opal::vulkan {
struct ContextState {
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
};

struct DeviceQueueFamilies {
    uint32_t graphicsQueueFamilyIndex = UINT32_MAX;
    uint32_t computeQueueFamilyIndex = UINT32_MAX;
    uint32_t presentQueueFamilyIndex = UINT32_MAX;

    inline bool isComplete() const {
        return graphicsQueueFamilyIndex != UINT32_MAX &&
               computeQueueFamilyIndex != UINT32_MAX &&
               presentQueueFamilyIndex != UINT32_MAX;
    }
};

struct PhysicalDeviceInfo {
    VkPhysicalDevice device = VK_NULL_HANDLE;

    VkPhysicalDeviceProperties properties{};
    VkPhysicalDeviceFeatures2 features{};
    VkPhysicalDeviceVulkan13Features features13{};

    DeviceQueueFamilies queueFamilies{};
};

struct DeviceState {
    PhysicalDeviceInfo physicalDeviceInfo{};
    VkDevice device = VK_NULL_HANDLE;

    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue computeQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;

    VkCommandPool graphicsPool = VK_NULL_HANDLE;
    VkCommandPool computePool = VK_NULL_HANDLE;
    VkCommandPool presentPool = VK_NULL_HANDLE;
};

ContextState &contextState(Context *context);
DeviceState &deviceState(Device *device);

void releaseContextState(Context *context);
void releaseDeviceState(Device *device);

bool checkValidationLayerSupport();
VKAPI_ATTR VkBool32 VKAPI_CALL vulkanDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT *callbackData, void *userData);

void configureDebugMessenger(VkDebugUtilsMessengerCreateInfoEXT &info);

bool supportsRayTracing(VkPhysicalDevice device);

PhysicalDeviceInfo buildQueuesAndPhysicalDevice(VkInstance instance,
                                                VkSurfaceKHR surface);
VkDevice createLogicalDevice(const PhysicalDeviceInfo &physicalDeviceInfo);
DeviceQueueFamilies findQueueFamilies(VkPhysicalDevice device,
                                      VkSurfaceKHR surface);
void createQueues(DeviceState &deviceState);
void createPools(DeviceState &deviceState);
} // namespace opal::vulkan

#endif

#endif