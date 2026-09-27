/*
 ray_tracing.cpp
 As part of the Atlas project
 Created by Max Van den Eynde in 2026
 --------------------------------------------------
 Description: Ray tracing APIs for using with the renderer
 Copyright (c) 2026 Max Van den Eynde
*/

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <opal/opal.h>
#ifdef METAL

#include "Metal/Metal.hpp"
#include "metal_state.h"

opal::PrimitiveAccelerationStructure::~PrimitiveAccelerationStructure() {
    if (blasDescriptor != nullptr) {
        blasDescriptor->release();
    }
    if (blas != nullptr) {
        blas->release();
    }
}

std::shared_ptr<opal::PrimitiveAccelerationStructure>
opal::PrimitiveAccelerationStructure::create(
    const std::vector<PrimitiveVertex> &vertices,
    const std::vector<uint32_t> &indices) {
    std::vector<float> positions;
    positions.reserve(vertices.size() * 3);
    for (const auto &vertex : vertices) {
        positions.push_back(vertex.position[0]);
        positions.push_back(vertex.position[1]);
        positions.push_back(vertex.position[2]);
    }
    return create(positions, indices);
}

std::shared_ptr<opal::PrimitiveAccelerationStructure>
opal::PrimitiveAccelerationStructure::create(
    const std::vector<float> &positions, const std::vector<uint32_t> &indices) {
    return create(std::vector<std::vector<float>>{positions},
                  std::vector<std::vector<uint32_t>>{indices});
}

std::shared_ptr<opal::PrimitiveAccelerationStructure>
opal::PrimitiveAccelerationStructure::create(
    const std::vector<std::vector<float>> &positions,
    const std::vector<std::vector<uint32_t>> &indices) {
    if (positions.empty() || positions.size() != indices.size()) {
        return nullptr;
    }
    auto blas = std::make_shared<PrimitiveAccelerationStructure>();

    auto &deviceState = metal::deviceState(Device::globalInstance);

    std::vector<MTL::AccelerationStructureGeometryDescriptor *> descriptors;
    descriptors.reserve(positions.size());
    blas->vertexBuffers.reserve(positions.size());
    blas->indexBuffers.reserve(indices.size());
    for (size_t geometryIndex = 0; geometryIndex < positions.size();
         ++geometryIndex) {
        const auto &geometryPositions = positions[geometryIndex];
        const auto &geometryIndices = indices[geometryIndex];
        if (geometryPositions.size() < 9 || geometryPositions.size() % 3 != 0 ||
            geometryIndices.size() < 3 || geometryIndices.size() % 3 != 0) {
            return nullptr;
        }

        auto vertexBuffer = std::shared_ptr<MTL::Buffer>(
            deviceState.device->newBuffer(geometryPositions.data(),
                                          geometryPositions.size() *
                                              sizeof(float),
                                          MTL::ResourceStorageModeShared),
            [](MTL::Buffer *buffer) {
                if (buffer != nullptr) {
                    buffer->release();
                }
            });
        auto indexBuffer = std::shared_ptr<MTL::Buffer>(
            deviceState.device->newBuffer(geometryIndices.data(),
                                          geometryIndices.size() *
                                              sizeof(uint32_t),
                                          MTL::ResourceStorageModeShared),
            [](MTL::Buffer *buffer) {
                if (buffer != nullptr) {
                    buffer->release();
                }
            });
        if (vertexBuffer == nullptr || indexBuffer == nullptr) {
            return nullptr;
        }

        auto *triangleDescriptor =
            MTL::AccelerationStructureTriangleGeometryDescriptor::descriptor();
        triangleDescriptor->setVertexBuffer(vertexBuffer.get());
        triangleDescriptor->setVertexStride(sizeof(float) * 3);
        triangleDescriptor->setVertexFormat(MTL::AttributeFormatFloat3);
        triangleDescriptor->setVertexBufferOffset(0);
        triangleDescriptor->setIndexBuffer(indexBuffer.get());
        triangleDescriptor->setIndexType(MTL::IndexType::IndexTypeUInt32);
        triangleDescriptor->setTriangleCount(geometryIndices.size() / 3);
        triangleDescriptor->setOpaque(true);
        descriptors.push_back(triangleDescriptor);
        blas->vertexBuffers.push_back(std::move(vertexBuffer));
        blas->indexBuffers.push_back(std::move(indexBuffer));
    }

    auto *blasDesc =
        MTL::PrimitiveAccelerationStructureDescriptor::descriptor()->retain();
    NS::Array *geoms =
        NS::Array::array((NS::Object **)descriptors.data(), descriptors.size());
    blasDesc->setGeometryDescriptors(geoms);
    blasDesc->setUsage(MTL::AccelerationStructureUsagePreferFastIntersection);

    MTL::AccelerationStructureSizes sizes =
        deviceState.device->accelerationStructureSizes(blasDesc);

    try {
        blas->scratch = Buffer::create(BufferUsage::GeneralPurpose,
                                       sizes.buildScratchBufferSize);
    } catch (const std::runtime_error &) {
        blasDesc->release();
        return nullptr;
    }

    MTL::AccelerationStructure *blasPtr =
        deviceState.device->newAccelerationStructure(
            sizes.accelerationStructureSize);

    if (blasPtr == nullptr) {
        blasDesc->release();
        return nullptr;
    }

    blas->blas = blasPtr;
    blas->blasDescriptor = blasDesc;

    return blas;
}

