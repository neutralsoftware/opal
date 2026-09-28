/*
 ray_tracing.cpp
 As part of the Atlas project
 Created by Max Van den Eynde in 2026
 --------------------------------------------------
 Description: Ray tracing APIs for using with the renderer
 Copyright (c) 2026 Max Van den Eynde
*/

#include "diagnostics.h"
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

#elif defined(VULKAN)

#include "vulkan_state.h"
#include <algorithm>
#include <array>
#include <stdexcept>
#include <vulkan/vulkan.h>

namespace {

using namespace opal;

void createAccelerationBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
                              VkDeviceSize size, VkBuffer &buffer,
                              VkDeviceMemory &memory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                       VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    VULKAN_GUARD(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer),
                 "Failed to create acceleration-structure buffer");

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device, buffer, &requirements);
    VkMemoryAllocateFlagsInfo flags{};
    flags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    VkMemoryAllocateInfo allocation{};
    allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation.pNext = &flags;
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex =
        vulkan::findMemoryType(physicalDevice, requirements.memoryTypeBits,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VULKAN_GUARD(vkAllocateMemory(device, &allocation, nullptr, &memory),
                 "Failed to allocate acceleration-structure memory");
    VULKAN_GUARD(vkBindBufferMemory(device, buffer, memory, 0),
                 "Failed to bind acceleration-structure memory");
}

VkDeviceAddress bufferAddress(VkDevice device, VkBuffer buffer) {
    VkBufferDeviceAddressInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    info.buffer = buffer;
    return vkGetBufferDeviceAddress(device, &info);
}

void destroyAccelerationBuffer(VkDevice device, uint64_t bufferValue,
                               uint64_t memoryValue) {
    auto buffer = vulkan::vulkanHandleFromUint64<VkBuffer>(bufferValue);
    auto memory = vulkan::vulkanHandleFromUint64<VkDeviceMemory>(memoryValue);
    if (buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, buffer, nullptr);
    }
    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, memory, nullptr);
    }
}

} // namespace

opal::PrimitiveAccelerationStructure::~PrimitiveAccelerationStructure() {
    if (Device::globalInstance == nullptr) {
        return;
    }
    auto &device = vulkan::deviceState(Device::globalInstance);
    if (accelerationStructure != 0) {
        vkDestroyAccelerationStructureKHR(
            device.device,
            vulkan::vulkanHandleFromUint64<VkAccelerationStructureKHR>(
                accelerationStructure),
            nullptr);
    }
    destroyAccelerationBuffer(device.device, scratchBuffer, scratchMemory);
    destroyAccelerationBuffer(device.device, accelerationBuffer,
                              accelerationMemory);
}

std::shared_ptr<opal::PrimitiveAccelerationStructure>
opal::PrimitiveAccelerationStructure::create(
    const std::vector<PrimitiveVertex> &vertices,
    const std::vector<uint32_t> &indices) {
    std::vector<float> positions;
    positions.reserve(vertices.size() * 3);
    for (const auto &vertex : vertices) {
        positions.insert(positions.end(), vertex.position, vertex.position + 3);
    }
    return create(positions, indices);
}

