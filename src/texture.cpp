//
// texture.cpp
// As part of the Atlas project
// Created by Max Van den Eynde in 2025
// --------------------------------------------------
// Description: Texture support for Opal
// Copyright (c) 2025 maxvdec
//

#include "diagnostics.h"
#include "opal/opal.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <glad/glad.h>
#include <memory>
#include <stdexcept>
#include <sys/types.h>
#include <vector>
#ifdef METAL
#include "metal_state.h"
#endif

#ifdef VULKAN
#include "vulkan_state.h"
#include <vulkan/vulkan.h>
#endif

namespace opal {

Texture::~Texture() {
#ifdef METAL
    metal::releaseTextureState(this);
#elif VULKAN
    vulkan::releaseTextureState(this);
#endif
}

namespace {

#ifdef OPENGL
constexpr GLenum glInternalFormatTable[] = {GL_RGBA8,
                                            GL_SRGB8_ALPHA8,
                                            GL_RGB8,
                                            GL_SRGB8,
                                            GL_RGBA16F,
                                            GL_RGB16F,
                                            GL_DEPTH24_STENCIL8,
                                            GL_DEPTH_COMPONENT24,
                                            GL_DEPTH_COMPONENT32F,
                                            GL_R8,
                                            GL_R16F};

constexpr GLenum glDataFormatTable[] = {
    GL_RGBA, GL_RGB, GL_RED, GL_BGR, GL_BGRA, GL_DEPTH_COMPONENT,
};

constexpr GLenum glTextureTypeTable[] = {GL_TEXTURE_2D, GL_TEXTURE_CUBE_MAP,
                                         GL_TEXTURE_3D, GL_TEXTURE_2D_ARRAY,
                                         GL_TEXTURE_2D_MULTISAMPLE};

constexpr GLenum glWrapModeTable[] = {GL_REPEAT, GL_MIRRORED_REPEAT,
                                      GL_CLAMP_TO_EDGE, GL_CLAMP_TO_BORDER};

constexpr GLenum glFilterModeTable[] = {
    GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_LINEAR};

inline GLenum getGLInternalFormat(TextureFormat format) {
    return glInternalFormatTable[static_cast<int>(format)];
}

inline GLenum getGLDataFormat(TextureDataFormat format) {
    return glDataFormatTable[static_cast<int>(format)];
}

inline GLenum getGLTextureType(TextureType type) {
    return glTextureTypeTable[static_cast<int>(type)];
}

inline GLenum getGLWrapMode(TextureWrapMode mode) {
    return glWrapModeTable[static_cast<int>(mode)];
}

inline GLenum getGLFilterMode(TextureFilterMode mode) {
    return glFilterModeTable[static_cast<int>(mode)];
}

#endif

inline float calculateTextureSizeMb(TextureFormat format, int width, int height,
                                    int depth = 1) {
    size_t bytesPerPixel = 0;

    switch (format) {
    case TextureFormat::Rgba8:
        bytesPerPixel = 4;
        break;
    case TextureFormat::Rgb8:
        bytesPerPixel = 3;
        break;
    case TextureFormat::Rgba16F:
        bytesPerPixel = 8;
        break;
    case TextureFormat::Rgb16F:
        bytesPerPixel = 6;
        break;
    case TextureFormat::Depth24Stencil8:
        bytesPerPixel = 4;
        break;
    case TextureFormat::DepthComponent24:
        bytesPerPixel = 3;
        break;
    case TextureFormat::Depth32F:
        bytesPerPixel = 4;
        break;
    case TextureFormat::Red8:
        bytesPerPixel = 1;
        break;
    case TextureFormat::Red16F:
        bytesPerPixel = 2;
        break;
    default:
        bytesPerPixel = 4;
        break;
    }

    size_t totalBytes =
        static_cast<size_t>(width) * height * depth * bytesPerPixel;
    return totalBytes / (1024.0f * 1024.0f);
}

#ifdef METAL
struct MetalUploadBuffer {
    std::vector<uint8_t> converted;
    const void *bytes = nullptr;
    NS::UInteger bytesPerRow = 0;
    NS::UInteger bytesPerImage = 0;
};

inline size_t metalDataChannels(TextureDataFormat format) {
    switch (format) {
    case TextureDataFormat::Rgba:
        return 4;
    case TextureDataFormat::Rgb:
        return 3;
    case TextureDataFormat::Red:
        return 1;
    case TextureDataFormat::DepthComponent:
        return 1;
    default:
        return 4;
    }
}

inline size_t metalDestinationChannels(TextureFormat format) {
    switch (format) {
    case TextureFormat::Red8:
    case TextureFormat::Red16F:
    case TextureFormat::DepthComponent24:
    case TextureFormat::Depth32F:
        return 1;
    case TextureFormat::Depth24Stencil8:
        return 2;
    default:
        return 4;
    }
}

inline size_t metalSourceBytesPerChannel(TextureFormat textureFormat,
                                         TextureDataFormat dataFormat) {
    if (dataFormat == TextureDataFormat::DepthComponent) {
        return 4;
    }
    switch (textureFormat) {
    case TextureFormat::Rgba16F:
    case TextureFormat::Rgb16F:
    case TextureFormat::Red16F:
        return 4;
    default:
        return 1;
    }
}

inline size_t metalDestinationBytesPerChannel(TextureFormat textureFormat) {
    switch (textureFormat) {
    case TextureFormat::Rgba16F:
    case TextureFormat::Rgb16F:
    case TextureFormat::Red16F:
        return 2;
    case TextureFormat::DepthComponent24:
    case TextureFormat::Depth32F:
        return 4;
    default:
        return 1;
    }
}

inline uint16_t metalFloatToHalf(float value) {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));

    uint32_t sign = (bits >> 16) & 0x8000u;
    uint32_t mantissa = bits & 0x007fffffu;
    int32_t exponent = static_cast<int32_t>((bits >> 23) & 0xffu) - 127 + 15;

    if (exponent <= 0) {
        if (exponent < -10) {
            return static_cast<uint16_t>(sign);
        }
        mantissa = (mantissa | 0x00800000u) >> (1 - exponent);
        if ((mantissa & 0x00001000u) != 0u) {
            mantissa += 0x00002000u;
        }
        return static_cast<uint16_t>(sign | (mantissa >> 13));
    }

    if (exponent >= 31) {
        if (mantissa == 0u) {
            return static_cast<uint16_t>(sign | 0x7c00u);
        }
        mantissa >>= 13;
        return static_cast<uint16_t>(sign | 0x7c00u | mantissa |
                                     (mantissa == 0u));
    }

    if ((mantissa & 0x00001000u) != 0u) {
        mantissa += 0x00002000u;
        if ((mantissa & 0x00800000u) != 0u) {
            mantissa = 0u;
            exponent += 1;
            if (exponent >= 31) {
                return static_cast<uint16_t>(sign | 0x7c00u);
            }
        }
    }

    return static_cast<uint16_t>(
        sign | (static_cast<uint32_t>(exponent) << 10) | (mantissa >> 13));
}

inline float metalReadChannel(const uint8_t *src, size_t bytesPerChannel) {
    if (bytesPerChannel == 1) {
        return static_cast<float>(src[0]) / 255.0f;
    }
    if (bytesPerChannel == 4) {
        float value = 0.0f;
        std::memcpy(&value, src, sizeof(float));
        return value;
    }
    return 0.0f;
}

inline void metalWriteChannel(uint8_t *dst, size_t bytesPerChannel,
                              float value) {
    if (bytesPerChannel == 1) {
        float clamped = std::clamp(value, 0.0f, 1.0f);
        dst[0] = static_cast<uint8_t>(std::lroundf(clamped * 255.0f));
        return;
    }
    if (bytesPerChannel == 2) {
        uint16_t half = metalFloatToHalf(value);
        std::memcpy(dst, &half, sizeof(uint16_t));
        return;
    }
    if (bytesPerChannel == 4) {
        std::memcpy(dst, &value, sizeof(float));
    }
}