void opal::CommandBuffer::buildPrimitiveAccelerationStructure(
    const std::shared_ptr<PrimitiveAccelerationStructure> &blas) {
    auto &deviceState = metal::deviceState(Device::globalInstance);
    auto &scratchBuffer = metal::bufferState(blas->scratch.get());
    auto &state = metal::commandBufferState(this);
    if (state.encoder != nullptr) {
        state.encoder->endEncoding();
        state.encoder = nullptr;
        state.textureBindingsInitialized = false;
    }
    if (state.commandBuffer == nullptr) {
        state.commandBuffer = deviceState.queue->commandBuffer();
    }
    if (state.computeEncoder != nullptr) {
        state.computeEncoder->endEncoding();
        state.computeEncoder = nullptr;
    }
    auto *cs = state.commandBuffer;

    auto *asEnc = cs->accelerationStructureCommandEncoder();
    asEnc->buildAccelerationStructure(blas->blas, blas->blasDescriptor,
                                      scratchBuffer.buffer, 0);
    asEnc->endEncoding();

    blas->isBuilt = true;
    state.pendingResources.emplace_back(blas->scratch);
    for (const auto &vertexBuffer : blas->vertexBuffers) {
        state.pendingResources.emplace_back(vertexBuffer);
    }
    for (const auto &indexBuffer : blas->indexBuffers) {
        state.pendingResources.emplace_back(indexBuffer);
    }
    blas->scratch.reset();
    blas->vertexBuffers.clear();
    blas->indexBuffers.clear();
}

