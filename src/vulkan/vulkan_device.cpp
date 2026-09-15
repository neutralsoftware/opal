//
// vulkan_device.cpp
// As part of the Opal project
// Created by Max Van den Eynde in 2026
// --------------------------------------------------
// Description: Vulkan device settings
// Copyright (c) 2026 Max Van den Eynde
//

#include <cstdint>
#include <set>
#include <tuple>
#include <unordered_map>
#include <vector>
#ifdef VULKAN

#include "diagnostics.h"
#include "opal/opal.h"
#include "vulkan_state.h"
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

namespace opal::vulkan {

PhysicalDeviceInfo pickPhysicalDevice(VkInstance instance,
                                      VkSurfaceKHR surface) {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    std::unordered_map<VkPhysicalDevice, uint32_t> deviceScores;
    std::vector<PhysicalDeviceInfo> suitableDevices;

    for (const auto &device : devices) {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);

        VkPhysicalDeviceVulkan13Features features13{};
        features13.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

        VkPhysicalDeviceFeatures2 features{};
        features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features.pNext = &features13;
        vkGetPhysicalDeviceFeatures2(device, &features);

        DeviceQueueFamilies queueFamilies = findQueueFamilies(device, surface);
        if (!queueFamilies.isComplete()) {
            continue;
        }

        uint32_t score = 0;
        if (deviceProperties.deviceType ==
            VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 10000;
        }

        if (features.features.samplerAnisotropy) {
            score += 150;
        }
        if (features.features.tessellationShader) {
            score += 150;
        }
        if (features.features.multiDrawIndirect) {
            score += 300;
        }
        if (features.features.fragmentStoresAndAtomics) {
            score += 150;
        }
        if (features.features.vertexPipelineStoresAndAtomics) {
            score += 100;
        }

        if (features13.dynamicRendering) {
            score += 1000;
        }

        if (features13.synchronization2) {
            score += 1000;
        }

        if (features13.subgroupSizeControl) {
            score += 250;
        }

        if (supportsRayTracing(device)) {
            score += 5000;
        }

        PhysicalDeviceInfo info{};
        info.device = device;
        info.features = features;
        info.features13 = features13;
        info.features.pNext = nullptr;
        info.queueFamilies = queueFamilies;
        info.properties = deviceProperties;
        deviceScores[info.device] = score;
        suitableDevices.push_back(info);
    }

    std::tuple<PhysicalDeviceInfo, uint32_t> bestDevice{};
    for (int i = 0; i < suitableDevices.size(); ++i) {
        const auto &deviceInfo = suitableDevices[i];
        uint32_t score = deviceScores[deviceInfo.device];

        if (std::get<1>(bestDevice) < score) {
            bestDevice = std::make_tuple(deviceInfo, score);
        }
    }

    PhysicalDeviceInfo &bestDeviceInfo = std::get<0>(bestDevice);

    return bestDeviceInfo;
}

DeviceQueueFamilies findQueueFamilies(VkPhysicalDevice device,
                                      VkSurfaceKHR surface) {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                             nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);

    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                             queueFamilies.data());

    DeviceQueueFamilies result;

    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        VkBool32 presentSupport = VK_FALSE;

        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface,
                                             &presentSupport);

        const bool graphicsSupport =
            queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT;

        const bool computeSupport =
            queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT;

        if (graphicsSupport && result.graphicsQueueFamilyIndex == UINT32_MAX) {
            result.graphicsQueueFamilyIndex = i;
        }

        if (presentSupport && result.presentQueueFamilyIndex == UINT32_MAX) {
            result.presentQueueFamilyIndex = i;
        }

        if (computeSupport && !graphicsSupport) {
            result.computeQueueFamilyIndex = i;
        }
    }

    if (result.computeQueueFamilyIndex == UINT32_MAX) {
        for (uint32_t i = 0; i < queueFamilyCount; ++i) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                result.computeQueueFamilyIndex = i;
                break;
            }
        }
    }

    return result;
}

PhysicalDeviceInfo buildQueuesAndPhysicalDevice(VkInstance instance,
                                                VkSurfaceKHR surface) {
    PhysicalDeviceInfo physicalDeviceInfo =
        pickPhysicalDevice(instance, surface);

    if (physicalDeviceInfo.device == VK_NULL_HANDLE) {
        throw std::runtime_error(
            "Failed to find a suitable Vulkan physical device");
    }

    return physicalDeviceInfo;
}

bool supportsRayTracing(VkPhysicalDevice device) {
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                         nullptr);

    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                         extensions.data());

    bool hasAccelerationStructure = false;
    bool hasRayTracingPipeline = false;

    for (const auto &extension : extensions) {
        if (strcmp(extension.extensionName,
                   VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) == 0) {
            hasAccelerationStructure = true;
        }

        if (strcmp(extension.extensionName,
                   VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) == 0) {
            hasRayTracingPipeline = true;
        }
    }

    if (!hasAccelerationStructure || !hasRayTracingPipeline)
        return false;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingFeatures{};
    rayTracingFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rayTracingFeatures.pNext = nullptr;
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelStructFeatures{};
    accelStructFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    accelStructFeatures.pNext = &rayTracingFeatures;

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &accelStructFeatures;
    vkGetPhysicalDeviceFeatures2(device, &features2);

    if (accelStructFeatures.accelerationStructure &&
        rayTracingFeatures.rayTracingPipeline) {
        return true;
    }
    return false;
}