MetalUploadBuffer prepareMetalUpload(const void *data, int width, int height,
                                     int depth, TextureFormat textureFormat,
                                     TextureDataFormat dataFormat) {
    MetalUploadBuffer upload{};
    if (data == nullptr || width <= 0 || height <= 0 || depth <= 0) {
        return upload;
    }

    size_t srcChannels = metalDataChannels(dataFormat);
    size_t dstChannels = metalDestinationChannels(textureFormat);
    size_t srcBytesPerChannel =
        metalSourceBytesPerChannel(textureFormat, dataFormat);
    size_t dstBytesPerChannel = metalDestinationBytesPerChannel(textureFormat);
    size_t texelCount = static_cast<size_t>(width) *
                        static_cast<size_t>(height) *
                        static_cast<size_t>(depth);

    size_t srcBytesPerTexel = srcChannels * srcBytesPerChannel;
    size_t dstBytesPerTexel = dstChannels * dstBytesPerChannel;
    size_t srcRowBytes = static_cast<size_t>(width) * srcBytesPerTexel;
    size_t dstRowBytes = static_cast<size_t>(width) * dstBytesPerTexel;

    if (srcChannels == dstChannels &&
        srcBytesPerChannel == dstBytesPerChannel) {
        upload.bytes = data;
        upload.bytesPerRow = static_cast<NS::UInteger>(srcRowBytes);
        upload.bytesPerImage = static_cast<NS::UInteger>(
            srcRowBytes * static_cast<size_t>(height));
        return upload;
    }

    upload.converted.resize(texelCount * dstBytesPerTexel);
    const uint8_t *srcBytes = static_cast<const uint8_t *>(data);
    uint8_t *dstBytes = upload.converted.data();

    for (size_t texel = 0; texel < texelCount; ++texel) {
        const uint8_t *srcTexel = srcBytes + (texel * srcBytesPerTexel);
        uint8_t *dstTexel = dstBytes + (texel * dstBytesPerTexel);

        for (size_t channel = 0; channel < dstChannels; ++channel) {
            float value = 0.0f;
            if (channel < srcChannels) {
                value =
                    metalReadChannel(srcTexel + (channel * srcBytesPerChannel),
                                     srcBytesPerChannel);
            } else if (channel == 3) {
                value = 1.0f;
            }
            metalWriteChannel(dstTexel + (channel * dstBytesPerChannel),
                              dstBytesPerChannel, value);
        }
    }

    upload.bytes = upload.converted.data();
    upload.bytesPerRow = static_cast<NS::UInteger>(dstRowBytes);
    upload.bytesPerImage =
        static_cast<NS::UInteger>(dstRowBytes * static_cast<size_t>(height));
    return upload;
}
#endif

#ifdef VULKAN
struct VulkanTransferBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};

struct VulkanUploadData {
    std::vector<uint8_t> bytes;
};

uint16_t floatToHalf(float value) {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    uint32_t sign = (bits >> 16) & 0x8000u;
    uint32_t mantissa = bits & 0x007fffffu;
    int32_t exponent =
        static_cast<int32_t>((bits >> 23) & 0xffu) - 127 + 15;
    if (exponent <= 0) {
        if (exponent < -10) {
            return static_cast<uint16_t>(sign);
        }
        mantissa = (mantissa | 0x00800000u) >> (1 - exponent);
        if ((mantissa & 0x00001000u) != 0u) {
            mantissa += 0x00002000u;
        }
        return static_cast<uint16_t>(sign | (mantissa >> 13));
    }
    if (exponent >= 31) {
        if (mantissa == 0u) {
            return static_cast<uint16_t>(sign | 0x7c00u);
        }
        mantissa >>= 13;
        return static_cast<uint16_t>(sign | 0x7c00u | mantissa |
                                     (mantissa == 0u));
    }
    if ((mantissa & 0x00001000u) != 0u) {
        mantissa += 0x00002000u;
        if ((mantissa & 0x00800000u) != 0u) {
            mantissa = 0u;
            ++exponent;
            if (exponent >= 31) {
                return static_cast<uint16_t>(sign | 0x7c00u);
            }
        }
    }
    return static_cast<uint16_t>(
        sign | (static_cast<uint32_t>(exponent) << 10) | (mantissa >> 13));
}