std::shared_ptr<opal::InstanceAccelerationStructure>
opal::CommandBuffer::buildAccelerationStructures(
    const std::vector<std::shared_ptr<PrimitiveAccelerationStructure>> &blases,
    const std::vector<AccelerationStructureInstance> &instances) {
    if (blases.empty() || instances.empty()) {
        return nullptr;
    }

    auto &deviceState = metal::deviceState(Device::globalInstance);
    auto &state = metal::commandBufferState(this);
    if (state.encoder != nullptr) {
        state.encoder->endEncoding();
        state.encoder = nullptr;
        state.textureBindingsInitialized = false;
    }
    if (state.commandBuffer == nullptr) {
        state.commandBuffer = deviceState.queue->commandBuffer();
    }
    if (state.computeEncoder != nullptr) {
        state.computeEncoder->endEncoding();
        state.computeEncoder = nullptr;
    }

    auto *asEnc = state.commandBuffer->accelerationStructureCommandEncoder();
    for (const auto &blas : blases) {
        if (blas == nullptr || blas->scratch == nullptr ||
            blas->vertexBuffers.empty() || blas->indexBuffers.empty()) {
            asEnc->endEncoding();
            return nullptr;
        }
        auto &scratchBuffer = metal::bufferState(blas->scratch.get());
        asEnc->buildAccelerationStructure(blas->blas, blas->blasDescriptor,
                                          scratchBuffer.buffer, 0);
        blas->isBuilt = true;
        state.pendingResources.emplace_back(blas->scratch);
        for (const auto &vertexBuffer : blas->vertexBuffers) {
            state.pendingResources.emplace_back(vertexBuffer);
        }
        for (const auto &indexBuffer : blas->indexBuffers) {
            state.pendingResources.emplace_back(indexBuffer);
        }
        blas->scratch.reset();
        blas->vertexBuffers.clear();
        blas->indexBuffers.clear();
    }

    std::shared_ptr<InstanceAccelerationStructure> tlas;
    try {
        tlas = InstanceAccelerationStructure::create(instances);
    } catch (...) {
        asEnc->endEncoding();
        throw;
    }
    if (tlas == nullptr) {
        asEnc->endEncoding();
        return nullptr;
    }

    auto &scratchState = metal::bufferState(tlas->scratch.get());
    asEnc->buildAccelerationStructure(tlas->tlas, tlas->tlasDescriptor,
                                      scratchState.buffer, 0);
    asEnc->endEncoding();

    tlas->isBuilt = true;
    state.pendingResources.emplace_back(tlas->scratch);
    state.pendingResources.emplace_back(tlas->instanceBuffer);
    tlas->scratch.reset();
    tlas->instanceBuffer.reset();
    return tlas;
}

static inline void opal::writeMetalTransform3x4(const glm::mat4 &M,
                                                float out[12]) {
    out[0] = M[0][0];
    out[1] = M[0][1];
    out[2] = M[0][2];
    out[3] = M[1][0];
    out[4] = M[1][1];
    out[5] = M[1][2];
    out[6] = M[2][0];
    out[7] = M[2][1];
    out[8] = M[2][2];
    out[9] = M[3][0];
    out[10] = M[3][1];
    out[11] = M[3][2];
}

opal::InstanceAccelerationStructure::~InstanceAccelerationStructure() {
    if (tlasDescriptor != nullptr) {
        tlasDescriptor->release();
    }
    if (tlas != nullptr) {
        tlas->release();
    }
}

std::shared_ptr<opal::InstanceAccelerationStructure>
opal::InstanceAccelerationStructure::create(
    const std::vector<opal::AccelerationStructureInstance> &instances) {
    if (instances.empty()) {
        return nullptr;
    }
    auto tlas = std::make_shared<opal::InstanceAccelerationStructure>();
    tlas->instances = instances;

    tlas->blasRefs.reserve(instances.size());
    tlas->blasPtrs.reserve(instances.size());

    std::vector<MTL::AccelerationStructureInstanceDescriptor> descs;
    descs.resize(instances.size());

    for (size_t i = 0; i < instances.size(); ++i) {
        const auto &inst = instances[i];

        if (!inst.blas || !inst.blas->isBuilt)
            throw std::runtime_error("All BLAS must be built before TLAS.");

        tlas->blasRefs.push_back(inst.blas);
        tlas->blasPtrs.push_back(inst.blas->blas);

        auto &d = descs[i];
        memset(&d, 0, sizeof(d));

        float t[12];
        writeMetalTransform3x4(inst.transform, t);

        memcpy(&d.transformationMatrix, t, sizeof(float) * 12);

        d.accelerationStructureIndex = (uint32_t)i;
        d.mask = inst.mask ? inst.mask : 0xFF;
        d.options = MTL::AccelerationStructureInstanceOptionOpaque;
        if (inst.cullDisable) {
            d.options |=
                MTL::AccelerationStructureInstanceOptionDisableTriangleCulling;
        }
        d.intersectionFunctionTableOffset = 0;
    }

    tlas->instanceBuffer =
        Buffer::create(BufferUsage::GeneralPurpose,
                       descs.size() * sizeof(descs[0]), descs.data());
    if (tlas->instanceBuffer == nullptr) {
        return nullptr;
    }

    tlas->tlasDescriptor =
        MTL::InstanceAccelerationStructureDescriptor::descriptor()->retain();
    tlas->tlasDescriptor->setUsage(
        MTL::AccelerationStructureUsagePreferFastIntersection);
    tlas->tlasDescriptor->setInstanceDescriptorType(
        MTL::AccelerationStructureInstanceDescriptorTypeDefault);
    auto &ib = metal::bufferState(tlas->instanceBuffer.get());

    tlas->tlasDescriptor->setInstanceDescriptorBuffer(ib.buffer);
    tlas->tlasDescriptor->setInstanceDescriptorStride(
        sizeof(MTL::AccelerationStructureInstanceDescriptor));
    tlas->tlasDescriptor->setInstanceCount((NS::UInteger)descs.size());

    NS::Array *instancedAS =
        NS::Array::array((NS::Object **)tlas->blasPtrs.data(),
                         (NS::UInteger)tlas->blasPtrs.size());
    tlas->tlasDescriptor->setInstancedAccelerationStructures(instancedAS);

    metal::DeviceState &deviceState =
        metal::deviceState(Device::globalInstance);

    MTL::AccelerationStructureSizes sizes =
        deviceState.device->accelerationStructureSizes(tlas->tlasDescriptor);

    tlas->scratch = Buffer::create(BufferUsage::GeneralPurpose,
                                   sizes.buildScratchBufferSize);

    MTL::AccelerationStructure *tlasPtr =
        deviceState.device->newAccelerationStructure(
            sizes.accelerationStructureSize);

    if (tlasPtr == nullptr) {
        return nullptr;
    }

    tlas->tlas = tlasPtr;

    return tlas;
}