VkDevice createLogicalDevice(const PhysicalDeviceInfo &physicalDeviceInfo) {
    float queuePriority = 1.0f;

    std::set<uint32_t> uniqueQueueFamilies = {
        physicalDeviceInfo.queueFamilies.graphicsQueueFamilyIndex,
        physicalDeviceInfo.queueFamilies.computeQueueFamilyIndex,
        physicalDeviceInfo.queueFamilies.presentQueueFamilyIndex};

    if (physicalDeviceInfo.queueFamilies.graphicsQueueFamilyIndex ==
        UINT32_MAX) {
        throw std::runtime_error(
            "No suitable graphics queue family found for the selected device");
    }
    if (physicalDeviceInfo.queueFamilies.computeQueueFamilyIndex ==
        UINT32_MAX) {
        throw std::runtime_error(
            "No suitable compute queue family found for the selected device");
    }
    if (physicalDeviceInfo.queueFamilies.presentQueueFamilyIndex ==
        UINT32_MAX) {
        throw std::runtime_error(
            "No suitable present queue family found for the selected device");
    }

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    std::vector<const char *> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    if (supportsRayTracing(physicalDeviceInfo.device)) {
        deviceExtensions.push_back(
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        deviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
    }

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingFeatures{};
    rayTracingFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationFeatures{};
    accelerationFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;

    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering =
        physicalDeviceInfo.features13.dynamicRendering;
    features13.synchronization2 =
        physicalDeviceInfo.features13.synchronization2;

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

    features2.features.samplerAnisotropy =
        physicalDeviceInfo.features.features.samplerAnisotropy;

    bool rayTracing = supportsRayTracing(physicalDeviceInfo.device);

    if (rayTracing) {
        accelerationFeatures.accelerationStructure = VK_TRUE;
        rayTracingFeatures.rayTracingPipeline = VK_TRUE;

        features13.pNext = &accelerationFeatures;
        accelerationFeatures.pNext = &rayTracingFeatures;
    }

    features2.pNext = &features13;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &features2;
    createInfo.flags = 0;
    createInfo.queueCreateInfoCount =
        static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.enabledExtensionCount =
        static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();
    createInfo.pEnabledFeatures = nullptr;

    VkDevice device;
    VULKAN_GUARD(vkCreateDevice(physicalDeviceInfo.device, &createInfo, nullptr,
                                &device),
                 "Failed to create Vulkan logical device");
    return device;
}

void createQueues(DeviceState &deviceState) {
    vkGetDeviceQueue(
        deviceState.device,
        deviceState.physicalDeviceInfo.queueFamilies.graphicsQueueFamilyIndex,
        0, &deviceState.graphicsQueue);

    vkGetDeviceQueue(
        deviceState.device,
        deviceState.physicalDeviceInfo.queueFamilies.computeQueueFamilyIndex, 0,
        &deviceState.computeQueue);

    vkGetDeviceQueue(
        deviceState.device,
        deviceState.physicalDeviceInfo.queueFamilies.presentQueueFamilyIndex, 0,
        &deviceState.presentQueue);
}

void createPools(DeviceState &deviceState) {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    poolInfo.queueFamilyIndex =
        deviceState.physicalDeviceInfo.queueFamilies.graphicsQueueFamilyIndex;
    VULKAN_GUARD(vkCreateCommandPool(deviceState.device, &poolInfo, nullptr,
                                     &deviceState.graphicsPool),
                 "Failed to create graphics command pool");

    poolInfo.queueFamilyIndex =
        deviceState.physicalDeviceInfo.queueFamilies.computeQueueFamilyIndex;
    VULKAN_GUARD(vkCreateCommandPool(deviceState.device, &poolInfo, nullptr,
                                     &deviceState.computePool),
                 "Failed to create compute command pool");
}

VkBufferUsageFlags bufferUsageToVk(BufferUsage usage) {
    switch (usage) {
    case BufferUsage::VertexBuffer:
        return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
               VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
               VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    case BufferUsage::IndexArray:
        return VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
               VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
               VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    case BufferUsage::UniformBuffer:
        return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
               VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    case BufferUsage::ShaderRead:
        return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
               VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    case BufferUsage::ShaderReadWrite:
        return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
               VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
               VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    case BufferUsage::GeneralPurpose:
        return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
               VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
               VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    default:
        throw std::runtime_error("Unsupported Vulkan buffer usage");
    }
}

void createBuffer(DeviceState &deviceState, VkDeviceSize size,
                  VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                  VkBuffer &buffer, VkDeviceMemory &memory) {
    VkBufferCreateInfo bufferInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                  .pNext = nullptr,
                                  .flags = 0,
                                  .size = size,
                                  .usage = usage,
                                  .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                  .queueFamilyIndexCount = 0,
                                  .pQueueFamilyIndices = nullptr};

    VULKAN_GUARD(
        vkCreateBuffer(deviceState.device, &bufferInfo, nullptr, &buffer),
        "Failed to create Vulkan buffer");

    VkMemoryRequirements memoryRequirements{};
    vkGetBufferMemoryRequirements(deviceState.device, buffer,
                                  &memoryRequirements);

    VkMemoryAllocateInfo allocationInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memoryRequirements.size,
        .memoryTypeIndex =
            findMemoryType(deviceState.physicalDeviceInfo.device,
                           memoryRequirements.memoryTypeBits, properties)};

    VkResult result =
        vkAllocateMemory(deviceState.device, &allocationInfo, nullptr, &memory);

    if (result != VK_SUCCESS) {
        vkDestroyBuffer(deviceState.device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;

        throw std::runtime_error("Failed to allocate Vulkan buffer memory");
    }

    VULKAN_GUARD(vkBindBufferMemory(deviceState.device, buffer, memory, 0),
                 "Failed to bind Vulkan buffer memory");
}

} // namespace opal::vulkan

#endif