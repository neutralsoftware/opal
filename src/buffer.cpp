//
// buffer.cpp
// As part of the Atlas project
// Created by Max Van den Eynde in 2025
// --------------------------------------------------
// Description: Core buffer definitions for allocating memory
// Copyright (c) 2025 maxvdec
//

#include "diagnostics.h"
#include <cstring>
#include <glad/glad.h>
#include <memory>
#include <opal/opal.h>
#include <stdexcept>
#include <string>
#include <utility>
#ifdef METAL
#include "metal_state.h"
#elif VULKAN
#include "vulkan/vulkan.h"
#include "vulkan_state.h"
#endif

namespace opal {

Buffer::~Buffer() {
#ifdef METAL
    metal::releaseBufferState(this);
#elif VULKAN
    vulkan::releaseBufferState(this);
#endif
}

namespace {
#ifdef OPENGL
uint getGLVertexAttributeType(VertexAttributeType type) {
    switch (type) {
    case VertexAttributeType::Float:
        return GL_FLOAT;
    case VertexAttributeType::Double:
        return GL_DOUBLE;
    case VertexAttributeType::Int:
        return GL_INT;
    case VertexAttributeType::UnsignedInt:
        return GL_UNSIGNED_INT;
    case VertexAttributeType::Short:
        return GL_SHORT;
    case VertexAttributeType::UnsignedShort:
        return GL_UNSIGNED_SHORT;
    case VertexAttributeType::Byte:
        return GL_BYTE;
    case VertexAttributeType::UnsignedByte:
        return GL_UNSIGNED_BYTE;
    default:
        return GL_FLOAT;
    }
}

#endif

} // namespace

std::shared_ptr<Buffer> Buffer::create(BufferUsage usage, size_t size,
                                       const void *data,
                                       MemoryUsageType memoryUsage,
                                       int callerId) {
    static uint32_t nextBufferId = 1;
    auto buffer = std::make_shared<Buffer>();
    buffer->usage = usage;
    buffer->memoryUsage = memoryUsage;
    buffer->bufferID = nextBufferId++;
#ifdef OPENGL
    glGenBuffers(1, &buffer->bufferID);
    uint glTarget;
    switch (usage) {
    case BufferUsage::VertexBuffer:
        glTarget = GL_ARRAY_BUFFER;
        break;
    case BufferUsage::IndexArray:
        glTarget = GL_ELEMENT_ARRAY_BUFFER;
        break;
    case BufferUsage::GeneralPurpose:
        glTarget = GL_ARRAY_BUFFER;
        break;
    case BufferUsage::UniformBuffer:
        throw std::runtime_error(
            "UniformBuffer is not support for the OpenGL backend in this "
            "version.");
    case BufferUsage::ShaderRead:
        throw std::runtime_error("ShaderRead is not support for the OpenGL "
                                 "backend in this version.");
    case BufferUsage::ShaderReadWrite:
        glTarget = GL_ARRAY_BUFFER;
        break;
    default:
        glTarget = GL_ARRAY_BUFFER;
        break;
    }
    glBindBuffer(glTarget, buffer->bufferID);
    glBufferData(glTarget, size, data, GL_STATIC_DRAW);
    glBindBuffer(glTarget, 0);
#elif defined(METAL)
    if (Device::globalInstance == nullptr) {
        throw std::runtime_error("Cannot create Metal buffer without device");
    }

    auto &deviceState = metal::deviceState(Device::globalInstance);
    if (deviceState.device == nullptr) {
        throw std::runtime_error("Metal device is not initialized");
    }

    auto &bufferState = metal::bufferState(buffer.get());
    bufferState.buffer = deviceState.device->newBuffer(
        static_cast<NS::UInteger>(size), MTL::ResourceStorageModeShared);
    if (bufferState.buffer == nullptr) {
        throw std::runtime_error("Failed to allocate Metal buffer");
    }
    bufferState.size = size;

    if (data != nullptr && size > 0) {
        std::memcpy(bufferState.buffer->contents(), data, size);
        if (bufferState.buffer->storageMode() == MTL::StorageModeManaged) {
            bufferState.buffer->didModifyRange(
                NS::Range::Make(0, static_cast<NS::UInteger>(size)));
        }
    }
#elif VULKAN
    if (Device::globalInstance == nullptr) {
        throw std::runtime_error("Cannot create Vulkan buffer without device");
    }

    auto &deviceState = vulkan::deviceState(Device::globalInstance);

    if (deviceState.device == VK_NULL_HANDLE) {
        throw std::runtime_error("Vulkan device is not initialized");
    }

    auto &state = vulkan::bufferState(buffer.get());
    state.size = std::max<VkDeviceSize>(static_cast<VkDeviceSize>(size), 1);

    state.usageFlags = vulkan::bufferUsageToVk(usage);

    state.memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    vulkan::createBuffer(deviceState, state.size, state.usageFlags,
                         state.memoryProperties, state.buffer, state.memory);

    VULKAN_GUARD(vkMapMemory(deviceState.device, state.memory, 0, state.size, 0,
                             &state.mapped),
                 "Failed to map Vulkan buffer memory");

    if (data != nullptr && size > 0) {
        std::memcpy(state.mapped, data, size);
    }
#endif

    detail::emit(ResourceEvent{
        std::to_string(callerId), ResourceType::Buffer,
        ResourceOperation::Created,
        Device::globalInstance
            ? static_cast<unsigned int>(Device::globalInstance->frameCount)
            : 0,
        static_cast<float>(size) / (1024.0f * 1024.0f)});
    return buffer;
}

void Buffer::updateData(size_t offset, size_t size, const void *data) {
#ifdef OPENGL
    uint glTarget;
    switch (usage) {
    case BufferUsage::VertexBuffer:
        glTarget = GL_ARRAY_BUFFER;
        break;
    case BufferUsage::IndexArray:
        glTarget = GL_ELEMENT_ARRAY_BUFFER;
        break;
    case BufferUsage::GeneralPurpose:
        glTarget = GL_ARRAY_BUFFER;
        break;
    case BufferUsage::UniformBuffer:
        throw std::runtime_error(
            "UniformBuffer is not support for the OpenGL backend in this "
            "version.");
    case BufferUsage::ShaderRead:
        throw std::runtime_error("ShaderRead is not support for the OpenGL "
                                 "backend in this version.");
    case BufferUsage::ShaderReadWrite:
        glTarget = GL_ARRAY_BUFFER;
        break;
    default:
        glTarget = GL_ARRAY_BUFFER;
        break;
    }
    glBindBuffer(glTarget, bufferID);
    glBufferSubData(glTarget, offset, size, data);
    glBindBuffer(glTarget, 0);
#elif defined(METAL)
    if (Device::globalInstance == nullptr) {
        throw std::runtime_error("Cannot update Metal buffer without device");
    }
    auto &deviceState = metal::deviceState(Device::globalInstance);
    auto &bufferState = metal::bufferState(this);
    if (bufferState.buffer == nullptr || deviceState.device == nullptr) {
        throw std::runtime_error("Metal buffer is not initialized");
    }
    if (data == nullptr || size == 0) {
        return;
    }

    size_t required = offset + size;
    if (required > bufferState.size) {
        size_t newSize = required;
        MTL::Buffer *oldBuffer = bufferState.buffer;
        MTL::Buffer *newBuffer = deviceState.device->newBuffer(
            static_cast<NS::UInteger>(newSize), MTL::ResourceStorageModeShared);
        if (newBuffer == nullptr) {
            throw std::runtime_error("Failed to resize Metal buffer");
        }
        if (oldBuffer != nullptr && bufferState.size > 0) {
            std::memcpy(newBuffer->contents(), oldBuffer->contents(),
                        bufferState.size);
            if (newBuffer->storageMode() == MTL::StorageModeManaged) {
                newBuffer->didModifyRange(NS::Range::Make(
                    0, static_cast<NS::UInteger>(bufferState.size)));
            }
        }
        bufferState.buffer = newBuffer;
        bufferState.size = newSize;
        if (oldBuffer != nullptr) {
            oldBuffer->release();
        }
    }

    std::memcpy(static_cast<uint8_t *>(bufferState.buffer->contents()) + offset,
                data, size);
    if (bufferState.buffer->storageMode() == MTL::StorageModeManaged) {
        bufferState.buffer->didModifyRange(
            NS::Range::Make(static_cast<NS::UInteger>(offset),
                            static_cast<NS::UInteger>(size)));
    }
#elif defined(VULKAN)
    if (Device::globalInstance == nullptr) {
        throw std::runtime_error("Cannot update Vulkan buffer without device");
    }

    if (data == nullptr || size == 0) {
        return;
    }

    auto &deviceState = vulkan::deviceState(Device::globalInstance);

    auto &state = vulkan::bufferState(this);

    if (state.buffer == VK_NULL_HANDLE || state.memory == VK_NULL_HANDLE) {
        throw std::runtime_error("Vulkan buffer is not initialized");
    }

    const VkDeviceSize requiredSize = static_cast<VkDeviceSize>(offset + size);

    if (requiredSize > state.size) {
        VkBuffer newBuffer = VK_NULL_HANDLE;
        VkDeviceMemory newMemory = VK_NULL_HANDLE;

        vulkan::createBuffer(deviceState, requiredSize, state.usageFlags,
                             state.memoryProperties, newBuffer, newMemory);

        void *newMapped = nullptr;

        VULKAN_GUARD(vkMapMemory(deviceState.device, newMemory, 0, requiredSize,
                                 0, &newMapped),
                     "Failed to map resized Vulkan buffer");

        if (state.mapped != nullptr && state.size > 0) {
            std::memcpy(newMapped, state.mapped,
                        static_cast<size_t>(state.size));
        }

        if (state.mapped != nullptr) {
            vkUnmapMemory(deviceState.device, state.memory);
        }

        vkDestroyBuffer(deviceState.device, state.buffer, nullptr);

        vkFreeMemory(deviceState.device, state.memory, nullptr);

        state.buffer = newBuffer;
        state.memory = newMemory;
        state.mapped = newMapped;
        state.size = requiredSize;
    }

    std::memcpy(static_cast<uint8_t *>(state.mapped) + offset, data, size);
#endif
}

void Buffer::bind(int callerId) const {
#ifdef OPENGL
    uint glTarget;
    switch (usage) {
    case BufferUsage::VertexBuffer:
        glTarget = GL_ARRAY_BUFFER;
        break;
    case BufferUsage::IndexArray:
        glTarget = GL_ELEMENT_ARRAY_BUFFER;
        break;
    case BufferUsage::GeneralPurpose:
        glTarget = GL_ARRAY_BUFFER;
        break;
    case BufferUsage::UniformBuffer:
        throw std::runtime_error(
            "UniformBuffer is not support for the OpenGL backend in this "
            "version.");
    case BufferUsage::ShaderRead:
        throw std::runtime_error("ShaderRead is not support for the OpenGL "
                                 "backend in this version.");
    case BufferUsage::ShaderReadWrite:
        glTarget = GL_ARRAY_BUFFER;
        break;
    default:
        glTarget = GL_ARRAY_BUFFER;
        break;
    }
    glBindBuffer(glTarget, bufferID);
#elif defined(METAL)
#endif

    detail::emit(ResourceEvent{
        std::to_string(callerId), ResourceType::Buffer,
        ResourceOperation::Loaded,
        Device::globalInstance
            ? static_cast<unsigned int>(Device::globalInstance->frameCount)
            : 0,
        0.0f});
}

void Buffer::unbind(int callerId) const {
#ifdef OPENGL
    uint glTarget;
    switch (usage) {
    case BufferUsage::VertexBuffer:
        glTarget = GL_ARRAY_BUFFER;
        break;
    case BufferUsage::IndexArray:
        glTarget = GL_ELEMENT_ARRAY_BUFFER;
        break;
    case BufferUsage::GeneralPurpose:
        glTarget = GL_ARRAY_BUFFER;
        break;
    case BufferUsage::UniformBuffer:
        throw std::runtime_error(
            "UniformBuffer is not support for the OpenGL backend in this "
            "version.");
    case BufferUsage::ShaderRead:
        throw std::runtime_error("ShaderRead is not support for the OpenGL "
                                 "backend in this version.");
    case BufferUsage::ShaderReadWrite:
        glTarget = GL_ARRAY_BUFFER;
        break;
    default:
        glTarget = GL_ARRAY_BUFFER;
        break;
    }
    glBindBuffer(glTarget, 0);
#elif defined(METAL)
#endif
    detail::emit(ResourceEvent{
        std::to_string(callerId), ResourceType::Buffer,
        ResourceOperation::Unloaded,
        Device::globalInstance
            ? static_cast<unsigned int>(Device::globalInstance->frameCount)
            : 0,
        0.0f});
}

std::shared_ptr<DrawingState>
DrawingState::create(std::shared_ptr<Buffer> vertexBuffer,
                     std::shared_ptr<Buffer> indexBuffer) {
    auto state = std::make_shared<DrawingState>();
    state->vertexBuffer = std::move(vertexBuffer);
    state->indexBuffer = std::move(indexBuffer);

#ifdef OPENGL
    glGenVertexArrays(1, &state->index);
#endif
    return state;
}

void DrawingState::setBuffers(std::shared_ptr<Buffer> vertexBuffer,
                              std::shared_ptr<Buffer> indexBuffer) {
    this->vertexBuffer = std::move(vertexBuffer);
    this->indexBuffer = std::move(indexBuffer);
}

void DrawingState::bind() const {
#ifdef OPENGL
    glBindVertexArray(index);
    if (vertexBuffer) {
        vertexBuffer->bind();
    }
    if (indexBuffer) {
        indexBuffer->bind();
    }
#elif defined(METAL)
#endif
}

void DrawingState::unbind() const {
#ifdef OPENGL
    glBindVertexArray(0);
    if (indexBuffer) {
        indexBuffer->unbind();
    }
    if (vertexBuffer) {
        vertexBuffer->unbind();
    }
#elif defined(METAL)
#endif
}

void DrawingState::configureAttributes(
    const std::vector<VertexAttributeBinding> &bindings) {
#ifdef OPENGL
    if (bindings.empty()) {
        return;
    }

    glBindVertexArray(index);

    for (const auto &binding : bindings) {
        auto buffer =
            binding.sourceBuffer ? binding.sourceBuffer : vertexBuffer;
        if (buffer == nullptr) {
            throw std::runtime_error("No vertex buffer bound for attribute '" +
                                     binding.attribute.name + "'.");
        }

        buffer->bind();
        glEnableVertexAttribArray(binding.attribute.location);
        glVertexAttribPointer(binding.attribute.location,
                              binding.attribute.size,
                              getGLVertexAttributeType(binding.attribute.type),
                              binding.attribute.normalized ? GL_TRUE : GL_FALSE,
                              binding.attribute.stride,
                              reinterpret_cast<void *>(static_cast<uintptr_t>(
                                  binding.attribute.offset)));

        unsigned int divisor = binding.attribute.divisor;
        if (binding.attribute.inputRate == VertexBindingInputRate::Instance) {
            divisor = divisor == 0 ? 1 : divisor;
        }
        glVertexAttribDivisor(binding.attribute.location, divisor);

        if (binding.sourceBuffer) {
            binding.sourceBuffer->unbind();
        }
    }

    glBindVertexArray(0);
#elif defined(METAL) || defined(VULKAN)
    bool hasInstanceBinding = false;
    for (const auto &binding : bindings) {
        if (binding.attribute.inputRate == VertexBindingInputRate::Instance &&
            binding.sourceBuffer != nullptr) {
            instanceBuffer = binding.sourceBuffer;
            hasInstanceBinding = true;
            break;
        }
    }
    if (!hasInstanceBinding) {
        instanceBuffer = nullptr;
    }
#else
    (void)bindings;
#endif
}

} // namespace opal