void opal::CommandBuffer::buildInstanceAccelerationStructure(
    const std::shared_ptr<InstanceAccelerationStructure> &tlas) {
    auto &deviceState = metal::deviceState(Device::globalInstance);
    auto &state = metal::commandBufferState(this);

    if (state.encoder != nullptr) {
        state.encoder->endEncoding();
        state.encoder = nullptr;
        state.textureBindingsInitialized = false;
    }
    if (state.commandBuffer == nullptr) {
        state.commandBuffer = deviceState.queue->commandBuffer();
    }
    if (state.computeEncoder != nullptr) {
        state.computeEncoder->endEncoding();
        state.computeEncoder = nullptr;
    }

    auto *cs = state.commandBuffer;
    auto &scratchState = metal::bufferState(tlas->scratch.get());

    auto *asEnc = cs->accelerationStructureCommandEncoder();
    asEnc->buildAccelerationStructure(tlas->tlas, tlas->tlasDescriptor,
                                      scratchState.buffer, 0);
    asEnc->endEncoding();

    tlas->isBuilt = true;
    state.pendingResources.emplace_back(tlas->scratch);
    state.pendingResources.emplace_back(tlas->instanceBuffer);
    tlas->scratch.reset();
    tlas->instanceBuffer.reset();
}

#endif

#ifdef VULKAN

namespace {

void createRayBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                     VkMemoryPropertyFlags properties, VkBuffer &buffer,
                     VkDeviceMemory &memory, const void *data = nullptr) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(opal::Device::globalDevice, &bufferInfo, nullptr,
                       &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan ray tracing buffer");
    }
    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(opal::Device::globalDevice, buffer,
                                  &requirements);
    VkMemoryAllocateFlagsInfo flags{};
    flags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    VkMemoryAllocateInfo allocation{};
    allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation.pNext = &flags;
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex =
        opal::Device::globalInstance->findMemoryType(
            requirements.memoryTypeBits, properties);
    if (vkAllocateMemory(opal::Device::globalDevice, &allocation, nullptr,
                         &memory) != VK_SUCCESS) {
        vkDestroyBuffer(opal::Device::globalDevice, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        throw std::runtime_error("Failed to allocate Vulkan ray tracing memory");
    }
    if (vkBindBufferMemory(opal::Device::globalDevice, buffer, memory, 0) !=
        VK_SUCCESS) {
        vkDestroyBuffer(opal::Device::globalDevice, buffer, nullptr);
        vkFreeMemory(opal::Device::globalDevice, memory, nullptr);
        buffer = VK_NULL_HANDLE;
        memory = VK_NULL_HANDLE;
        throw std::runtime_error("Failed to bind Vulkan ray tracing memory");
    }
    if (data != nullptr) {
        void *mapped = nullptr;
        if (vkMapMemory(opal::Device::globalDevice, memory, 0, size, 0,
                        &mapped) != VK_SUCCESS) {
            throw std::runtime_error("Failed to map Vulkan ray tracing memory");
        }
        std::memcpy(mapped, data, static_cast<size_t>(size));
        vkUnmapMemory(opal::Device::globalDevice, memory);
    }
}