float halfToFloat(uint16_t value) {
    uint32_t sign = static_cast<uint32_t>(value & 0x8000u) << 16;
    int32_t exponent = static_cast<int32_t>((value >> 10) & 0x1fu);
    uint32_t mantissa = value & 0x03ffu;
    uint32_t bits = 0;
    if (exponent == 0) {
        if (mantissa == 0) {
            bits = sign;
        } else {
            exponent = 1;
            while ((mantissa & 0x0400u) == 0) {
                mantissa <<= 1;
                --exponent;
            }
            mantissa &= 0x03ffu;
            bits = sign |
                   (static_cast<uint32_t>(exponent + 112) << 23) |
                   (mantissa << 13);
        }
    } else if (exponent == 31) {
        bits = sign | 0x7f800000u | (mantissa << 13);
    } else {
        bits = sign | (static_cast<uint32_t>(exponent + 112) << 23) |
               (mantissa << 13);
    }
    float result = 0.0f;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

size_t dataFormatChannels(TextureDataFormat format) {
    switch (format) {
    case TextureDataFormat::Rgba:
    case TextureDataFormat::Bgra:
        return 4;
    case TextureDataFormat::Rgb:
    case TextureDataFormat::Bgr:
        return 3;
    case TextureDataFormat::Red:
    case TextureDataFormat::DepthComponent:
        return 1;
    }
    throw std::runtime_error("Unsupported Vulkan texture data format");
}

size_t textureFormatChannels(TextureFormat format) {
    switch (format) {
    case TextureFormat::Rgba8:
    case TextureFormat::sRgba8:
    case TextureFormat::Rgba16F:
        return 4;
    case TextureFormat::Rgb8:
    case TextureFormat::sRgb8:
    case TextureFormat::Rgb16F:
        return 4;
    default:
        return 1;
    }
}

bool isHalfTextureFormat(TextureFormat format) {
    return format == TextureFormat::Rgba16F ||
           format == TextureFormat::Rgb16F ||
           format == TextureFormat::Red16F;
}

bool isDepthTextureFormat(TextureFormat format) {
    return format == TextureFormat::Depth24Stencil8 ||
           format == TextureFormat::DepthComponent24 ||
           format == TextureFormat::Depth32F;
}

size_t sourceChannelSize(TextureFormat format, TextureDataFormat dataFormat) {
    if (isHalfTextureFormat(format) ||
        dataFormat == TextureDataFormat::DepthComponent) {
        return sizeof(float);
    }
    return sizeof(uint8_t);
}

float readSourceChannel(const uint8_t *source, size_t channelSize) {
    if (channelSize == sizeof(float)) {
        float value = 0.0f;
        std::memcpy(&value, source, sizeof(value));
        return value;
    }
    return static_cast<float>(*source) / 255.0f;
}

VulkanUploadData prepareVulkanUpload(const void *data, size_t texelCount,
                                     TextureFormat format,
                                     TextureDataFormat dataFormat) {
    VulkanUploadData upload{};
    if (data == nullptr || texelCount == 0) {
        return upload;
    }

    size_t sourceChannels = dataFormatChannels(dataFormat);
    size_t destinationChannels = textureFormatChannels(format);
    size_t channelSize = sourceChannelSize(format, dataFormat);
    size_t destinationSize = vulkan::bytesPerPixel(format);
    upload.bytes.resize(texelCount * destinationSize);

    const auto *sourceBytes = static_cast<const uint8_t *>(data);
    for (size_t texel = 0; texel < texelCount; ++texel) {
        float channels[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        for (size_t channel = 0; channel < sourceChannels; ++channel) {
            size_t sourceChannel = channel;
            if (dataFormat == TextureDataFormat::Bgr ||
                dataFormat == TextureDataFormat::Bgra) {
                if (channel == 0) {
                    sourceChannel = 2;
                } else if (channel == 2) {
                    sourceChannel = 0;
                }
            }
            channels[channel] = readSourceChannel(
                sourceBytes +
                    ((texel * sourceChannels + sourceChannel) * channelSize),
                channelSize);
        }

        uint8_t *destination =
            upload.bytes.data() + texel * destinationSize;
        if (format == TextureFormat::Depth32F) {
            std::memcpy(destination, channels, sizeof(float));
        } else if (format == TextureFormat::DepthComponent24 ||
                   format == TextureFormat::Depth24Stencil8) {
            uint32_t depth = static_cast<uint32_t>(
                std::clamp(channels[0], 0.0f, 1.0f) * 16777215.0f);
            uint32_t packed = format == TextureFormat::DepthComponent24
                                  ? depth << 8
                                  : depth;
            std::memcpy(destination, &packed, sizeof(packed));
        } else if (isHalfTextureFormat(format)) {
            for (size_t channel = 0; channel < destinationChannels; ++channel) {
                uint16_t half = floatToHalf(channels[channel]);
                std::memcpy(destination + channel * sizeof(uint16_t), &half,
                            sizeof(half));
            }
        } else {
            for (size_t channel = 0; channel < destinationChannels; ++channel) {
                destination[channel] = static_cast<uint8_t>(std::lround(
                    std::clamp(channels[channel], 0.0f, 1.0f) * 255.0f));
            }
        }
    }
    return upload;
}

void createTransferBuffer(vulkan::DeviceState &deviceState, VkDeviceSize size,
                          VkBufferUsageFlags usage,
                          VkMemoryPropertyFlags properties,
                          VulkanTransferBuffer &buffer) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VULKAN_GUARD(vkCreateBuffer(deviceState.device, &bufferInfo, nullptr,
                                &buffer.buffer),
                 "Failed to create Vulkan transfer buffer");

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(deviceState.device, buffer.buffer,
                                  &requirements);
    VkMemoryAllocateInfo allocationInfo{};
    allocationInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocationInfo.allocationSize = requirements.size;
    allocationInfo.memoryTypeIndex = vulkan::findMemoryType(
        deviceState.physicalDeviceInfo.device, requirements.memoryTypeBits,
        properties);

    VkResult result = vkAllocateMemory(deviceState.device, &allocationInfo,
                                       nullptr, &buffer.memory);
    if (result != VK_SUCCESS) {
        vkDestroyBuffer(deviceState.device, buffer.buffer, nullptr);
        buffer.buffer = VK_NULL_HANDLE;
        VULKAN_GUARD(result, "Failed to allocate Vulkan transfer memory");
    }
    VULKAN_GUARD(vkBindBufferMemory(deviceState.device, buffer.buffer,
                                    buffer.memory, 0),
                 "Failed to bind Vulkan transfer memory");
}

void destroyTransferBuffer(vulkan::DeviceState &deviceState,
                           VulkanTransferBuffer &buffer) {
    if (buffer.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(deviceState.device, buffer.buffer, nullptr);
    }
    if (buffer.memory != VK_NULL_HANDLE) {
        vkFreeMemory(deviceState.device, buffer.memory, nullptr);
    }
    buffer = {};
}

VkImageLayout defaultVulkanTextureLayout(const vulkan::TextureState &state) {
    return isDepthTextureFormat(state.opalFormat)
               ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
               : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void layoutAccess(VkImageLayout layout, VkPipelineStageFlags2 &stage,
                  VkAccessFlags2 &access) {
    switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
        stage = VK_PIPELINE_STAGE_2_NONE;
        access = VK_ACCESS_2_NONE;
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        access = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        access = VK_ACCESS_2_TRANSFER_READ_BIT;
        break;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        access = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
                 VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        stage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                 VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;
    default:
        stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT |
                 VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
        break;
    }
}

void transitionVulkanImage(VkCommandBuffer commandBuffer,
                           vulkan::TextureState &state, VkImageLayout oldLayout,
                           VkImageLayout newLayout, uint32_t baseMipLevel,
                           uint32_t levelCount, uint32_t baseArrayLayer,
                           uint32_t layerCount) {
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    layoutAccess(oldLayout, barrier.srcStageMask, barrier.srcAccessMask);
    layoutAccess(newLayout, barrier.dstStageMask, barrier.dstAccessMask);
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = state.image;
    barrier.subresourceRange.aspectMask = state.aspectMask;
    barrier.subresourceRange.baseMipLevel = baseMipLevel;
    barrier.subresourceRange.levelCount = levelCount;
    barrier.subresourceRange.baseArrayLayer = baseArrayLayer;
    barrier.subresourceRange.layerCount = layerCount;

    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
}

VkImageAspectFlags transferAspect(const vulkan::TextureState &state) {
    return (state.aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT) != 0
               ? VK_IMAGE_ASPECT_DEPTH_BIT
               : VK_IMAGE_ASPECT_COLOR_BIT;
}

VkImageViewType imageViewType(TextureType type) {
    switch (type) {
    case TextureType::TextureCubeMap:
        return VK_IMAGE_VIEW_TYPE_CUBE;
    case TextureType::Texture3D:
        return VK_IMAGE_VIEW_TYPE_3D;
    case TextureType::Texture2DArray:
        return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    default:
        return VK_IMAGE_VIEW_TYPE_2D;
    }
}

VkSamplerAddressMode samplerAddressMode(TextureWrapMode mode) {
    switch (mode) {
    case TextureWrapMode::Repeat:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case TextureWrapMode::MirroredRepeat:
        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case TextureWrapMode::ClampToEdge:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case TextureWrapMode::ClampToBorder:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    }
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

VkFilter samplerFilter(TextureFilterMode mode) {
    return mode == TextureFilterMode::Nearest ||
                   mode == TextureFilterMode::NearestMipmapNearest
               ? VK_FILTER_NEAREST
               : VK_FILTER_LINEAR;
}

VkSamplerMipmapMode samplerMipmapMode(TextureFilterMode mode) {
    return mode == TextureFilterMode::LinearMipmapLinear
               ? VK_SAMPLER_MIPMAP_MODE_LINEAR
               : VK_SAMPLER_MIPMAP_MODE_NEAREST;
}

VkBorderColor samplerBorderColor(const glm::vec4 &color) {
    if (color.a <= 0.0f) {
        return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    }
    if (color.r >= 0.5f && color.g >= 0.5f && color.b >= 0.5f) {
        return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    }
    return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
}

void rebuildVulkanSampler(Texture *texture) {
    auto &deviceState = vulkan::deviceState(Device::globalInstance);
    auto &state = vulkan::textureState(texture);
    if (state.sampler != VK_NULL_HANDLE) {
        vkDestroySampler(deviceState.device, state.sampler, nullptr);
        state.sampler = VK_NULL_HANDLE;
    }
    if (state.type == TextureType::Texture2DMultisample) {
        return;
    }

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = samplerFilter(state.magFilter);
    samplerInfo.minFilter = samplerFilter(state.minFilter);
    samplerInfo.mipmapMode = samplerMipmapMode(state.minFilter);
    samplerInfo.addressModeU = samplerAddressMode(state.wrapS);
    samplerInfo.addressModeV = samplerAddressMode(state.wrapT);
    samplerInfo.addressModeW = samplerAddressMode(state.wrapR);
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.anisotropyEnable =
        deviceState.physicalDeviceInfo.features.features.samplerAnisotropy;
    samplerInfo.maxAnisotropy = samplerInfo.anisotropyEnable
                                    ? deviceState.physicalDeviceInfo.properties
                                          .limits.maxSamplerAnisotropy
                                    : 1.0f;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(state.mipLevels);
    samplerInfo.borderColor = samplerBorderColor(state.borderColor);
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    VULKAN_GUARD(vkCreateSampler(deviceState.device, &samplerInfo, nullptr,
                                 &state.sampler),
                 "Failed to create Vulkan texture sampler");
    state.generation++;
}

void destroyVulkanImage(Texture *texture) {
    auto &deviceState = vulkan::deviceState(Device::globalInstance);
    auto &state = vulkan::textureState(texture);
    if (state.imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(deviceState.device, state.imageView, nullptr);
        state.imageView = VK_NULL_HANDLE;
    }
    if (state.sampler != VK_NULL_HANDLE) {
        vkDestroySampler(deviceState.device, state.sampler, nullptr);
        state.sampler = VK_NULL_HANDLE;
    }
    if (state.image != VK_NULL_HANDLE && state.ownsImage) {
        vkDestroyImage(deviceState.device, state.image, nullptr);
        state.image = VK_NULL_HANDLE;
    }
    if (state.memory != VK_NULL_HANDLE && state.ownsImage) {
        vkFreeMemory(deviceState.device, state.memory, nullptr);
        state.memory = VK_NULL_HANDLE;
    }
    state.ownsImage = false;
    state.layout = VK_IMAGE_LAYOUT_UNDEFINED;
}

void createVulkanImage(Texture *texture, int depth) {
    auto &deviceState = vulkan::deviceState(Device::globalInstance);
    auto &state = vulkan::textureState(texture);
    destroyVulkanImage(texture);

    state.width = static_cast<uint32_t>(std::max(texture->width, 1));
    state.height = static_cast<uint32_t>(std::max(texture->height, 1));
    state.depth = static_cast<uint32_t>(std::max(depth, 1));
    state.mipLevels = std::max<uint32_t>(texture->mipLevels, 1);
    state.arrayLayers = texture->type == TextureType::TextureCubeMap ? 6u : 1u;
    state.sampleCount = vulkan::sampleCountFlagBitsFor(texture->samples);
    state.format = vulkan::textureFormatToVkFormat(texture->format);
    state.aspectMask = vulkan::textureAspectFlagsFor(texture->format);
    state.opalFormat = texture->format;
    state.type = texture->type;

    VkSampleCountFlags supportedSamples =
        isDepthTextureFormat(texture->format)
            ? deviceState.physicalDeviceInfo.properties.limits
                  .framebufferDepthSampleCounts
            : deviceState.physicalDeviceInfo.properties.limits
                  .framebufferColorSampleCounts;
    if ((supportedSamples & state.sampleCount) == 0) {
        throw std::runtime_error(
            "Requested Vulkan texture sample count is unsupported");
    }

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.flags = texture->type == TextureType::TextureCubeMap
                          ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT
                          : 0;
    imageInfo.imageType = vulkan::textureTypeToVk(texture->type);
    imageInfo.format = state.format;
    imageInfo.extent = {state.width, state.height, state.depth};
    imageInfo.mipLevels = state.mipLevels;
    imageInfo.arrayLayers = state.arrayLayers;
    imageInfo.samples = state.sampleCount;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage =
        vulkan::textureUsageFlagsFor(texture->type, texture->format);
    VkFormatProperties formatProperties{};
    vkGetPhysicalDeviceFormatProperties(deviceState.physicalDeviceInfo.device,
                                        state.format, &formatProperties);
    if ((formatProperties.optimalTilingFeatures &
         VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) == 0) {
        imageInfo.usage &= ~VK_IMAGE_USAGE_STORAGE_BIT;
    }
    if ((formatProperties.optimalTilingFeatures &
         VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) == 0) {
        imageInfo.usage &= ~VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }
    if ((formatProperties.optimalTilingFeatures &
         VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) == 0) {
        throw std::runtime_error(
            "Vulkan texture format does not support sampled images");
    }
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VULKAN_GUARD(
        vkCreateImage(deviceState.device, &imageInfo, nullptr, &state.image),
        "Failed to create Vulkan image");
    state.ownsImage = true;

    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(deviceState.device, state.image,
                                 &requirements);
    VkMemoryAllocateInfo allocationInfo{};
    allocationInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocationInfo.allocationSize = requirements.size;
    allocationInfo.memoryTypeIndex = vulkan::findMemoryType(
        deviceState.physicalDeviceInfo.device, requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VULKAN_GUARD(vkAllocateMemory(deviceState.device, &allocationInfo, nullptr,
                                  &state.memory),
                 "Failed to allocate Vulkan image memory");
    VULKAN_GUARD(
        vkBindImageMemory(deviceState.device, state.image, state.memory, 0),
        "Failed to bind Vulkan image memory");
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = state.image;
    viewInfo.viewType = imageViewType(texture->type);
    viewInfo.format = state.format;
    viewInfo.subresourceRange.aspectMask = state.aspectMask;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = state.mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = state.arrayLayers;
    VULKAN_GUARD(vkCreateImageView(deviceState.device, &viewInfo, nullptr,
                                   &state.imageView),
                 "Failed to create Vulkan image view");

    rebuildVulkanSampler(texture);
    VkCommandBuffer commandBuffer =
        vulkan::beginSingleTimeCommands(deviceState);
    VkImageLayout finalLayout = defaultVulkanTextureLayout(state);
    transitionVulkanImage(commandBuffer, state, VK_IMAGE_LAYOUT_UNDEFINED,
                          finalLayout, 0, state.mipLevels, 0,
                          state.arrayLayers);
    vulkan::endSingleTimeCommands(deviceState, commandBuffer);
    state.layout = finalLayout;
    if (state.type == TextureType::Texture2DMultisample) {
        state.generation++;
    }
}

void uploadVulkanTexture(Texture *texture, const void *data, int width,
                         int height, int depth, uint32_t arrayLayer,
                         TextureDataFormat dataFormat) {
    if (data == nullptr || width <= 0 || height <= 0 || depth <= 0) {
        return;
    }
    auto &deviceState = vulkan::deviceState(Device::globalInstance);
    auto &state = vulkan::textureState(texture);
    size_t texelCount = static_cast<size_t>(width) *
                        static_cast<size_t>(height) *
                        static_cast<size_t>(depth);
    VulkanUploadData upload =
        prepareVulkanUpload(data, texelCount, texture->format, dataFormat);
    VulkanTransferBuffer staging{};
    createTransferBuffer(deviceState, upload.bytes.size(),
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         staging);

    void *mapped = nullptr;
    VULKAN_GUARD(vkMapMemory(deviceState.device, staging.memory, 0,
                             upload.bytes.size(), 0, &mapped),
                 "Failed to map Vulkan texture upload memory");
    std::memcpy(mapped, upload.bytes.data(), upload.bytes.size());
    vkUnmapMemory(deviceState.device, staging.memory);

    VkCommandBuffer commandBuffer =
        vulkan::beginSingleTimeCommands(deviceState);
    transitionVulkanImage(commandBuffer, state, state.layout,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 1,
                          arrayLayer, 1);

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = transferAspect(state);
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = arrayLayer;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {static_cast<uint32_t>(width),
                          static_cast<uint32_t>(height),
                          static_cast<uint32_t>(depth)};
    vkCmdCopyBufferToImage(commandBuffer, staging.buffer, state.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    transitionVulkanImage(commandBuffer, state,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, state.layout, 0,
                          1, arrayLayer, 1);
    vulkan::endSingleTimeCommands(deviceState, commandBuffer);
    destroyTransferBuffer(deviceState, staging);
}

void readVulkanTexture(Texture *texture, void *output,
                       TextureDataFormat dataFormat) {
    if (output == nullptr) {
        return;
    }
    auto &deviceState = vulkan::deviceState(Device::globalInstance);
    auto &state = vulkan::textureState(texture);
    VkDeviceSize size = static_cast<VkDeviceSize>(state.width) * state.height *
                        state.depth * vulkan::bytesPerPixel(texture->format);
    VulkanTransferBuffer staging{};
    createTransferBuffer(deviceState, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         staging);

    VkCommandBuffer commandBuffer =
        vulkan::beginSingleTimeCommands(deviceState);
    transitionVulkanImage(commandBuffer, state, state.layout,
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 1, 0, 1);
    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = transferAspect(state);
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {state.width, state.height, state.depth};
    vkCmdCopyImageToBuffer(commandBuffer, state.image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging.buffer,
                           1, &region);
    transitionVulkanImage(commandBuffer, state,
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, state.layout, 0,
                          1, 0, 1);
    vulkan::endSingleTimeCommands(deviceState, commandBuffer);

    void *mapped = nullptr;
    VULKAN_GUARD(vkMapMemory(deviceState.device, staging.memory, 0, size, 0,
                             &mapped),
                 "Failed to map Vulkan texture readback memory");
    size_t texelCount = static_cast<size_t>(state.width) * state.height *
                        state.depth;
    size_t sourceChannels = textureFormatChannels(texture->format);
    size_t outputChannels = dataFormatChannels(dataFormat);
    size_t outputChannelSize = sourceChannelSize(texture->format, dataFormat);
    auto *destination = static_cast<uint8_t *>(output);
    const auto *source = static_cast<const uint8_t *>(mapped);
    size_t sourceTexelSize = vulkan::bytesPerPixel(texture->format);

    for (size_t texel = 0; texel < texelCount; ++texel) {
        float channels[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        const uint8_t *sourceTexel = source + texel * sourceTexelSize;
        if (texture->format == TextureFormat::Depth32F) {
            std::memcpy(channels, sourceTexel, sizeof(float));
        } else if (texture->format == TextureFormat::DepthComponent24 ||
                   texture->format == TextureFormat::Depth24Stencil8) {
            uint32_t packed = 0;
            std::memcpy(&packed, sourceTexel, sizeof(packed));
            uint32_t depth = texture->format == TextureFormat::DepthComponent24
                                 ? packed >> 8
                                 : packed & 0x00ffffffu;
            channels[0] = static_cast<float>(depth) / 16777215.0f;
        } else if (isHalfTextureFormat(texture->format)) {
            for (size_t channel = 0; channel < sourceChannels; ++channel) {
                uint16_t half = 0;
                std::memcpy(&half,
                            sourceTexel + channel * sizeof(uint16_t),
                            sizeof(half));
                channels[channel] = halfToFloat(half);
            }
        } else {
            for (size_t channel = 0; channel < sourceChannels; ++channel) {
                channels[channel] =
                    static_cast<float>(sourceTexel[channel]) / 255.0f;
            }
        }

        for (size_t channel = 0; channel < outputChannels; ++channel) {
            size_t destinationChannel = channel;
            if (dataFormat == TextureDataFormat::Bgr ||
                dataFormat == TextureDataFormat::Bgra) {
                if (channel == 0) {
                    destinationChannel = 2;
                } else if (channel == 2) {
                    destinationChannel = 0;
                }
            }
            uint8_t *outputChannel =
                destination +
                ((texel * outputChannels + destinationChannel) *
                 outputChannelSize);
            if (outputChannelSize == sizeof(float)) {
                std::memcpy(outputChannel, &channels[channel], sizeof(float));
            } else {
                *outputChannel = static_cast<uint8_t>(std::lround(
                    std::clamp(channels[channel], 0.0f, 1.0f) * 255.0f));
            }
        }
    }

    vkUnmapMemory(deviceState.device, staging.memory);
    destroyTransferBuffer(deviceState, staging);
}
#endif

} // namespace

std::shared_ptr<Texture> Texture::create(TextureType type, TextureFormat format,
                                         int width, int height,
                                         TextureDataFormat dataFormat,
                                         const void *data, uint mipLevels) {
    const auto emitCreated = [&](const std::shared_ptr<Texture> &texture) {
        detail::emit(ResourceEvent{
            "-1", ResourceType::Texture, ResourceOperation::Created,
            Device::globalInstance
                ? static_cast<unsigned int>(Device::globalInstance->frameCount)
                : 0,
            calculateTextureSizeMb(format, width, height)});
        return texture;
    };
#ifdef OPENGL
    auto texture = std::make_shared<Texture>();
    texture->type = type;
    texture->format = format;
    texture->width = width;
    texture->height = height;
    texture->mipLevels = type == TextureType::Texture2DMultisample
                             ? 1
                             : std::max<uint>(1, mipLevels);

    const GLenum textureType = getGLTextureType(type);
    const GLenum glFormat = getGLInternalFormat(format);
    const GLenum glDataFmt = getGLDataFormat(dataFormat);

    texture->glType = textureType;
    texture->glFormat = glFormat;

    // For single-channel textures, save and set unpack alignment to 1
    GLint previousAlignment = 4;
    bool needsAlignmentFix = (dataFormat == TextureDataFormat::Red);
    if (needsAlignmentFix) {
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousAlignment);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    }

    glGenTextures(1, &texture->textureID);
    glBindTexture(textureType, texture->textureID);

    if (textureType == GL_TEXTURE_2D && width > 0 && height > 0) {
        GLenum dataType = GL_UNSIGNED_BYTE;
        if (format == TextureFormat::Rgba16F ||
            format == TextureFormat::Rgb16F ||
            format == TextureFormat::Red16F) {
            dataType = GL_FLOAT;
        }
        glTexImage2D(textureType, 0, glFormat, width, height, 0, glDataFmt,
                     dataType, data);
        if (mipLevels > 1 && data != nullptr) {
            glGenerateMipmap(textureType);
        }
    } else if (textureType == GL_TEXTURE_CUBE_MAP && width > 0 && height > 0) {
        for (int i = 0; i < 6; ++i) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, glFormat, width,
                         height, 0, glDataFmt, GL_UNSIGNED_BYTE, nullptr);
        }
    }

    // Restore previous alignment
    if (needsAlignmentFix) {
        glPixelStorei(GL_UNPACK_ALIGNMENT, previousAlignment);
    }

    return emitCreated(texture);
#elif defined(METAL)
    if (Device::globalInstance == nullptr) {
        throw std::runtime_error("Cannot create Metal texture without device");
    }

    auto &deviceState = metal::deviceState(Device::globalInstance);
    if (deviceState.device == nullptr) {
        throw std::runtime_error("Metal device is not initialized");
    }

    auto texture = std::make_shared<Texture>();
    texture->type = type;
    texture->format = format;
    texture->width = width;
    texture->height = height;
    texture->mipLevels = type == TextureType::Texture2DMultisample
                             ? 1
                             : std::max<uint>(1, mipLevels);
    texture->samples = (type == TextureType::Texture2DMultisample)
                           ? static_cast<int>(mipLevels)
                           : 1;

    auto &state = metal::textureState(texture.get());
    state.type = type;
    state.format = format;
    state.dataFormat = dataFormat;
    state.width = width;
    state.height = height;
    state.depth = 1;
    state.samples = texture->samples;

    MTL::TextureDescriptor *descriptor =
        MTL::TextureDescriptor::alloc()->init();
    descriptor->setTextureType(metal::textureTypeToMetal(type));
    descriptor->setPixelFormat(metal::textureFormatToPixelFormat(format));
    descriptor->setWidth(static_cast<NS::UInteger>(std::max(width, 1)));
    descriptor->setHeight(static_cast<NS::UInteger>(std::max(height, 1)));
    descriptor->setDepth(1);
    descriptor->setMipmapLevelCount(
        static_cast<NS::UInteger>(texture->mipLevels));
    descriptor->setUsage(metal::textureUsageFor(type, format));
    descriptor->setStorageMode(MTL::StorageModeShared);

    if (type == TextureType::TextureCubeMap) {
        descriptor->setTextureType(MTL::TextureTypeCube);
        descriptor->setArrayLength(1);
    } else if (type == TextureType::Texture2DArray) {
        descriptor->setTextureType(MTL::TextureType2DArray);
        descriptor->setArrayLength(1);
    } else if (type == TextureType::Texture2DMultisample) {
        descriptor->setTextureType(MTL::TextureType2DMultisample);
        descriptor->setSampleCount(
            static_cast<NS::UInteger>(std::max(1, texture->samples)));
        descriptor->setMipmapLevelCount(1);
    }

    state.texture = deviceState.device->newTexture(descriptor);
    descriptor->release();
    if (state.texture == nullptr) {
        throw std::runtime_error("Failed to create Metal texture");
    }

    metal::rebuildTextureSampler(texture.get(), deviceState.device);
    state.handle = metal::registerTextureHandle(texture);
    texture->textureID = state.handle;

    if (data != nullptr && width > 0 && height > 0 &&
        type != TextureType::Texture2DMultisample &&
        type != TextureType::TextureCubeMap) {
        texture->updateData(data, width, height, dataFormat);
    }

    return emitCreated(texture);

#elif VULKAN
    if (Device::globalInstance == nullptr) {
        throw std::runtime_error("Cannot create Vulkan texture without device");
    }
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("Vulkan texture dimensions must be positive");
    }
    auto &deviceState = vulkan::deviceState(Device::globalInstance);
    if (deviceState.device == nullptr) {
        throw std::runtime_error("Vulkan device is not initialized");
    }

    auto texture = std::make_shared<Texture>();
    texture->type = type;
    texture->format = format;
    texture->width = width;
    texture->height = height;
    texture->mipLevels = type == TextureType::Texture2DMultisample
                             ? 1
                             : std::max<uint>(1, mipLevels);
    texture->samples = (type == TextureType::Texture2DMultisample)
                           ? static_cast<int>(mipLevels)
                           : 1;
    if (type != TextureType::Texture2DMultisample) {
        uint32_t maximumMipLevels =
            static_cast<uint32_t>(std::floor(std::log2(
                static_cast<double>(std::max(width, height))))) +
            1;
        texture->mipLevels =
            std::min<uint32_t>(texture->mipLevels, maximumMipLevels);
    }

    auto &state = vulkan::textureState(texture.get());
    state.type = type;
    state.dataFormat = dataFormat;
    state.opalFormat = format;
    createVulkanImage(texture.get(), 1);
    texture->textureID = vulkan::registerTextureHandle(texture);

    if (data != nullptr && type != TextureType::TextureCubeMap &&
        type != TextureType::Texture2DMultisample) {
        uploadVulkanTexture(texture.get(), data, width, height, 1, 0,
                            dataFormat);
        if (texture->mipLevels > 1) {
            texture->generateMipmaps(texture->mipLevels);
        }
    }

    return emitCreated(texture);
#else
    return nullptr;
#endif
}

