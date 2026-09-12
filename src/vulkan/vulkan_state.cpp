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

ContextState &contextState(Context *context) {
    return contextStatesStorage()[context];
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

} // namespace opal::vulkan

#endif