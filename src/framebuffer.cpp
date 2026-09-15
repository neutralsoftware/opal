//
// framebuffer.cpp
// As part of the Atlas project
// Created by Max Van den Eynde in 2025
// --------------------------------------------------
// Description: Framebuffer abstractions for Opal
// Copyright (c) 2025 maxvdec
//

#include "diagnostics.h"
#include "opal/opal.h"
#include <algorithm>
#include <glad/glad.h>
#include <memory>
#include <utility>
#ifdef METAL
#include "metal_state.h"
#endif

#ifdef VULKAN
#include "vulkan_state.h"
#endif

namespace opal {

#ifndef OPENGL
namespace {

void upsertColorAttachment(std::vector<Attachment> &attachments, int colorIndex,
                           const std::shared_ptr<Texture> &texture) {
    if (colorIndex < 0) {
        return;
    }

    int currentColor = 0;
    for (auto &attachment : attachments) {
        if (attachment.type != Attachment::Type::Color) {
            continue;
        }
        if (currentColor == colorIndex) {
            attachment.texture = texture;
            return;
        }
        currentColor++;
    }

    Attachment attachment;
    attachment.type = Attachment::Type::Color;
    attachment.texture = texture;
    attachments.push_back(attachment);
}

void upsertAttachmentByType(std::vector<Attachment> &attachments,
                            Attachment::Type type,
                            const std::shared_ptr<Texture> &texture) {
    for (auto &attachment : attachments) {
        if (attachment.type == type) {
            attachment.texture = texture;
            return;
        }
    }
    Attachment attachment;
    attachment.type = type;
    attachment.texture = texture;
    attachments.push_back(attachment);
}

std::vector<std::shared_ptr<Texture>>
collectDrawColorAttachments(const std::shared_ptr<Framebuffer> &fb,
                            int preferredIndex = -1) {
    std::vector<std::shared_ptr<Texture>> result;
    if (fb == nullptr) {
        return result;
    }

    const int drawLimit = fb->getDrawBufferCount();
    int colorIndex = 0;
    for (const auto &attachment : fb->attachments) {
        if (attachment.type != Attachment::Type::Color ||
            attachment.texture == nullptr) {
            continue;
        }
        if (drawLimit >= 0 && colorIndex >= drawLimit) {
            break;
        }
        if (preferredIndex >= 0) {
            if (colorIndex == preferredIndex) {
                result.push_back(attachment.texture);
                break;
            }
        } else {
            result.push_back(attachment.texture);
        }
        colorIndex++;
    }
    return result;
}

std::shared_ptr<Texture>
collectDepthAttachment(const std::shared_ptr<Framebuffer> &framebuffer) {
    if (framebuffer == nullptr) {
        return nullptr;
    }
    for (const auto &attachment : framebuffer->attachments) {
        if ((attachment.type == Attachment::Type::Depth ||
             attachment.type == Attachment::Type::DepthStencil) &&
            attachment.texture != nullptr) {
            return attachment.texture;
        }
    }
    return nullptr;
}

} // namespace

#endif

Framebuffer::~Framebuffer() {
#ifdef METAL
    metal::releaseFramebufferState(this);
#elif VULKAN
    vulkan::releaseFramebufferState(this);
#endif
}

std::shared_ptr<RenderPass> RenderPass::create() {
    return std::make_shared<RenderPass>();
}

void RenderPass::setFramebuffer(std::shared_ptr<Framebuffer> framebuffer) {
    this->framebuffer = std::move(framebuffer);
}

std::shared_ptr<Framebuffer> Framebuffer::create(int width, int height) {
    auto framebuffer = std::make_shared<Framebuffer>();
    framebuffer->width = width;
    framebuffer->height = height;
    framebuffer->drawBufferCount = -1;

#ifdef OPENGL
    glGenFramebuffers(1, &framebuffer->framebufferID);
#elif VULKAN
    auto &state = vulkan::framebufferState(framebuffer.get());
    state.dirty = true;
#endif

    return framebuffer;
}

std::shared_ptr<Framebuffer> Framebuffer::create() {
    auto framebuffer = std::make_shared<Framebuffer>();
    framebuffer->width = 0;
    framebuffer->height = 0;
    framebuffer->drawBufferCount = -1;

#ifdef OPENGL
    glGenFramebuffers(1, &framebuffer->framebufferID);
#elif VULKAN
    auto &state = vulkan::framebufferState(framebuffer.get());
    state.dirty = true;
#endif

    return framebuffer;
}

void Framebuffer::attachTexture(const std::shared_ptr<Texture> &texture,
                                int attachmentIndex) {
#ifdef OPENGL
    glBindFramebuffer(GL_FRAMEBUFFER, framebufferID);
    GLenum attachmentType =
        GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(attachmentIndex);
    glFramebufferTexture2D(GL_FRAMEBUFFER, attachmentType, texture->glType,
                           texture->textureID, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
#elif defined(METAL)
    upsertColorAttachment(attachments, attachmentIndex, texture);
#elif defined(VULKAN)
    upsertColorAttachment(attachments, attachmentIndex, texture);
    vulkan::framebufferState(this).dirty = true;
#endif
}

void Framebuffer::addAttachment(const Attachment &attachment) {
#ifdef OPENGL
    glBindFramebuffer(GL_FRAMEBUFFER, framebufferID);
    GLenum attachmentType;
    switch (attachment.type) {
    case Attachment::Type::Color:
        attachmentType =
            GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(attachments.size());
        break;
    case Attachment::Type::Depth:
        attachmentType = GL_DEPTH_ATTACHMENT;
        break;
    case Attachment::Type::Stencil:
        attachmentType = GL_STENCIL_ATTACHMENT;
        break;
    case Attachment::Type::DepthStencil:
        attachmentType = GL_DEPTH_STENCIL_ATTACHMENT;
        break;
    default:
        detail::log(LogLevel::Error, "Unknown attachment type");
        throw std::runtime_error("Unknown attachment type");
    }
    glFramebufferTexture2D(GL_FRAMEBUFFER, attachmentType,
                           attachment.texture->glType,
                           attachment.texture->textureID, 0);
    attachments.push_back(attachment);
#elif defined(METAL)
    attachments.push_back(attachment);
#elif defined(VULKAN)
    attachments.push_back(attachment);
    vulkan::framebufferState(this).dirty = true;
#endif
}

void Framebuffer::attachCubemap(const std::shared_ptr<Texture> &texture,
                                Attachment::Type attachmentType) {
#ifdef OPENGL
    glBindFramebuffer(GL_FRAMEBUFFER, framebufferID);
    GLenum glAttachmentType;
    switch (attachmentType) {
    case Attachment::Type::Depth:
        glAttachmentType = GL_DEPTH_ATTACHMENT;
        break;
    case Attachment::Type::Color:
        glAttachmentType =
            GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(attachments.size());
        break;
    default:
        glAttachmentType = GL_DEPTH_ATTACHMENT;
        break;
    }
    // Use glFramebufferTexture for cubemaps (attaches all 6 faces)
    glFramebufferTexture(GL_FRAMEBUFFER, glAttachmentType, texture->textureID,
                         0);

    Attachment att;
    att.type = attachmentType;
    att.texture = texture;
    attachments.push_back(att);
#elif defined(METAL)
    upsertAttachmentByType(attachments, attachmentType, texture);
#elif defined(VULKAN)
    upsertAttachmentByType(attachments, attachmentType, texture);
    vulkan::framebufferState(this).dirty = true;
#endif
}

void Framebuffer::attachCubemapFace(const std::shared_ptr<Texture> &texture,
                                    int face, Attachment::Type attachmentType) {
#ifdef OPENGL
    glBindFramebuffer(GL_FRAMEBUFFER, framebufferID);
    GLenum glAttachmentType;
    switch (attachmentType) {
    case Attachment::Type::Depth:
        glAttachmentType = GL_DEPTH_ATTACHMENT;
        break;
    case Attachment::Type::Color:
        glAttachmentType =
            GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(attachments.size());
        break;
    default:
        glAttachmentType = GL_DEPTH_ATTACHMENT;
        break;
    }
    // Attach a specific cubemap face (0-5: +X, -X, +Y, -Y, +Z, -Z)
    glFramebufferTexture2D(GL_FRAMEBUFFER, glAttachmentType,
                           GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                           texture->textureID, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
#elif defined(METAL)
    (void)face;
    upsertAttachmentByType(attachments, attachmentType, texture);
#elif defined(VULKAN)
    (void)face;
    upsertAttachmentByType(attachments, attachmentType, texture);
    auto &state = vulkan::framebufferState(this);

    state.cubemapFace = face;
    state.dirty = true;
#endif
}

void Framebuffer::disableColorBuffer() {
#ifdef OPENGL
    glBindFramebuffer(GL_FRAMEBUFFER, framebufferID);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    colorBufferDisabled = true;
#elif defined(METAL)
    colorBufferDisabled = true;
    drawBufferCount = 0;
#elif defined(VULKAN)
    colorBufferDisabled = true;
    drawBufferCount = 0;
    vulkan::framebufferState(this).dirty = true;
#endif
}

void Framebuffer::setViewport() {
#ifdef OPENGL
    glViewport(0, 0, width, height);
#elif defined(METAL)
    width = std::max(width, 1);
    height = std::max(height, 1);
#elif defined(VULKAN)
    width = std::max(width, 1);
    height = std::max(height, 1);
#endif
}

void Framebuffer::setViewport(int x, int y, int viewWidth, int viewHeight) {
#ifdef OPENGL
    glViewport(x, y, viewWidth, viewHeight);
#elif defined(METAL)
    (void)x;
    (void)y;
    if (viewWidth > 0) {
        width = viewWidth;
    }
    if (viewHeight > 0) {
        height = viewHeight;
    }
#elif defined(VULKAN)
    auto &state = vulkan::framebufferState(this);
    state.viewportX = x;
    state.viewportY = y;

    if (viewWidth > 0) {
        width = viewWidth;
    }
    if (viewHeight > 0) {
        height = viewHeight;
    }
#endif
}

bool Framebuffer::getStatus() const {
#ifdef OPENGL
    glBindFramebuffer(GL_FRAMEBUFFER, framebufferID);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    return status == GL_FRAMEBUFFER_COMPLETE;
#elif defined(METAL)
    (void)this;
    return true;
#elif defined(VULKAN)
    if (isDefaultFramebuffer) {
        return true;
    }
    if (width <= 0 || height <= 0 || attachments.empty()) {
        return false;
    }
    int samples = -1;
    for (const auto &attachment : attachments) {
        if (attachment.texture == nullptr ||
            attachment.texture->width != width ||
            attachment.texture->height != height) {
            return false;
        }
        if (samples < 0) {
            samples = attachment.texture->samples;
        } else if (samples != attachment.texture->samples) {
            return false;
        }
        const auto &state = vulkan::textureState(attachment.texture.get());
        if (state.image == VK_NULL_HANDLE ||
            state.imageView == VK_NULL_HANDLE) {
            return false;
        }
    }
    return true;
#else
    return false;
#endif
}

void Framebuffer::bind() {
#ifdef OPENGL
    glBindFramebuffer(GL_FRAMEBUFFER, framebufferID);
    if (framebufferID == 0) {
        glDrawBuffer(GL_BACK);
    } else if (colorBufferDisabled) {
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    } else if (attachments.size() > 0) {
        std::vector<GLenum> drawBuffers;
        for (size_t i = 0; i < attachments.size(); ++i) {
            if (attachments[i].type == Attachment::Type::Color) {
                drawBuffers.push_back(GL_COLOR_ATTACHMENT0 +
                                      static_cast<GLenum>(drawBuffers.size()));
            }
        }
        if (drawBuffers.empty()) {
            glDrawBuffer(GL_NONE);
        } else {
            glDrawBuffers(static_cast<GLsizei>(drawBuffers.size()),
                          drawBuffers.data());
        }
    } else {
        glDrawBuffer(GL_NONE);
    }
#elif defined(METAL)
#endif
}

void Framebuffer::unbind() {
#ifdef OPENGL
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
#elif defined(METAL)
#endif
}

void Framebuffer::bindForRead() {
#ifdef OPENGL
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebufferID);
#elif defined(METAL)
#endif
}

void Framebuffer::bindForDraw() {
#ifdef OPENGL
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebufferID);
#elif defined(METAL)
#endif
}

void Framebuffer::setDrawBuffers(int attachmentCount) {
    drawBufferCount = attachmentCount < 0 ? -1 : attachmentCount;
#ifdef OPENGL
    if (attachmentCount <= 0) {
        glDrawBuffer(GL_NONE);
        return;
    }
    std::vector<GLenum> drawBuffers;
    drawBuffers.reserve(attachmentCount);
    for (int i = 0; i < attachmentCount; ++i) {
        drawBuffers.push_back(GL_COLOR_ATTACHMENT0 + i);
    }
    glDrawBuffers(attachmentCount, drawBuffers.data());
#elif defined(METAL)
    (void)attachmentCount;
#elif defined(VULKAN)
    vulkan::framebufferState(this).dirty = true;
#endif
}

std::shared_ptr<ResolveAction>
ResolveAction::create(std::shared_ptr<Framebuffer> source,
                      std::shared_ptr<Framebuffer> destination) {
    auto action = std::make_shared<ResolveAction>();
    action->source = std::move(source);
    action->destination = std::move(destination);
    action->colorAttachmentIndex = -1;
    action->resolveColor = true;
    action->resolveDepth = true;
    return action;
}

std::shared_ptr<ResolveAction> ResolveAction::createForColorAttachment(
    std::shared_ptr<Framebuffer> source,
    std::shared_ptr<Framebuffer> destination, int colorAttachmentIndex) {
    auto action = std::make_shared<ResolveAction>();
    action->source = std::move(source);
    action->destination = std::move(destination);
    action->colorAttachmentIndex = colorAttachmentIndex;
    action->resolveColor = true;
    action->resolveDepth = false;
    return action;
}

std::shared_ptr<ResolveAction>
ResolveAction::createForDepth(std::shared_ptr<Framebuffer> source,
                              std::shared_ptr<Framebuffer> destination) {
    auto action = std::make_shared<ResolveAction>();
    action->source = std::move(source);
    action->destination = std::move(destination);
    action->colorAttachmentIndex = -1;
    action->resolveColor = false;
    action->resolveDepth = true;
    return action;
}

void CommandBuffer::performResolve(
    const std::shared_ptr<ResolveAction> &action) {
#ifdef OPENGL
    glBindFramebuffer(GL_READ_FRAMEBUFFER, action->source->framebufferID);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, action->destination->framebufferID);