void Texture::updateFace(int faceIndex, const void *data, int width, int height,
                         TextureDataFormat dataFormat) {
#ifdef OPENGL
    const GLenum glDataFmt = getGLDataFormat(dataFormat);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);
    if (this->width == width && this->height == height) {
        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + faceIndex, 0, 0, 0,
                        width, height, glDataFmt, GL_UNSIGNED_BYTE, data);
    } else {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + faceIndex, 0, glFormat,
                     width, height, 0, glDataFmt, GL_UNSIGNED_BYTE, data);
    }
#elif defined(METAL)
    auto &state = metal::textureState(this);
    if (state.texture == nullptr || data == nullptr || width <= 0 ||
        height <= 0 || type != TextureType::TextureCubeMap) {
        return;
    }

    MetalUploadBuffer upload =
        prepareMetalUpload(data, width, height, 1, format, dataFormat);
    if (upload.bytes == nullptr || upload.bytesPerRow == 0) {
        return;
    }

    MTL::Region region =
        MTL::Region::Make2D(0, 0, static_cast<NS::UInteger>(width),
                            static_cast<NS::UInteger>(height));
    state.texture->replaceRegion(
        region, 0, static_cast<NS::UInteger>(faceIndex), upload.bytes,
        upload.bytesPerRow, upload.bytesPerImage);