std::shared_ptr<opal::PrimitiveAccelerationStructure>
opal::PrimitiveAccelerationStructure::create(
    const std::vector<float> &positions, const std::vector<uint32_t> &indices) {
    if (positions.size() < 9 || positions.size() % 3 != 0 ||
        indices.size() < 3 || indices.size() % 3 != 0) {
        return nullptr;
    }
    if (Device::globalInstance == nullptr) {
        throw std::runtime_error("Cannot create BLAS without a Vulkan device");
    }

    auto result = std::make_shared<PrimitiveAccelerationStructure>();
    result->vertexData =
        Buffer::create(BufferUsage::GeneralPurpose,
                       positions.size() * sizeof(float), positions.data());
    result->indexData =
        Buffer::create(BufferUsage::GeneralPurpose,
                       indices.size() * sizeof(uint32_t), indices.data());
    auto &device = vulkan::deviceState(Device::globalInstance);
    auto &vertexState = vulkan::bufferState(result->vertexData.get());
    auto &indexState = vulkan::bufferState(result->indexData.get());

    VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
    triangles.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    triangles.vertexData.deviceAddress = vertexState.deviceAddress;
    triangles.vertexStride = sizeof(float) * 3;
    triangles.maxVertex = static_cast<uint32_t>(positions.size() / 3 - 1);
    triangles.indexType = VK_INDEX_TYPE_UINT32;
    triangles.indexData.deviceAddress = indexState.deviceAddress;

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.geometry.triangles = triangles;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;
    uint32_t primitiveCount = static_cast<uint32_t>(indices.size() / 3);
    VkAccelerationStructureBuildSizesInfoKHR sizes{};
    sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    vkGetAccelerationStructureBuildSizesKHR(
        device.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, &primitiveCount, &sizes);

    VkBuffer accelerationBuffer = VK_NULL_HANDLE;
    VkDeviceMemory accelerationMemory = VK_NULL_HANDLE;
    createAccelerationBuffer(device.device, device.physicalDeviceInfo.device,
                             sizes.accelerationStructureSize,
                             accelerationBuffer, accelerationMemory);
    VkAccelerationStructureCreateInfoKHR accelerationInfo{};
    accelerationInfo.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    accelerationInfo.buffer = accelerationBuffer;
    accelerationInfo.size = sizes.accelerationStructureSize;
    accelerationInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    VkAccelerationStructureKHR accelerationStructure = VK_NULL_HANDLE;
    VULKAN_GUARD(vkCreateAccelerationStructureKHR(device.device,
                                                  &accelerationInfo, nullptr,
                                                  &accelerationStructure),
                 "Failed to create BLAS");

    VkBuffer scratchBuffer = VK_NULL_HANDLE;
    VkDeviceMemory scratchMemory = VK_NULL_HANDLE;
    createAccelerationBuffer(device.device, device.physicalDeviceInfo.device,
                             sizes.buildScratchSize, scratchBuffer,
                             scratchMemory);
    result->accelerationStructure =
        vulkan::vulkanHandleToUint64(accelerationStructure);
    result->accelerationBuffer =
        vulkan::vulkanHandleToUint64(accelerationBuffer);
    result->accelerationMemory =
        vulkan::vulkanHandleToUint64(accelerationMemory);
    result->scratchBuffer = vulkan::vulkanHandleToUint64(scratchBuffer);
    result->scratchMemory = vulkan::vulkanHandleToUint64(scratchMemory);
    result->geometryBuffer = vulkan::vulkanHandleToUint64(vertexState.buffer);
    result->indexBuffer = vulkan::vulkanHandleToUint64(indexState.buffer);
    return result;
}

void opal::CommandBuffer::buildPrimitiveAccelerationStructure(
    const std::shared_ptr<PrimitiveAccelerationStructure> &blas) {
    if (blas == nullptr || blas->vertexData == nullptr ||
        blas->indexData == nullptr) {
        throw std::runtime_error("Invalid Vulkan BLAS");
    }
    auto &device = vulkan::deviceState(Device::globalInstance);
    auto &state = vulkan::commandBufferState(this);
    if (!state.recording || state.commandBuffer == VK_NULL_HANDLE) {
        throw std::runtime_error(
            "BLAS build requires recording Vulkan commands");
    }
    auto &vertexState = vulkan::bufferState(blas->vertexData.get());
    auto &indexState = vulkan::bufferState(blas->indexData.get());
    VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
    triangles.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    triangles.vertexData.deviceAddress = vertexState.deviceAddress;
    triangles.vertexStride = sizeof(float) * 3;
    triangles.maxVertex =
        static_cast<uint32_t>(vertexState.size / sizeof(float) / 3 - 1);
    triangles.indexType = VK_INDEX_TYPE_UINT32;
    triangles.indexData.deviceAddress = indexState.deviceAddress;
    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.geometry.triangles = triangles;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.dstAccelerationStructure =
        vulkan::vulkanHandleFromUint64<VkAccelerationStructureKHR>(
            blas->accelerationStructure);
    buildInfo.scratchData.deviceAddress = bufferAddress(
        device.device,
        vulkan::vulkanHandleFromUint64<VkBuffer>(blas->scratchBuffer));
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;
    VkAccelerationStructureBuildRangeInfoKHR range{};
    range.primitiveCount =
        static_cast<uint32_t>(indexState.size / sizeof(uint32_t) / 3);
    const VkAccelerationStructureBuildRangeInfoKHR *ranges[] = {&range};
    vkCmdBuildAccelerationStructuresKHR(state.commandBuffer, 1, &buildInfo,
                                        ranges);
    blas->isBuilt = true;
}