VkDeviceAddress rayBufferAddress(VkBuffer buffer) {
    VkBufferDeviceAddressInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    info.buffer = buffer;
    return vkGetBufferDeviceAddress(opal::Device::globalDevice, &info);
}

void destroyRayBuffer(VkBuffer &buffer, VkDeviceMemory &memory) {
    if (buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(opal::Device::globalDevice, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
    }
    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(opal::Device::globalDevice, memory, nullptr);
        memory = VK_NULL_HANDLE;
    }
}

}

opal::PrimitiveAccelerationStructure::~PrimitiveAccelerationStructure() {
    if (blas != VK_NULL_HANDLE) {
        vkDestroyAccelerationStructureKHR(Device::globalDevice, blas, nullptr);
    }
    destroyRayBuffer(accelerationBuffer, accelerationMemory);
    destroyRayBuffer(scratchBuffer, scratchMemory);
    for (size_t index = 0; index < vertexBuffers.size(); ++index) {
        destroyRayBuffer(vertexBuffers[index], vertexMemories[index]);
    }
    for (size_t index = 0; index < indexBuffers.size(); ++index) {
        destroyRayBuffer(indexBuffers[index], indexMemories[index]);
    }
}

std::shared_ptr<opal::PrimitiveAccelerationStructure>
opal::PrimitiveAccelerationStructure::create(
    const std::vector<PrimitiveVertex> &vertices,
    const std::vector<uint32_t> &indices) {
    std::vector<float> positions;
    positions.reserve(vertices.size() * 3);
    for (const auto &vertex : vertices) {
        positions.push_back(vertex.position[0]);
        positions.push_back(vertex.position[1]);
        positions.push_back(vertex.position[2]);
    }
    return create(positions, indices);
}

std::shared_ptr<opal::PrimitiveAccelerationStructure>
opal::PrimitiveAccelerationStructure::create(
    const std::vector<float> &positions,
    const std::vector<uint32_t> &indices) {
    return create(std::vector<std::vector<float>>{positions},
                  std::vector<std::vector<uint32_t>>{indices});
}