#elif VULKAN
    if (type != TextureType::TextureCubeMap || faceIndex < 0 || faceIndex >= 6 ||
        data == nullptr || width <= 0 || height <= 0) {
        return;
    }
    if (this->width != width || this->height != height) {
        this->width = width;
        this->height = height;
        createVulkanImage(this, 1);
    }
    vulkan::textureState(this).dataFormat = dataFormat;
    uploadVulkanTexture(this, data, width, height, 1,
                        static_cast<uint32_t>(faceIndex), dataFormat);
#endif

    detail::emit(ResourceEvent{
        "-1", ResourceType::Texture, ResourceOperation::Created,
        Device::globalInstance
            ? static_cast<unsigned int>(Device::globalInstance->frameCount)
            : 0,
        calculateTextureSizeMb(format, width, height)});
}

void Texture::updateData3D(const void *data, int width, int height, int depth,
                           TextureDataFormat dataFormat) {
#ifdef OPENGL
    const GLenum glDataFmt = getGLDataFormat(dataFormat);
    glBindTexture(GL_TEXTURE_3D, textureID);
    glTexImage3D(GL_TEXTURE_3D, 0, glFormat, width, height, depth, 0, glDataFmt,
                 GL_UNSIGNED_BYTE, data);
#elif defined(METAL)
    auto &state = metal::textureState(this);
    if (state.texture == nullptr || data == nullptr || width <= 0 ||
        height <= 0 || depth <= 0) {
        return;
    }

    MetalUploadBuffer upload =
        prepareMetalUpload(data, width, height, depth, format, dataFormat);
    if (upload.bytes == nullptr || upload.bytesPerRow == 0 ||
        upload.bytesPerImage == 0) {
        return;
    }

    MTL::Region region = MTL::Region::Make3D(
        0, 0, 0, static_cast<NS::UInteger>(width),
        static_cast<NS::UInteger>(height), static_cast<NS::UInteger>(depth));
    state.texture->replaceRegion(region, 0, 0, upload.bytes, upload.bytesPerRow,
                                 upload.bytesPerImage);
    state.depth = depth;
#elif VULKAN
    if (type != TextureType::Texture3D || data == nullptr || width <= 0 ||
        height <= 0 || depth <= 0) {
        return;
    }
    auto &state = vulkan::textureState(this);
    if (this->width != width || this->height != height ||
        state.depth != static_cast<uint32_t>(depth)) {
        this->width = width;
        this->height = height;
        createVulkanImage(this, depth);
    }
    state.dataFormat = dataFormat;
    uploadVulkanTexture(this, data, width, height, depth, 0, dataFormat);
#endif
}