opal::InstanceAccelerationStructure::~InstanceAccelerationStructure() {
    if (Device::globalInstance == nullptr) {
        return;
    }
    auto &device = vulkan::deviceState(Device::globalInstance);
    if (accelerationStructure != 0) {
        vkDestroyAccelerationStructureKHR(
            device.device,
            vulkan::vulkanHandleFromUint64<VkAccelerationStructureKHR>(
                accelerationStructure),
            nullptr);
    }
    destroyAccelerationBuffer(device.device, scratchBuffer, scratchMemory);
    destroyAccelerationBuffer(device.device, accelerationBuffer,
                              accelerationMemory);
}

std::shared_ptr<opal::InstanceAccelerationStructure>
opal::InstanceAccelerationStructure::create(
    const std::vector<opal::AccelerationStructureInstance> &instances) {
    if (instances.empty() || Device::globalInstance == nullptr) {
        return nullptr;
    }
    auto result = std::make_shared<InstanceAccelerationStructure>();
    std::vector<VkAccelerationStructureInstanceKHR> descriptors(
        instances.size());
    result->blases.reserve(instances.size());
    for (size_t index = 0; index < instances.size(); ++index) {
        const auto &instance = instances[index];
        if (instance.blas == nullptr || !instance.blas->isBuilt) {
            throw std::runtime_error(
                "All Vulkan BLAS must be built before TLAS creation");
        }
        result->blases.push_back(instance.blas);
        auto &descriptor = descriptors[index];
        std::memset(&descriptor, 0, sizeof(descriptor));
        for (uint32_t row = 0; row < 3; ++row) {
            for (uint32_t column = 0; column < 4; ++column) {
                descriptor.transform.matrix[row][column] =
                    instance.transform[column][row];
            }
        }
        descriptor.instanceCustomIndex = instance.instanceId & 0x00FFFFFFu;
        descriptor.mask = instance.mask ? instance.mask : 0xFF;
        descriptor.instanceShaderBindingTableRecordOffset = 0;
        descriptor.flags =
            instance.cullDisable
                ? VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR
                : 0;
        auto &device = vulkan::deviceState(Device::globalInstance);
        VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
        addressInfo.sType =
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        addressInfo.accelerationStructure =
            vulkan::vulkanHandleFromUint64<VkAccelerationStructureKHR>(
                instance.blas->nativeHandle());
        descriptor.accelerationStructureReference =
            vkGetAccelerationStructureDeviceAddressKHR(device.device,
                                                       &addressInfo);
    }
    result->instanceData = Buffer::create(
        BufferUsage::GeneralPurpose,
        descriptors.size() * sizeof(VkAccelerationStructureInstanceKHR),
        descriptors.data());

    auto &device = vulkan::deviceState(Device::globalInstance);
    auto &instanceState = vulkan::bufferState(result->instanceData.get());
    VkAccelerationStructureGeometryInstancesDataKHR instanceGeometry{};
    instanceGeometry.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instanceGeometry.arrayOfPointers = VK_FALSE;
    instanceGeometry.data.deviceAddress = instanceState.deviceAddress;
    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.geometry.instances = instanceGeometry;
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;
    uint32_t primitiveCount = static_cast<uint32_t>(descriptors.size());
    VkAccelerationStructureBuildSizesInfoKHR sizes{};
    sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    vkGetAccelerationStructureBuildSizesKHR(
        device.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, &primitiveCount, &sizes);
    VkBuffer accelerationBuffer = VK_NULL_HANDLE;
    VkDeviceMemory accelerationMemory = VK_NULL_HANDLE;
    createAccelerationBuffer(device.device, device.physicalDeviceInfo.device,
                             sizes.accelerationStructureSize,
                             accelerationBuffer, accelerationMemory);
    VkAccelerationStructureCreateInfoKHR accelerationInfo{};
    accelerationInfo.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    accelerationInfo.buffer = accelerationBuffer;
    accelerationInfo.size = sizes.accelerationStructureSize;
    accelerationInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    VkAccelerationStructureKHR accelerationStructure = VK_NULL_HANDLE;
    VULKAN_GUARD(vkCreateAccelerationStructureKHR(device.device,
                                                  &accelerationInfo, nullptr,
                                                  &accelerationStructure),
                 "Failed to create TLAS");
    VkBuffer scratchBuffer = VK_NULL_HANDLE;
    VkDeviceMemory scratchMemory = VK_NULL_HANDLE;
    createAccelerationBuffer(device.device, device.physicalDeviceInfo.device,
                             sizes.buildScratchSize, scratchBuffer,
                             scratchMemory);
    result->accelerationStructure =
        vulkan::vulkanHandleToUint64(accelerationStructure);
    result->accelerationBuffer =
        vulkan::vulkanHandleToUint64(accelerationBuffer);
    result->accelerationMemory =
        vulkan::vulkanHandleToUint64(accelerationMemory);
    result->scratchBuffer = vulkan::vulkanHandleToUint64(scratchBuffer);
    result->scratchMemory = vulkan::vulkanHandleToUint64(scratchMemory);
    return result;
}

