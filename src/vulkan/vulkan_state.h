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

#include <cwchar>
#include <vector>
#ifdef VULKAN

#include "opal/opal.h"
#include "slang-com-ptr.h"
#include <slang.h>
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

struct SlangCompilerState {
    Slang::ComPtr<slang::IGlobalSession> globalSession;
    Slang::ComPtr<slang::ISession> session;

    SlangCompilerState();
};

struct ContextState {
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;

    VkFormat swapchainImageFormat = VK_FORMAT_UNDEFINED;
    VkColorSpaceKHR swapchainColorSpace{};
    VkPresentModeKHR swapchainPresentMode{};
    VkExtent2D swapchainExtent{};

    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;

    uint32_t currentSwapchainImageIndex = UINT32_MAX;

    std::vector<VkImageLayout> swapchainImageLayouts;
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
};

struct CommandBufferState {
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

    bool recording = false;
    bool rendering = false;

    bool needsPresent = false;

    VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence inFlightFence = VK_NULL_HANDLE;

    bool clearColorPending = false;
    bool clearDepthPending = false;

    uint32_t imageIndex = UINT32_MAX;
};

struct FramebufferState {
    bool dirty = false;

    int viewportX = 0;
    int viewportY = 0;

    int cubemapFace = -1;
};

struct TextureState {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;

    VkFormat format = VK_FORMAT_UNDEFINED;

    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImageAspectFlags aspectMask = 0;

    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;

    TextureType type = TextureType::Texture2D;
    TextureDataFormat dataFormat = TextureDataFormat::Rgba;
    TextureFormat opalFormat = TextureFormat::Rgba8;

    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 1;

    uint32_t mipLevels = 1;
    uint32_t arrayLayers = 1;
    uint32_t handle = 0;

    TextureWrapMode wrapS = TextureWrapMode::Repeat;
    TextureWrapMode wrapT = TextureWrapMode::Repeat;
    TextureWrapMode wrapR = TextureWrapMode::Repeat;
    TextureFilterMode minFilter = TextureFilterMode::Linear;
    TextureFilterMode magFilter = TextureFilterMode::Linear;
    glm::vec4 borderColor = glm::vec4(0.0f);

    bool ownsImage = false;
};

struct ShaderState {
    VkShaderModule shaderModule = VK_NULL_HANDLE;
    VkShaderStageFlagBits stage{};
    std::string entryPoint = "main";

    std::vector<uint32_t> spirv;

    bool compiled = false;
    std::string log;
};

enum class ShaderResourceType {
    UniformBuffer,
    StorageBuffer,
    CombinedImageSampler,
    SampledImage,
    Sampler,
    StorageImage
};

struct ShaderBinding {
    uint32_t set = 0;
    uint32_t binding = 0;

    std::string name;

    ShaderResourceType type = ShaderResourceType::UniformBuffer;

    uint32_t count = 1;

    VkShaderStageFlags stages = 0;
};

struct UniformMember {
    std::string name;

    uint32_t set = 0;
    uint32_t binding = 0;

    size_t offset = 0;
    size_t size = 0;
};

struct UniformBlockReflection {
    std::string name;

    uint32_t set = 0;
    uint32_t binding = 0;

    size_t size = 0;

    std::vector<UniformMember> members;
};

struct ProgramState {
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

    std::vector<ShaderBinding> bindings;

    std::unordered_map<std::string, ShaderBinding> bindingsByName;

    std::unordered_map<std::string, UniformMember> uniformsByName;
    std::vector<UniformBlockReflection> uniformBlocks;

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

    bool computeProgram = false;

    uint32_t fragmentColorOutputs = 1;

    bool linked = false;

    std::string log;
};

struct VulkanUniformLocation {
    uint32_t set = 0;
    uint32_t binding = 0;

    uint32_t offset = 0;
    uint32_t size = 0;
};

struct VulkanUniformBlock {
    uint32_t set = 0;
    uint32_t binding = 0;

    uint32_t size = 0;

    std::vector<uint8_t> data;

    std::vector<std::shared_ptr<Buffer>> buffers;

    bool dirty = false;
};

struct PipelineRenderTargetKey {
    std::vector<VkFormat> colorFormats;

    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    VkFormat stencilFormat = VK_FORMAT_UNDEFINED;

    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;

    uint32_t viewMask = 0;