#ifdef METAL
MTL::Texture *Texture::getMetalTexture() const {
    return metal::textureState(const_cast<Texture *>(this)).texture;
}
#endif

void Texture::updateData(const void *data, int width, int height,
                         TextureDataFormat dataFormat) {
#ifdef OPENGL
    const GLenum glDataFmt = getGLDataFormat(dataFormat);
    GLenum dataType = GL_UNSIGNED_BYTE;
    if (format == TextureFormat::Rgba16F || format == TextureFormat::Rgb16F ||
        format == TextureFormat::Red16F) {
        dataType = GL_FLOAT;
    }
    glBindTexture(GL_TEXTURE_2D, textureID);
    if (this->width == width && this->height == height) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, glDataFmt,
                        dataType, data);
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, glFormat, width, height, 0, glDataFmt,
                     dataType, data);
        this->width = width;
        this->height = height;
    }
#elif defined(METAL)
    auto &state = metal::textureState(this);
    if (state.texture == nullptr || data == nullptr || width <= 0 ||
        height <= 0) {
        return;
    }

    MetalUploadBuffer upload =
        prepareMetalUpload(data, width, height, 1, format, dataFormat);
    if (upload.bytes == nullptr || upload.bytesPerRow == 0) {
        return;
    }

    MTL::Region region =
        MTL::Region::Make2D(0, 0, static_cast<NS::UInteger>(width),
                            static_cast<NS::UInteger>(height));
    state.texture->replaceRegion(region, 0, upload.bytes, upload.bytesPerRow);
    this->width = width;
    this->height = height;
    state.width = width;
    state.height = height;
#elif VULKAN
    if (data == nullptr || width <= 0 || height <= 0 ||
        type == TextureType::TextureCubeMap ||
        type == TextureType::Texture2DMultisample ||
        type == TextureType::Texture3D) {
        return;
    }
    if (this->width != width || this->height != height) {
        this->width = width;
        this->height = height;
        createVulkanImage(this, 1);
    }
    vulkan::textureState(this).dataFormat = dataFormat;
    uploadVulkanTexture(this, data, width, height, 1, 0, dataFormat);
#endif
}

void Texture::changeFormat(TextureFormat newFormat) {
#ifdef OPENGL
    this->format = newFormat;
    this->glFormat = getGLInternalFormat(newFormat);
#elif defined(METAL)
    this->format = newFormat;
    metal::textureState(this).format = newFormat;
#elif VULKAN
    if (format == newFormat) {
        return;
    }
    int depth = static_cast<int>(vulkan::textureState(this).depth);
    format = newFormat;
    createVulkanImage(this, depth);
#endif
}

void Texture::readData(void *buffer, TextureDataFormat dataFormat) {
#ifdef OPENGL
    glBindTexture(this->glType, textureID);
    GLenum glDataFormat;
    GLenum glDataType = GL_UNSIGNED_BYTE;
    switch (dataFormat) {
    case TextureDataFormat::Rgba:
        glDataFormat = GL_RGBA;
        break;
    case TextureDataFormat::Rgb:
        glDataFormat = GL_RGB;
        break;
    case TextureDataFormat::Red:
        glDataFormat = GL_RED;
        break;
    case TextureDataFormat::DepthComponent:
        glDataFormat = GL_DEPTH_COMPONENT;
        glDataType = GL_FLOAT;
        break;
    default:
        glDataFormat = GL_RGBA;
        break;
    }
    glGetTexImage(this->glType, 0, glDataFormat, glDataType, buffer);
#elif defined(METAL)
    auto &state = metal::textureState(this);
    if (state.texture == nullptr || buffer == nullptr || width <= 0 ||
        height <= 0) {
        return;
    }

    size_t bytesPerPixel = 4;
    if (dataFormat == TextureDataFormat::Rgb) {
        bytesPerPixel = 3;
    } else if (dataFormat == TextureDataFormat::Red) {
        bytesPerPixel = 1;
    } else if (dataFormat == TextureDataFormat::DepthComponent) {
        bytesPerPixel = 4;
    }

    MTL::Region region =
        MTL::Region::Make2D(0, 0, static_cast<NS::UInteger>(width),
                            static_cast<NS::UInteger>(height));
    size_t rowBytes = static_cast<size_t>(width) * bytesPerPixel;
    NS::UInteger bytesPerRow = static_cast<NS::UInteger>(rowBytes);
    state.texture->getBytes(buffer, bytesPerRow, region, 0);
#elif VULKAN
    readVulkanTexture(this, buffer, dataFormat);
#endif
}

void Texture::generateMipmaps([[maybe_unused]] uint levels) {
#ifdef OPENGL
    glBindTexture(this->glType, textureID);
    glGenerateMipmap(this->glType);
#elif defined(METAL)
    if (Device::globalInstance == nullptr) {
        return;
    }
    auto &deviceState = metal::deviceState(Device::globalInstance);
    auto &state = metal::textureState(this);
    if (deviceState.queue == nullptr || state.texture == nullptr ||
        state.texture->mipmapLevelCount() <= 1) {
        return;
    }
    MTL::CommandBuffer *commandBuffer = deviceState.queue->commandBuffer();
    MTL::BlitCommandEncoder *blit = commandBuffer->blitCommandEncoder();
    blit->generateMipmaps(state.texture);
    blit->endEncoding();
    commandBuffer->commit();
#elif VULKAN
    auto &deviceState = vulkan::deviceState(Device::globalInstance);
    auto &state = vulkan::textureState(this);
    if (state.image == VK_NULL_HANDLE || state.mipLevels <= 1 ||
        state.sampleCount != VK_SAMPLE_COUNT_1_BIT ||
        isDepthTextureFormat(format)) {
        return;
    }

    uint32_t mipCount = levels == 0
                            ? state.mipLevels
                            : std::min<uint32_t>(levels, state.mipLevels);
    if (mipCount <= 1) {
        return;
    }

    VkFormatProperties properties{};
    vkGetPhysicalDeviceFormatProperties(deviceState.physicalDeviceInfo.device,
                                        state.format, &properties);
    if ((properties.optimalTilingFeatures &
         VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) == 0) {
        throw std::runtime_error(
            "Vulkan texture format does not support linear mipmap generation");
    }

    VkCommandBuffer commandBuffer =
        vulkan::beginSingleTimeCommands(deviceState);
    int32_t mipWidth = static_cast<int32_t>(state.width);
    int32_t mipHeight = static_cast<int32_t>(state.height);
    int32_t mipDepth = static_cast<int32_t>(state.depth);
    for (uint32_t mipLevel = 1; mipLevel < mipCount; ++mipLevel) {
        transitionVulkanImage(commandBuffer, state, state.layout,
                              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                              mipLevel - 1, 1, 0, state.arrayLayers);
        transitionVulkanImage(commandBuffer, state, state.layout,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevel, 1,
                              0, state.arrayLayers);

        VkImageBlit blit{};
        blit.srcSubresource.aspectMask = state.aspectMask;
        blit.srcSubresource.mipLevel = mipLevel - 1;
        blit.srcSubresource.layerCount = state.arrayLayers;
        blit.srcOffsets[1] = {mipWidth, mipHeight, mipDepth};
        blit.dstSubresource.aspectMask = state.aspectMask;
        blit.dstSubresource.mipLevel = mipLevel;
        blit.dstSubresource.layerCount = state.arrayLayers;
        blit.dstOffsets[1] = {std::max(mipWidth / 2, 1),
                              std::max(mipHeight / 2, 1),
                              std::max(mipDepth / 2, 1)};
        vkCmdBlitImage(commandBuffer, state.image,
                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, state.image,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                       VK_FILTER_LINEAR);

        transitionVulkanImage(commandBuffer, state,
                              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                              state.layout, mipLevel - 1, 1, 0,
                              state.arrayLayers);
        transitionVulkanImage(commandBuffer, state,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, state.layout,
                              mipLevel, 1, 0, state.arrayLayers);
        mipWidth = std::max(mipWidth / 2, 1);
        mipHeight = std::max(mipHeight / 2, 1);
        mipDepth = std::max(mipDepth / 2, 1);
    }
    vulkan::endSingleTimeCommands(deviceState, commandBuffer);
#endif
}

void Texture::automaticallyGenerateMipmaps() {
#ifdef OPENGL
    glBindTexture(this->glType, textureID);
    glGenerateMipmap(this->glType);
#elif defined(METAL)
    generateMipmaps(0);
#elif VULKAN
    generateMipmaps(0);
#endif
}

void Texture::setWrapMode(TextureAxis axis, TextureWrapMode mode) {
#ifdef OPENGL
    glBindTexture(this->glType, textureID);
    const GLenum glMode = getGLWrapMode(mode);
    static constexpr GLenum axisTable[] = {GL_TEXTURE_WRAP_S, GL_TEXTURE_WRAP_T,
                                           GL_TEXTURE_WRAP_R};
    glTexParameteri(this->glType, axisTable[static_cast<int>(axis)], glMode);
#elif defined(METAL)
    auto &state = metal::textureState(this);
    if (axis == TextureAxis::S) {
        state.wrapS = mode;
    } else if (axis == TextureAxis::T) {
        state.wrapT = mode;
    } else {
        state.wrapR = mode;
    }
    if (Device::globalInstance != nullptr) {
        auto &deviceState = metal::deviceState(Device::globalInstance);
        metal::rebuildTextureSampler(this, deviceState.device);
    }
#elif VULKAN
    auto &state = vulkan::textureState(this);
    if (axis == TextureAxis::S) {
        state.wrapS = mode;
    } else if (axis == TextureAxis::T) {
        state.wrapT = mode;
    } else {
        state.wrapR = mode;
    }
    rebuildVulkanSampler(this);
#endif
}