std::shared_ptr<opal::PrimitiveAccelerationStructure>
opal::PrimitiveAccelerationStructure::create(
    const std::vector<std::vector<float>> &positions,
    const std::vector<std::vector<uint32_t>> &indices) {
    if (positions.empty() || positions.size() != indices.size()) {
        return nullptr;
    }
    auto result = std::make_shared<PrimitiveAccelerationStructure>();
    result->vertexBuffers.resize(positions.size(), VK_NULL_HANDLE);
    result->vertexMemories.resize(positions.size(), VK_NULL_HANDLE);
    result->indexBuffers.resize(indices.size(), VK_NULL_HANDLE);
    result->indexMemories.resize(indices.size(), VK_NULL_HANDLE);
    result->geometries.resize(positions.size());
    result->ranges.resize(positions.size());
    std::vector<uint32_t> primitiveCounts(positions.size());

    for (size_t geometryIndex = 0; geometryIndex < positions.size();
         ++geometryIndex) {
        const auto &geometryPositions = positions[geometryIndex];
        const auto &geometryIndices = indices[geometryIndex];
        if (geometryPositions.size() < 9 || geometryPositions.size() % 3 != 0 ||
            geometryIndices.size() < 3 || geometryIndices.size() % 3 != 0) {
            return nullptr;
        }
        createRayBuffer(
            geometryPositions.size() * sizeof(float),
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            result->vertexBuffers[geometryIndex],
            result->vertexMemories[geometryIndex], geometryPositions.data());
        createRayBuffer(
            geometryIndices.size() * sizeof(uint32_t),
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            result->indexBuffers[geometryIndex],
            result->indexMemories[geometryIndex], geometryIndices.data());

        auto &geometry = result->geometries[geometryIndex];
        geometry.sType =
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        geometry.geometry.triangles.sType =
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        geometry.geometry.triangles.vertexData.deviceAddress =
            rayBufferAddress(result->vertexBuffers[geometryIndex]);
        geometry.geometry.triangles.vertexStride = sizeof(float) * 3;
        geometry.geometry.triangles.maxVertex =
            static_cast<uint32_t>(geometryPositions.size() / 3 - 1);
        geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
        geometry.geometry.triangles.indexData.deviceAddress =
            rayBufferAddress(result->indexBuffers[geometryIndex]);
        primitiveCounts[geometryIndex] =
            static_cast<uint32_t>(geometryIndices.size() / 3);
        result->ranges[geometryIndex].primitiveCount =
            primitiveCounts[geometryIndex];
    }

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags =
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.geometryCount = static_cast<uint32_t>(result->geometries.size());
    buildInfo.pGeometries = result->geometries.data();
    VkAccelerationStructureBuildSizesInfoKHR sizes{};
    sizes.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    vkGetAccelerationStructureBuildSizesKHR(
        Device::globalDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, primitiveCounts.data(), &sizes);
    createRayBuffer(
        sizes.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, result->accelerationBuffer,
        result->accelerationMemory);
    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = result->accelerationBuffer;
    createInfo.size = sizes.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    if (vkCreateAccelerationStructureKHR(Device::globalDevice, &createInfo,
                                         nullptr, &result->blas) != VK_SUCCESS) {
        return nullptr;
    }
    createRayBuffer(sizes.buildScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    result->scratchBuffer, result->scratchMemory);
    return result;
}

void opal::CommandBuffer::buildPrimitiveAccelerationStructure(
    const std::shared_ptr<PrimitiveAccelerationStructure> &blas) {
    if (blas == nullptr || blas->blas == VK_NULL_HANDLE) {
        throw std::runtime_error("Vulkan BLAS is unavailable");
    }
    beginCommandBufferIfNeeded();
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags =
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.dstAccelerationStructure = blas->blas;
    buildInfo.geometryCount = static_cast<uint32_t>(blas->geometries.size());
    buildInfo.pGeometries = blas->geometries.data();
    buildInfo.scratchData.deviceAddress = rayBufferAddress(blas->scratchBuffer);
    std::vector<const VkAccelerationStructureBuildRangeInfoKHR *> ranges;
    ranges.reserve(blas->ranges.size());
    for (const auto &range : blas->ranges) {
        ranges.push_back(&range);
    }
    vkCmdBuildAccelerationStructuresKHR(commandBuffers[currentFrame], 1,
                                        &buildInfo, ranges.data());
    blas->isBuilt = true;
}

opal::InstanceAccelerationStructure::~InstanceAccelerationStructure() {
    if (tlas != VK_NULL_HANDLE) {
        vkDestroyAccelerationStructureKHR(Device::globalDevice, tlas, nullptr);
    }
    destroyRayBuffer(accelerationBuffer, accelerationMemory);
    destroyRayBuffer(scratchBuffer, scratchMemory);
    destroyRayBuffer(instanceVkBuffer, instanceVkMemory);
}