    bool operator==(const PipelineRenderTargetKey &other) const = default;
};

struct BoundBufferResource {
    std::shared_ptr<Buffer> buffer;

    VkDeviceSize offset = 0;
    VkDeviceSize range = VK_WHOLE_SIZE;
};

struct PipelineState {
    VkPipeline computePipeline = VK_NULL_HANDLE;

    std::unordered_map<PipelineRenderTargetKey, VkPipeline> graphicsPipelines;

    VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
    VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
    VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    VkCompareOp depthCompare = VK_COMPARE_OP_LESS;

    std::vector<VkVertexInputBindingDescription> vertexBindings;
    std::vector<VkVertexInputAttributeDescription> vertexAttributes;

    std::unordered_map<std::string, VulkanUniformBlock> uniformsByName;
    std::unordered_map<uint64_t, VulkanUniformBlock> uniformBlocks;
    std::unordered_map<uint64_t, BoundBufferResource> boundBuffers;

    std::vector<std::vector<VkDescriptorSet>> descriptorSets;

    bool descriptorsDirty = false;
    bool pipelineDirty = false;

    bool built = false;
};

SlangCompilerState &slangCompiler();
ContextState &contextState(Context *context);
DeviceState &deviceState(Device *device);
CommandBufferState &commandBufferState(CommandBuffer *commandBuffer);
FramebufferState &framebufferState(Framebuffer *framebuffer);
TextureState &textureState(Texture *texture);
ShaderState &shaderState(Shader *shader);
ProgramState &programState(ShaderProgram *program);
PipelineState &pipelineState(Pipeline *pipeline);

uint32_t registerTextureHandle(const std::shared_ptr<Texture> &texture);
std::shared_ptr<Texture> getTextureFromHandle(uint32_t handle);

void releaseContextState(Context *context);
void releaseDeviceState(Device *device);
void releaseCommandBufferState(CommandBuffer *commandBuffer);
void releaseFramebufferState(Framebuffer *framebuffer);
void releaseTextureState(Texture *texture);
void releaseShaderState(Shader *shader);
void releaseProgramState(ShaderProgram *program);
void releasePipelineState(Pipeline *pipeline);

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
VkCommandBuffer beginSingleTimeCommands(DeviceState &deviceState);
void endSingleTimeCommands(DeviceState &deviceState,
                           VkCommandBuffer commandBuffer);

void createSwapchain(ContextState &contextState, DeviceState &deviceState,
                     uint32_t width, uint32_t height);
void createSwapchainImages(ContextState &contextState,
                           DeviceState &deviceState);

VkFormat textureFormatToVkFormat(TextureFormat format);
VkImageType textureTypeToVk(TextureType type);
VkImageAspectFlags textureAspectFlagsFor(TextureFormat format);
VkImageUsageFlags textureUsageFlagsFor(TextureType type, TextureFormat format);
VkSampleCountFlagBits sampleCountFlagBitsFor(int samples);

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter,
                        VkMemoryPropertyFlags properties);

size_t bytesPerPixel(TextureFormat format);

VkShaderStageFlagBits shaderTypeToVk(ShaderType type);

std::vector<uint32_t> compileSlangToSPIRV(const std::string &source,
                                          ShaderType type,
                                          const std::string &entryPoint);

std::vector<ShaderBinding> reflectShaderBindings(ShaderState &state,
                                                 ProgramState &programState);
VkDescriptorType descriptorTypeToVk(ShaderResourceType type);

inline uint64_t bindingKey(uint32_t set, uint32_t binding) {
    return (uint64_t(set) << 32) | binding;
}

VkBlendFactor blenderFuncToVk(BlendFunc func);
VkBlendOp blenderOpToVk(BlendEquation op);

VkCompareOp compareOpToVk(CompareOp op);
VkPrimitiveTopology primitiveStyleToVk(PrimitiveStyle style);
VkPolygonMode rasterizerModeToVk(RasterizerMode mode);
VkCullModeFlags cullModeToVk(CullMode mode);
VkFrontFace frontFaceToVk(FrontFace face);

VkFormat vertexAttributeFormatToVk(VertexAttributeType type, uint size,
                                   bool normalized);

VkVertexInputRate vertexBindingRateToVk(VertexBindingInputRate rate);

} // namespace opal::vulkan

#endif

#endif