    GLbitfield mask = 0;

    if (action->resolveColor) {
        if (action->colorAttachmentIndex >= 0) {
            // Resolve specific color attachment
            glReadBuffer(GL_COLOR_ATTACHMENT0 + action->colorAttachmentIndex);
            glDrawBuffer(GL_COLOR_ATTACHMENT0 + action->colorAttachmentIndex);
            mask |= GL_COLOR_BUFFER_BIT;
        } else {
            // Resolve all color attachments
            mask |= GL_COLOR_BUFFER_BIT;
        }
    }

    if (action->resolveDepth) {
        mask |= GL_DEPTH_BUFFER_BIT;
    }

    if (mask != 0) {
        glBlitFramebuffer(0, 0, action->source->width, action->source->height,
                          0, 0, action->destination->width,
                          action->destination->height, mask, GL_NEAREST);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
#elif defined(METAL)
    if (action == nullptr || action->source == nullptr ||
        action->destination == nullptr) {
        return;
    }
    if (Device::globalInstance == nullptr) {
        return;
    }

    auto &deviceState = metal::deviceState(Device::globalInstance);
    if (deviceState.queue == nullptr) {
        return;
    }

    auto &state = metal::commandBufferState(this);
    if (state.encoder != nullptr) {
        state.encoder->endEncoding();
        state.encoder = nullptr;
        state.textureBindingsInitialized = false;
    }
    if (state.computeEncoder != nullptr) {
        state.computeEncoder->endEncoding();
        state.computeEncoder = nullptr;
    }

    bool ownedCommandBuffer = false;
    NS::AutoreleasePool *resolvePool = nullptr;
    MTL::CommandBuffer *metalCB = state.commandBuffer;
    if (metalCB == nullptr) {
        resolvePool = NS::AutoreleasePool::alloc()->init();
        metalCB = deviceState.queue->commandBuffer();
        ownedCommandBuffer = true;
    }

    auto gatherColorAttachments = [](const std::shared_ptr<Framebuffer> &fb,
                                     int preferredIndex) {
        return collectDrawColorAttachments(fb, preferredIndex);
    };

    auto gatherDepthAttachment = [](const std::shared_ptr<Framebuffer> &fb) {
        if (fb == nullptr) {
            return std::shared_ptr<Texture>{nullptr};
        }
        for (const auto &attachment : fb->attachments) {
            if ((attachment.type == Attachment::Type::Depth ||
                 attachment.type == Attachment::Type::DepthStencil) &&
                attachment.texture != nullptr) {
                return attachment.texture;
            }
        }
        return std::shared_ptr<Texture>{nullptr};
    };

    auto resolveMsaaColor = [&](const std::shared_ptr<Texture> &source,
                                const std::shared_ptr<Texture> &destination) {
        auto &srcState = metal::textureState(source.get());
        auto &dstState = metal::textureState(destination.get());
        if (srcState.texture == nullptr || dstState.texture == nullptr) {
            return;
        }

        MTL::RenderPassDescriptor *descriptor =
            MTL::RenderPassDescriptor::renderPassDescriptor();
        auto *colorAttachment = descriptor->colorAttachments()->object(0);
        colorAttachment->setTexture(srcState.texture);
        colorAttachment->setResolveTexture(dstState.texture);
        colorAttachment->setLoadAction(MTL::LoadActionLoad);
        colorAttachment->setStoreAction(MTL::StoreActionMultisampleResolve);

        MTL::RenderCommandEncoder *encoder =
            metalCB->renderCommandEncoder(descriptor);
        if (encoder != nullptr) {
            encoder->endEncoding();
        }
    };

    auto copyTexture = [&](const std::shared_ptr<Texture> &source,
                           const std::shared_ptr<Texture> &destination) {
        auto &srcState = metal::textureState(source.get());
        auto &dstState = metal::textureState(destination.get());
        if (srcState.texture == nullptr || dstState.texture == nullptr) {
            return;
        }
        MTL::BlitCommandEncoder *blit = metalCB->blitCommandEncoder();
        blit->copyFromTexture(srcState.texture, dstState.texture);
        blit->endEncoding();
    };

    auto sourceColors =
        gatherColorAttachments(action->source, action->colorAttachmentIndex);
    auto destinationColors = gatherColorAttachments(
        action->destination, action->colorAttachmentIndex);

    if (action->resolveColor && !sourceColors.empty() &&
        !destinationColors.empty()) {
        size_t count = std::min(sourceColors.size(), destinationColors.size());
        for (size_t i = 0; i < count; ++i) {
            auto &src = sourceColors[i];
            auto &dst = destinationColors[i];
            if (!src || !dst) {
                continue;
            }
            if (src->samples > 1 && dst->samples == 1) {
                resolveMsaaColor(src, dst);
            } else {
                copyTexture(src, dst);
            }
        }
    }

    if (action->resolveDepth) {
        auto srcDepth = gatherDepthAttachment(action->source);
        auto dstDepth = gatherDepthAttachment(action->destination);
        if (srcDepth != nullptr && dstDepth != nullptr &&
            srcDepth->samples == dstDepth->samples) {
            copyTexture(srcDepth, dstDepth);
        }
    }

    if (ownedCommandBuffer) {
        metalCB->commit();
        metalCB->waitUntilCompleted();
        if (resolvePool != nullptr) {
            resolvePool->release();
            resolvePool = nullptr;
        }
    }
#elif defined(VULKAN)
    if (action == nullptr || action->source == nullptr ||
        action->destination == nullptr) {
        return;
    }
    auto &command = vulkan::commandBufferState(this);
    if (!command.recording) {
        throw std::runtime_error("performResolve requires command recording");
    }
    if (command.rendering) {
        endPass();
    }

    auto copyTexture = [&](const std::shared_ptr<Texture> &source,
                           const std::shared_ptr<Texture> &destination) {
        auto &sourceState = vulkan::textureState(source.get());
        auto &destinationState = vulkan::textureState(destination.get());
        if (sourceState.sampleCount != destinationState.sampleCount ||
            sourceState.format != destinationState.format) {
            throw std::runtime_error(
                "Vulkan texture copy requires matching formats and samples");
        }
        VkImageLayout sourceLayout = sourceState.layout;
        VkImageLayout destinationLayout = destinationState.layout;
        vulkan::transitionTexture(command.commandBuffer, sourceState,
                                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        vulkan::transitionTexture(command.commandBuffer, destinationState,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkImageCopy region{};
        region.srcSubresource.aspectMask = sourceState.aspectMask;
        region.srcSubresource.layerCount =
            std::min(sourceState.arrayLayers, destinationState.arrayLayers);
        region.dstSubresource.aspectMask = destinationState.aspectMask;
        region.dstSubresource.layerCount = region.srcSubresource.layerCount;
        region.extent = {std::min(sourceState.width, destinationState.width),
                         std::min(sourceState.height, destinationState.height),
                         std::min(sourceState.depth, destinationState.depth)};
        vkCmdCopyImage(command.commandBuffer, sourceState.image,
                       sourceState.layout, destinationState.image,
                       destinationState.layout, 1, &region);
        vulkan::transitionTexture(command.commandBuffer, sourceState,
                                  sourceLayout);
        vulkan::transitionTexture(command.commandBuffer, destinationState,
                                  destinationLayout);
    };

    auto resolveTexture = [&](const std::shared_ptr<Texture> &source,
                              const std::shared_ptr<Texture> &destination,
                              bool depth) {
        auto &sourceState = vulkan::textureState(source.get());
        auto &destinationState = vulkan::textureState(destination.get());
        if (sourceState.sampleCount == VK_SAMPLE_COUNT_1_BIT ||
            destinationState.sampleCount != VK_SAMPLE_COUNT_1_BIT ||
            sourceState.format != destinationState.format) {
            throw std::runtime_error("Vulkan resolve requires matching "
                                     "multisample and single-sample textures");
        }
        VkImageLayout sourceLayout = sourceState.layout;
        VkImageLayout destinationLayout = destinationState.layout;
        VkImageLayout attachmentLayout =
            depth ? VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
                  : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        vulkan::transitionTexture(command.commandBuffer, sourceState,
                                  attachmentLayout);
        vulkan::transitionTexture(command.commandBuffer, destinationState,
                                  attachmentLayout);
        VkRenderingAttachmentInfo attachment{};
        attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        attachment.imageView = sourceState.imageView;
        attachment.imageLayout = attachmentLayout;
        attachment.resolveMode = depth ? VK_RESOLVE_MODE_SAMPLE_ZERO_BIT
                                       : VK_RESOLVE_MODE_AVERAGE_BIT;
        attachment.resolveImageView = destinationState.imageView;
        attachment.resolveImageLayout = attachmentLayout;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        VkRenderingInfo info{};
        info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        info.renderArea.extent = {
            std::min(sourceState.width, destinationState.width),
            std::min(sourceState.height, destinationState.height)};
        info.layerCount = 1;
        if (depth) {
            info.pDepthAttachment = &attachment;
        } else {
            info.colorAttachmentCount = 1;
            info.pColorAttachments = &attachment;
        }
        vkCmdBeginRendering(command.commandBuffer, &info);
        vkCmdEndRendering(command.commandBuffer);
        vulkan::transitionTexture(command.commandBuffer, sourceState,
                                  sourceLayout);
        vulkan::transitionTexture(command.commandBuffer, destinationState,
                                  destinationLayout);
    };

    if (action->resolveColor) {
        auto sources = collectDrawColorAttachments(
            action->source, action->colorAttachmentIndex);
        auto destinations = collectDrawColorAttachments(
            action->destination, action->colorAttachmentIndex);
        size_t count = std::min(sources.size(), destinations.size());
        for (size_t index = 0; index < count; ++index) {
            if (sources[index]->samples > 1) {
                resolveTexture(sources[index], destinations[index], false);
            } else {
                copyTexture(sources[index], destinations[index]);
            }
        }
    }
    if (action->resolveDepth) {
        auto source = collectDepthAttachment(action->source);
        auto destination = collectDepthAttachment(action->destination);
        if (source != nullptr && destination != nullptr) {
            if (source->samples > 1) {
                resolveTexture(source, destination, true);
            } else {
                copyTexture(source, destination);
            }
        }
    }
#endif
}

} // namespace opal