std::shared_ptr<opal::InstanceAccelerationStructure>
opal::InstanceAccelerationStructure::create(
    const std::vector<opal::AccelerationStructureInstance> &instances) {
    if (instances.empty()) {
        return nullptr;
    }
    auto result = std::make_shared<InstanceAccelerationStructure>();
    result->instances = instances;
    result->blasRefs.reserve(instances.size());
    std::vector<VkAccelerationStructureInstanceKHR> descriptors(instances.size());
    for (size_t index = 0; index < instances.size(); ++index) {
        const auto &instance = instances[index];
        if (instance.blas == nullptr || !instance.blas->isBuilt) {
            throw std::runtime_error("All BLAS must be built before TLAS");
        }
        result->blasRefs.push_back(instance.blas);
        VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
        addressInfo.sType =
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        addressInfo.accelerationStructure = instance.blas->blas;
        auto &descriptor = descriptors[index];
        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 4; ++column) {
                descriptor.transform.matrix[row][column] =
                    instance.transform[column][row];
            }
        }
        descriptor.instanceCustomIndex = instance.instanceId;
        descriptor.mask = instance.mask == 0 ? 0xFF : instance.mask;
        descriptor.instanceShaderBindingTableRecordOffset = 0;
        descriptor.flags = instance.cullDisable
                               ? VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR
                               : 0;
        descriptor.accelerationStructureReference =
            vkGetAccelerationStructureDeviceAddressKHR(Device::globalDevice,
                                                        &addressInfo);
    }
    createRayBuffer(
        descriptors.size() * sizeof(descriptors[0]),
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        result->instanceVkBuffer, result->instanceVkMemory,
        descriptors.data());
    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.geometry.instances.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    geometry.geometry.instances.data.deviceAddress =
        rayBufferAddress(result->instanceVkBuffer);
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.flags =
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;
    uint32_t primitiveCount = static_cast<uint32_t>(descriptors.size());
    VkAccelerationStructureBuildSizesInfoKHR sizes{};
    sizes.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    vkGetAccelerationStructureBuildSizesKHR(
        Device::globalDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, &primitiveCount, &sizes);
    createRayBuffer(
        sizes.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, result->accelerationBuffer,
        result->accelerationMemory);
    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = result->accelerationBuffer;
    createInfo.size = sizes.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    if (vkCreateAccelerationStructureKHR(Device::globalDevice, &createInfo,
                                         nullptr, &result->tlas) != VK_SUCCESS) {
        return nullptr;
    }
    createRayBuffer(sizes.buildScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    result->scratchBuffer, result->scratchMemory);
    return result;
}

void opal::CommandBuffer::buildInstanceAccelerationStructure(
    const std::shared_ptr<InstanceAccelerationStructure> &tlas) {
    if (tlas == nullptr || tlas->tlas == VK_NULL_HANDLE) {
        throw std::runtime_error("Vulkan TLAS is unavailable");
    }
    beginCommandBufferIfNeeded();
    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.geometry.instances.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    geometry.geometry.instances.data.deviceAddress =
        rayBufferAddress(tlas->instanceVkBuffer);
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.flags =
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.dstAccelerationStructure = tlas->tlas;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;
    buildInfo.scratchData.deviceAddress = rayBufferAddress(tlas->scratchBuffer);
    VkAccelerationStructureBuildRangeInfoKHR range{};
    range.primitiveCount = static_cast<uint32_t>(tlas->instances.size());
    const VkAccelerationStructureBuildRangeInfoKHR *rangePointer = &range;
    vkCmdBuildAccelerationStructuresKHR(commandBuffers[currentFrame], 1,
                                        &buildInfo, &rangePointer);
    tlas->isBuilt = true;
}

std::shared_ptr<opal::InstanceAccelerationStructure>
opal::CommandBuffer::buildAccelerationStructures(
    const std::vector<std::shared_ptr<PrimitiveAccelerationStructure>> &blases,
    const std::vector<AccelerationStructureInstance> &instances) {
    if (blases.empty() || instances.empty()) {
        return nullptr;
    }
    for (const auto &blas : blases) {
        buildPrimitiveAccelerationStructure(blas);
    }
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    vkCmdPipelineBarrier(
        commandBuffers[currentFrame],
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1,
        &barrier, 0, nullptr, 0, nullptr);
    auto tlas = InstanceAccelerationStructure::create(instances);
    buildInstanceAccelerationStructure(tlas);
    barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    vkCmdPipelineBarrier(
        commandBuffers[currentFrame],
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0,
        nullptr);
    return tlas;
}

#endif