void Texture::changeBorderColor(const glm::vec4 &borderColor) {
#ifdef OPENGL
    glBindTexture(this->glType, textureID);
    GLfloat color[4] = {borderColor.r, borderColor.g, borderColor.b,
                        borderColor.a};
    glTexParameterfv(this->glType, GL_TEXTURE_BORDER_COLOR, color);
#elif defined(METAL)
    auto &state = metal::textureState(this);
    state.borderColor = borderColor;
    if (Device::globalInstance != nullptr) {
        auto &deviceState = metal::deviceState(Device::globalInstance);
        metal::rebuildTextureSampler(this, deviceState.device);
    }
#elif VULKAN
    vulkan::textureState(this).borderColor = borderColor;
    rebuildVulkanSampler(this);
#endif
}

void Texture::setFilterMode(TextureFilterMode minFilter,
                            TextureFilterMode magFilter) {
#ifdef OPENGL
    glBindTexture(this->glType, textureID);
    glTexParameteri(glType, GL_TEXTURE_MIN_FILTER, getGLFilterMode(minFilter));
    glTexParameteri(glType, GL_TEXTURE_MAG_FILTER, getGLFilterMode(magFilter));
#elif defined(METAL)
    auto &state = metal::textureState(this);
    state.minFilter = minFilter;
    state.magFilter = magFilter;
    if (Device::globalInstance != nullptr) {
        auto &deviceState = metal::deviceState(Device::globalInstance);
        metal::rebuildTextureSampler(this, deviceState.device);
    }
#elif VULKAN
    auto &state = vulkan::textureState(this);
    state.minFilter = minFilter;
    state.magFilter = magFilter;
    rebuildVulkanSampler(this);
#endif
}

void Texture::setParameters(TextureWrapMode wrapS, TextureWrapMode wrapT,
                            TextureFilterMode minFilter,
                            TextureFilterMode magFilter) {
#ifdef OPENGL
    glBindTexture(this->glType, textureID);
    glTexParameteri(glType, GL_TEXTURE_WRAP_S, getGLWrapMode(wrapS));
    glTexParameteri(glType, GL_TEXTURE_WRAP_T, getGLWrapMode(wrapT));
    glTexParameteri(glType, GL_TEXTURE_MIN_FILTER, getGLFilterMode(minFilter));
    glTexParameteri(glType, GL_TEXTURE_MAG_FILTER, getGLFilterMode(magFilter));
#elif defined(METAL)
    auto &state = metal::textureState(this);
    state.wrapS = wrapS;
    state.wrapT = wrapT;
    state.minFilter = minFilter;
    state.magFilter = magFilter;
    if (Device::globalInstance != nullptr) {
        auto &deviceState = metal::deviceState(Device::globalInstance);
        metal::rebuildTextureSampler(this, deviceState.device);
    }
#elif VULKAN
    auto &state = vulkan::textureState(this);
    state.wrapS = wrapS;
    state.wrapT = wrapT;
    state.minFilter = minFilter;
    state.magFilter = magFilter;
    rebuildVulkanSampler(this);
#endif
}

void Texture::setParameters3D(TextureWrapMode wrapS, TextureWrapMode wrapT,
                              TextureWrapMode wrapR,
                              TextureFilterMode minFilter,
                              TextureFilterMode magFilter) {
#ifdef OPENGL
    glBindTexture(this->glType, textureID);
    glTexParameteri(glType, GL_TEXTURE_WRAP_S, getGLWrapMode(wrapS));
    glTexParameteri(glType, GL_TEXTURE_WRAP_T, getGLWrapMode(wrapT));
    glTexParameteri(glType, GL_TEXTURE_WRAP_R, getGLWrapMode(wrapR));
    glTexParameteri(glType, GL_TEXTURE_MIN_FILTER, getGLFilterMode(minFilter));
    glTexParameteri(glType, GL_TEXTURE_MAG_FILTER, getGLFilterMode(magFilter));
#elif defined(METAL)
    auto &state = metal::textureState(this);
    state.wrapS = wrapS;
    state.wrapT = wrapT;
    state.wrapR = wrapR;
    state.minFilter = minFilter;
    state.magFilter = magFilter;
    if (Device::globalInstance != nullptr) {
        auto &deviceState = metal::deviceState(Device::globalInstance);
        metal::rebuildTextureSampler(this, deviceState.device);
    }
#elif VULKAN
    auto &state = vulkan::textureState(this);
    state.wrapS = wrapS;
    state.wrapT = wrapT;
    state.wrapR = wrapR;
    state.minFilter = minFilter;
    state.magFilter = magFilter;
    rebuildVulkanSampler(this);
#endif
}

void Pipeline::bindTexture(const std::string &name,
                           const std::shared_ptr<Texture> &texture,
                           [[maybe_unused]] int unit, int callerId) {
#ifdef OPENGL
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(texture->glType, texture->textureID);
    int location = glGetUniformLocation(shaderProgram->programID, name.c_str());
    glUniform1i(location, unit);
#elif defined(METAL)
    auto &state = metal::pipelineState(this);
    int resolvedUnit = unit;
    if (shaderProgram != nullptr) {
        auto &programState = metal::programState(shaderProgram.get());
        auto bindingIt = programState.textureBindings.find(name);
        if (bindingIt != programState.textureBindings.end()) {
            resolvedUnit = bindingIt->second;
        }
    }
    if (texture == nullptr) {
        state.texturesByUnit.erase(resolvedUnit);
        return;
    }
    state.texturesByUnit[resolvedUnit] = texture;
#elif VULKAN
    if (shaderProgram == nullptr) {
        throw std::runtime_error(
            "bindTexture requires a Vulkan shader program");
    }
    auto &programState = vulkan::programState(shaderProgram.get());
    auto bindingIt = programState.bindingsByName.find(name);
    if (bindingIt == programState.bindingsByName.end()) {
        throw std::runtime_error("Vulkan texture binding not found: " + name);
    }
    const auto &binding = bindingIt->second;
    if (binding.type != vulkan::ShaderResourceType::CombinedImageSampler &&
        binding.type != vulkan::ShaderResourceType::SampledImage &&
        binding.type != vulkan::ShaderResourceType::Sampler &&
        binding.type != vulkan::ShaderResourceType::StorageImage) {
        throw std::runtime_error(
            "bindTexture requires an image or sampler binding");
    }
    uint32_t arrayElement = 0;
    if (binding.count > 1) {
        if (unit < 0 || static_cast<uint32_t>(unit) >= binding.count) {
            throw std::runtime_error(
                "Vulkan texture descriptor array index is out of range");
        }
        arrayElement = static_cast<uint32_t>(unit);
    }
    auto &pipelineState = vulkan::pipelineState(this);
    uint64_t key = vulkan::bindingKey(binding.set, binding.binding);
    auto &images = pipelineState.boundImages[key].textures;
    images.resize(binding.count);
    images[arrayElement] = texture;
    if (texture == nullptr &&
        std::none_of(images.begin(), images.end(),
                     [](const std::shared_ptr<Texture> &item) {
                         return item != nullptr;
                     })) {
        pipelineState.boundImages.erase(key);
    }
    pipelineState.descriptorsDirty = true;
#endif

    if (texture == nullptr) {
        return;
    }

    detail::emit(ResourceEvent{
        std::to_string(callerId), ResourceType::Texture,
        ResourceOperation::Loaded,
        Device::globalInstance
            ? static_cast<unsigned int>(Device::globalInstance->frameCount)
            : 0,
        calculateTextureSizeMb(texture->format, texture->width,
                               texture->height)});
}

#ifdef METAL
void Pipeline::bindTextureArray(
    const std::vector<std::shared_ptr<Texture>> &textures,
    uint32_t bufferIndex) {
    if (shaderProgram == nullptr || Device::globalInstance == nullptr) {
        return;
    }
    auto &state = metal::pipelineState(this);
    if (state.textureArgumentBuffer != nullptr &&
        state.textureArgumentBufferIndex == bufferIndex &&
        state.textureArgumentTextures == textures) {
        return;
    }
    auto &programState = metal::programState(shaderProgram.get());
    if (programState.computeFunction == nullptr) {
        return;
    }
    if (state.textureArgumentBuffer != nullptr) {
        state.textureArgumentBuffer->release();
        state.textureArgumentBuffer = nullptr;
    }
    if (state.textureArgumentEncoder != nullptr) {
        state.textureArgumentEncoder->release();
    }
    state.textureArgumentEncoder =
        programState.computeFunction->newArgumentEncoder(bufferIndex);
    if (state.textureArgumentEncoder == nullptr) {
        return;
    }
    auto &deviceState = metal::deviceState(Device::globalInstance);
    state.textureArgumentBuffer = deviceState.device->newBuffer(
        state.textureArgumentEncoder->encodedLength(),
        MTL::ResourceStorageModeShared);
    state.textureArgumentEncoder->setArgumentBuffer(state.textureArgumentBuffer,
                                                    0);
    for (size_t i = 0; i < textures.size(); ++i) {
        if (textures[i] == nullptr) {
            continue;
        }
        auto &textureState = metal::textureState(textures[i].get());
        state.textureArgumentEncoder->setTexture(textureState.texture, i);
    }
    state.textureArgumentBufferIndex = bufferIndex;
    state.textureArgumentTextures = textures;
}
#endif