std::shared_ptr<opal::InstanceAccelerationStructure>
opal::CommandBuffer::buildAccelerationStructures(
    const std::vector<std::shared_ptr<PrimitiveAccelerationStructure>> &blases,
    const std::vector<AccelerationStructureInstance> &instances) {
    for (const auto &blas : blases) {
        buildPrimitiveAccelerationStructure(blas);
    }
    auto tlas = InstanceAccelerationStructure::create(instances);
    buildInstanceAccelerationStructure(tlas);
    return tlas;
}

void opal::CommandBuffer::buildInstanceAccelerationStructure(
    const std::shared_ptr<InstanceAccelerationStructure> &tlas) {
    if (tlas == nullptr || tlas->instanceData == nullptr) {
        throw std::runtime_error("Invalid Vulkan TLAS");
    }
    auto &device = vulkan::deviceState(Device::globalInstance);
    auto &state = vulkan::commandBufferState(this);
    if (!state.recording || state.commandBuffer == VK_NULL_HANDLE) {
        throw std::runtime_error(
            "TLAS build requires recording Vulkan commands");
    }
    auto &instanceState = vulkan::bufferState(tlas->instanceData.get());
    VkAccelerationStructureGeometryInstancesDataKHR instanceGeometry{};
    instanceGeometry.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instanceGeometry.data.deviceAddress = instanceState.deviceAddress;
    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.geometry.instances = instanceGeometry;
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.dstAccelerationStructure =
        vulkan::vulkanHandleFromUint64<VkAccelerationStructureKHR>(
            tlas->accelerationStructure);
    buildInfo.scratchData.deviceAddress = bufferAddress(
        device.device,
        vulkan::vulkanHandleFromUint64<VkBuffer>(tlas->scratchBuffer));
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;
    VkAccelerationStructureBuildRangeInfoKHR range{};
    range.primitiveCount = static_cast<uint32_t>(tlas->blases.size());
    const VkAccelerationStructureBuildRangeInfoKHR *ranges[] = {&range};
    vkCmdBuildAccelerationStructuresKHR(state.commandBuffer, 1, &buildInfo,
                                        ranges);
    tlas->isBuilt = true;
}

void opal::CommandBuffer::bindInstanceAccelerationStructure(
    const std::shared_ptr<InstanceAccelerationStructure> &tlas,
    uint32_t binding) {
    if (boundPipeline == nullptr || tlas == nullptr || !tlas->isBuilt) {
        throw std::runtime_error(
            "Vulkan TLAS binding requires a built pipeline and TLAS");
    }
    auto &pipeline = vulkan::pipelineState(boundPipeline.get());
    pipeline.boundAccelerationStructures[vulkan::bindingKey(0, binding)] = tlas;
    pipeline.descriptorsDirty = true;
}

#endif
