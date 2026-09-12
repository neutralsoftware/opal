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
            detail::log(LogLevel::Error, (message));                           \
            throw std::runtime_error(message);                                 \
        }                                                                      \
    } while (false)

namespace opal::vulkan {
struct ContextState {
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
};

ContextState &contextState(Context *context);
void releaseContextState(Context *context);

bool checkValidationLayerSupport();
VKAPI_ATTR VkBool32 VKAPI_CALL vulkanDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT *callbackData, void *userData);

void configureDebugMessenger(VkDebugUtilsMessengerCreateInfoEXT &info);
} // namespace opal::vulkan

#endif

#endif