void Pipeline::bindTexture2D(const std::string &name, uint textureId, int unit,
                             int callerId) {
#ifdef OPENGL
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, textureId);
    int location = glGetUniformLocation(shaderProgram->programID, name.c_str());
    glUniform1i(location, unit);
    (void)callerId;
#elif defined(METAL)
    auto texture = metal::getTextureFromHandle(textureId);
    bindTexture(name, texture, unit, callerId);
#elif VULKAN
    auto texture = vulkan::getTextureFromHandle(textureId);
    bindTexture(name, texture, unit, callerId);
#endif
}

void Pipeline::bindTexture3D(const std::string &name, uint textureId, int unit,
                             int callerId) {
#ifdef OPENGL
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_3D, textureId);
    int location = glGetUniformLocation(shaderProgram->programID, name.c_str());
    glUniform1i(location, unit);
    (void)callerId;

#elif defined(METAL)
    auto texture = metal::getTextureFromHandle(textureId);
    bindTexture(name, texture, unit, callerId);
#elif VULKAN
    auto texture = vulkan::getTextureFromHandle(textureId);
    bindTexture(name, texture, unit, callerId);
#endif
}

void Pipeline::bindTextureCubemap(const std::string &name, uint textureId,
                                  int unit, int callerId) {
#ifdef OPENGL
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureId);
    int location = glGetUniformLocation(shaderProgram->programID, name.c_str());
    glUniform1i(location, unit);
    (void)callerId;
#elif defined(METAL)
    auto texture = metal::getTextureFromHandle(textureId);
    bindTexture(name, texture, unit, callerId);
#elif VULKAN
    auto texture = vulkan::getTextureFromHandle(textureId);
    bindTexture(name, texture, unit, callerId);
#endif
}

std::shared_ptr<Texture> Texture::createMultisampled(TextureFormat format,
                                                     int width, int height,
                                                     int samples) {
#ifdef OPENGL
    auto texture = std::make_shared<Texture>();
    texture->type = TextureType::Texture2DMultisample;
    texture->format = format;
    texture->width = width;
    uint glFormat = getGLInternalFormat(format);
    texture->glFormat = glFormat;

    glGenTextures(1, &texture->textureID);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, texture->textureID);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, glFormat, width,
                            height, GL_TRUE);

    return texture;
#elif defined(METAL)
    return Texture::create(TextureType::Texture2DMultisample, format, width,
                           height, TextureDataFormat::Rgba, nullptr,
                           static_cast<uint>(samples));
#elif VULKAN
    return Texture::create(TextureType::Texture2DMultisample, format, width,
                           height, TextureDataFormat::Rgba, nullptr,
                           static_cast<uint>(samples));
#else
    return nullptr;
#endif
}

std::shared_ptr<Texture> Texture::createDepthCubemap(TextureFormat format,
                                                     int resolution) {
#ifdef OPENGL
    auto texture = std::make_shared<Texture>();
    texture->type = TextureType::TextureCubeMap;
    texture->format = format;
    texture->width = resolution;
    texture->height = resolution;

    const GLenum glFormat = getGLInternalFormat(format);
    texture->glType = GL_TEXTURE_CUBE_MAP;
    texture->glFormat = glFormat;

    glGenTextures(1, &texture->textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture->textureID);

    for (unsigned int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, glFormat,
                     resolution, resolution, 0, GL_DEPTH_COMPONENT, GL_FLOAT,
                     nullptr);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return texture;
#elif defined(METAL)
    return Texture::create(TextureType::TextureCubeMap, format, resolution,
                           resolution, TextureDataFormat::DepthComponent,
                           nullptr, 1);
#elif VULKAN
    auto texture = Texture::create(TextureType::TextureCubeMap, format,
                                   resolution, resolution,
                                   TextureDataFormat::DepthComponent, nullptr,
                                   1);
    texture->setParameters3D(TextureWrapMode::ClampToEdge,
                             TextureWrapMode::ClampToEdge,
                             TextureWrapMode::ClampToEdge,
                             TextureFilterMode::Nearest,
                             TextureFilterMode::Nearest);
    return texture;
#else
    return nullptr;
#endif
}

std::shared_ptr<Texture> Texture::create3D(TextureFormat format, int width,
                                           int height, int depth,
                                           TextureDataFormat dataFormat,
                                           const void *data) {
#ifdef OPENGL
    auto texture = std::make_shared<Texture>();
    texture->type = TextureType::Texture3D;
    texture->format = format;
    texture->width = width;
    texture->height = height;

    const GLenum glFormat = getGLInternalFormat(format);
    const GLenum glDataFmt = getGLDataFormat(dataFormat);
    texture->glType = GL_TEXTURE_3D;
    texture->glFormat = glFormat;

    // Determine data type based on format
    GLenum dataType = GL_UNSIGNED_BYTE;
    if (format == TextureFormat::Rgba16F || format == TextureFormat::Rgb16F ||
        format == TextureFormat::Red16F) {
        dataType = GL_FLOAT;
    }

    // Save and restore previous state
    GLint previousAlignment = 0;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousAlignment);
    GLint previousTexture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_3D, &previousTexture);

    glGenTextures(1, &texture->textureID);
    glBindTexture(GL_TEXTURE_3D, texture->textureID);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage3D(GL_TEXTURE_3D, 0, glFormat, width, height, depth, 0, glDataFmt,
                 dataType, data);

    // Restore previous state
    glBindTexture(GL_TEXTURE_3D, previousTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, previousAlignment);

    return texture;
#elif defined(METAL)
    if (Device::globalInstance == nullptr) {
        throw std::runtime_error(
            "Cannot create Metal 3D texture without device");
    }
    auto &deviceState = metal::deviceState(Device::globalInstance);
    if (deviceState.device == nullptr) {
        throw std::runtime_error("Metal device is not initialized");
    }

    auto texture = std::make_shared<Texture>();
    texture->type = TextureType::Texture3D;
    texture->format = format;
    texture->width = width;
    texture->height = height;
    texture->samples = 1;

    auto &state = metal::textureState(texture.get());
    state.type = TextureType::Texture3D;
    state.format = format;
    state.dataFormat = dataFormat;
    state.width = width;
    state.height = height;
    state.depth = depth;
    state.samples = 1;

    MTL::TextureDescriptor *descriptor =
        MTL::TextureDescriptor::alloc()->init();
    descriptor->setTextureType(MTL::TextureType3D);
    descriptor->setPixelFormat(metal::textureFormatToPixelFormat(format));
    descriptor->setWidth(static_cast<NS::UInteger>(std::max(width, 1)));
    descriptor->setHeight(static_cast<NS::UInteger>(std::max(height, 1)));
    descriptor->setDepth(static_cast<NS::UInteger>(std::max(depth, 1)));
    descriptor->setMipmapLevelCount(1);
    descriptor->setUsage(
        metal::textureUsageFor(TextureType::Texture3D, format));
    descriptor->setStorageMode(MTL::StorageModeShared);

    state.texture = deviceState.device->newTexture(descriptor);
    descriptor->release();
    if (state.texture == nullptr) {
        throw std::runtime_error("Failed to create Metal 3D texture");
    }

    metal::rebuildTextureSampler(texture.get(), deviceState.device);
    state.handle = metal::registerTextureHandle(texture);
    texture->textureID = state.handle;

    if (data != nullptr) {
        texture->updateData3D(data, width, height, depth, dataFormat);
    }

    return texture;
#elif VULKAN
    if (Device::globalInstance == nullptr) {
        throw std::runtime_error(
            "Cannot create Vulkan 3D texture without device");
    }
    auto &deviceState = vulkan::deviceState(Device::globalInstance);
    if (deviceState.device == VK_NULL_HANDLE) {
        throw std::runtime_error("Vulkan device is not initialized");
    }
    if (width <= 0 || height <= 0 || depth <= 0) {
        throw std::runtime_error(
            "Vulkan 3D texture dimensions must be positive");
    }

    auto texture = std::make_shared<Texture>();
    texture->type = TextureType::Texture3D;
    texture->format = format;
    texture->width = width;
    texture->height = height;
    texture->mipLevels = 1;
    texture->samples = 1;

    auto &state = vulkan::textureState(texture.get());
    state.type = TextureType::Texture3D;
    state.opalFormat = format;
    state.dataFormat = dataFormat;
    createVulkanImage(texture.get(), depth);
    texture->textureID = vulkan::registerTextureHandle(texture);
    if (data != nullptr) {
        uploadVulkanTexture(texture.get(), data, width, height, depth, 0,
                            dataFormat);
    }
    return texture;
#else
    return nullptr;
#endif
}

} // namespace opal
