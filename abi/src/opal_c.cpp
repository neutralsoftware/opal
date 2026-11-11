#include "opal_c.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <opal/opal.h>
#include <stdexcept>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {
thread_local char last_error[2048] = {};
struct Unsupported {};
struct InitializationFailure : std::runtime_error {
    using std::runtime_error::runtime_error;
};
void require(bool condition,
             const char *message = "Invalid or missing argument") {
    if (!condition)
        throw std::invalid_argument(message);
}
template <class F>
OpalResult guard(const char *operation, F &&function) noexcept {
    last_error[0] = 0;
    try {
        function();
        return OPAL_SUCCESS;
    } catch (const InitializationFailure &e) {
        std::snprintf(last_error, sizeof(last_error), "%s: %s", operation,
                      e.what());
        return OPAL_ERROR_INITIALIZATION_FAILED;
    } catch (const std::invalid_argument &e) {
        std::snprintf(last_error, sizeof(last_error), "%s: %s", operation,
                      e.what());
        return OPAL_ERROR_INVALID_ARGUMENT;
    } catch (const std::bad_alloc &) {
        std::snprintf(last_error, sizeof(last_error), "%s: Out of memory",
                      operation);
        return OPAL_ERROR_OUT_OF_MEMORY;
    } catch (const Unsupported &) {
        std::snprintf(last_error, sizeof(last_error),
                      "%s: Unsupported by the compiled backend", operation);
        return OPAL_ERROR_UNSUPPORTED;
    } catch (const std::exception &e) {
        std::snprintf(last_error, sizeof(last_error), "%s: %s", operation,
                      e.what());
        return OPAL_ERROR_UNKNOWN;
    } catch (...) {
        std::snprintf(last_error, sizeof(last_error),
                      "%s: Unknown C++ exception", operation);
        return OPAL_ERROR_UNKNOWN;
    }
}
template <class T> T checked_enum(int32_t value, int32_t count) {
    require(value >= 0 && value < count, "Enum value is out of range");
    return static_cast<T>(value);
}
glm::mat4 matrix(const float *values) {
    require(values);
    glm::mat4 result(1.0f);
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            result[c][r] = values[c * 4 + r];
    return result;
}
template <class T> std::vector<T> copy_array(const T *values, size_t count) {
    require(values || count == 0);
    if (!count)
        return {};
    return {values, values + count};
}
void copy_string(const std::string &value, char *buffer, size_t capacity,
                 size_t *required) {
    require(required && (buffer || capacity == 0));
    *required = value.size() + 1;
    if (capacity) {
        size_t n = std::min(value.size(), capacity - 1);
        std::memcpy(buffer, value.data(), n);
        buffer[n] = 0;
    }
}
}

struct OpalContext {
    std::shared_ptr<opal::Device> owner;
    std::shared_ptr<opal::Context> value;
};

struct OpalDevice {
    std::shared_ptr<opal::Device> owner;
    std::shared_ptr<opal::Device> value;
};

struct OpalCommandBuffer {
    std::shared_ptr<opal::Device> owner;
    std::shared_ptr<opal::CommandBuffer> value;
    bool recording = false;
    bool pass_active = false;
    bool pipeline_bound = false;
};

struct OpalTexture {
    std::shared_ptr<opal::Device> owner;
    std::shared_ptr<opal::Texture> value;
};

struct OpalShader {
    std::shared_ptr<opal::Device> owner;
    std::shared_ptr<opal::Shader> value;
};

struct OpalShaderProgram {
    std::shared_ptr<opal::Device> owner;
    std::shared_ptr<opal::ShaderProgram> value;
};

struct OpalPipeline {
    std::shared_ptr<opal::Device> owner;
    std::shared_ptr<opal::Pipeline> value;
};

struct OpalBuffer {
    std::shared_ptr<opal::Device> owner;
    std::shared_ptr<opal::Buffer> value;
};

struct OpalDrawingState {
    std::shared_ptr<opal::Device> owner;
    std::shared_ptr<opal::DrawingState> value;
};

struct OpalDepthStencilBuffer {
    std::shared_ptr<opal::Device> owner;
    std::shared_ptr<opal::DepthStencilBuffer> value;
};

struct OpalFramebuffer {
    std::shared_ptr<opal::Device> owner;
    std::shared_ptr<opal::Framebuffer> value;
};

struct OpalRenderPass {
    std::shared_ptr<opal::Device> owner;
    std::shared_ptr<opal::RenderPass> value;
};

struct OpalResolveAction {
    std::shared_ptr<opal::Device> owner;
    std::shared_ptr<opal::ResolveAction> value;
};

#if defined(METAL) || defined(VULKAN)
struct OpalPrimitiveAccelerationStructure {
    std::shared_ptr<opal::Device> owner;
    std::shared_ptr<opal::PrimitiveAccelerationStructure> value;
};
#else
struct OpalPrimitiveAccelerationStructure {};
#endif

#if defined(METAL) || defined(VULKAN)
struct OpalInstanceAccelerationStructure {
    std::shared_ptr<opal::Device> owner;
    std::shared_ptr<opal::InstanceAccelerationStructure> value;
};
#else
struct OpalInstanceAccelerationStructure {};
#endif

namespace {
template <class H> std::unordered_set<H *> &handles() {
    static std::unordered_set<H *> values;
    return values;
}
template <class H> auto &object(H *handle) {
    require(handle && handles<H>().contains(handle),
            "Handle is null, destroyed, foreign, or has the wrong type");
    require(handle->value != nullptr, "Handle has no native object");
    return handle->value;
}
template <class H> auto ownership(H *handle) {
    object(handle);
    return handle->owner;
}
template <class H> void release(H *handle) {
    if (!handle)
        return;
    require(handles<H>().erase(handle) == 1,
            "Cannot destroy an unknown or already destroyed handle");
    delete handle;
}
template <class H, class V>
void output(H **out, V value, std::shared_ptr<opal::Device> owner = {}) {
    require(out);
    *out = nullptr;
    if (!value)
        throw std::runtime_error("Backend returned an empty object");
    auto handle = std::make_unique<H>(H{std::move(owner), std::move(value)});
    handles<H>().insert(handle.get());
    *out = handle.release();
}
std::weak_ptr<opal::Device> active_device;
std::shared_ptr<opal::Device> current_device() {
    auto device = active_device.lock();
    require(device && device.get() == opal::Device::globalInstance,
            "Acquire a C ABI device before creating resources");
    return device;
}
opal::VertexAttribute attribute(const OpalVertexAttribute &a) {
    require(a.name && a.size > 0 && a.size <= 4);
    return {a.name,
            checked_enum<opal::VertexAttributeType>(a.type, 8),
            a.offset,
            a.location,
            a.normalized != 0,
            a.size,
            a.stride,
            checked_enum<opal::VertexBindingInputRate>(a.input_rate, 2),
            a.divisor};
}
}

const char *OPAL_C_CALL opal_get_last_error(void) { return last_error; }

uint32_t OPAL_C_CALL opal_get_abi_version(void) { return 1; }

OpalBackend OPAL_C_CALL opal_get_backend(void) {
#if defined(METAL)
    return OPAL_BACKEND_METAL;
#elif defined(VULKAN)
    return OPAL_BACKEND_VULKAN;
#else
    return OPAL_BACKEND_OPENGL;
#endif
}

OpalResult OPAL_C_CALL
opal_context_configuration_default(OpalContextConfiguration *out) {
    return guard(__func__, [&] {
        require(out);
        *out = {1, 4, 1, OPAL_OPEN_GL_PROFILE_CORE, "", "", 1};
    });
}

OpalResult OPAL_C_CALL opal_context_create(OpalContext **out) {
    return guard(__func__, [&] {
        require(out);
        *out = nullptr;
        output(out, opal::Context::create(), {});
    });
}

OpalResult OPAL_C_CALL opal_context_create_with_configuration(
    const OpalContextConfiguration *configuration, OpalContext **out) {
    return guard(__func__, [&] {
        require(out);
        *out = nullptr;
        require(configuration);
        const auto &v = *configuration;
        opal::ContextConfiguration c;
        c.useOpenGL = v.use_opengl != 0;
        c.majorVersion = v.major_version;
        c.minorVersion = v.minor_version;
        c.profile = checked_enum<opal::OpenGLProfile>(v.profile, 2);
        c.applicationName = v.application_name ? v.application_name : "";
        c.applicationVersion =
            v.application_version ? v.application_version : "";
        c.createValidationLayers = v.create_validation_layers != 0;
        output(out, opal::Context::create(c), {});
    });
}

OpalResult OPAL_C_CALL opal_context_destroy(OpalContext *handle) {
    return guard(__func__, [&] { release(handle); });
}

OpalResult OPAL_C_CALL opal_device_destroy(OpalDevice *handle) {
    return guard(__func__, [&] { release(handle); });
}

OpalResult OPAL_C_CALL opal_command_buffer_destroy(OpalCommandBuffer *handle) {
    return guard(__func__, [&] { release(handle); });
}

OpalResult OPAL_C_CALL opal_texture_destroy(OpalTexture *handle) {
    return guard(__func__, [&] { release(handle); });
}

OpalResult OPAL_C_CALL opal_shader_destroy(OpalShader *handle) {
    return guard(__func__, [&] { release(handle); });
}

OpalResult OPAL_C_CALL opal_shader_program_destroy(OpalShaderProgram *handle) {
    return guard(__func__, [&] { release(handle); });
}

OpalResult OPAL_C_CALL opal_pipeline_destroy(OpalPipeline *handle) {
    return guard(__func__, [&] { release(handle); });
}

OpalResult OPAL_C_CALL opal_buffer_destroy(OpalBuffer *handle) {
    return guard(__func__, [&] { release(handle); });
}

OpalResult OPAL_C_CALL opal_drawing_state_destroy(OpalDrawingState *handle) {
    return guard(__func__, [&] { release(handle); });
}

OpalResult OPAL_C_CALL
opal_depth_stencil_buffer_destroy(OpalDepthStencilBuffer *handle) {
    return guard(__func__, [&] { release(handle); });
}

OpalResult OPAL_C_CALL opal_framebuffer_destroy(OpalFramebuffer *handle) {
    return guard(__func__, [&] { release(handle); });
}

OpalResult OPAL_C_CALL opal_render_pass_destroy(OpalRenderPass *handle) {
    return guard(__func__, [&] { release(handle); });
}

OpalResult OPAL_C_CALL opal_resolve_action_destroy(OpalResolveAction *handle) {
    return guard(__func__, [&] { release(handle); });
}

OpalResult OPAL_C_CALL opal_primitive_acceleration_structure_destroy(
    OpalPrimitiveAccelerationStructure *handle) {
    return guard(__func__, [&] {
        if (!handle)
            return;
#if defined(METAL) || defined(VULKAN)
        release(handle);
#else
        throw Unsupported{};
#endif
    });
}

OpalResult OPAL_C_CALL opal_instance_acceleration_structure_destroy(
    OpalInstanceAccelerationStructure *handle) {
    return guard(__func__, [&] {
        if (!handle)
            return;
#if defined(METAL) || defined(VULKAN)
        release(handle);
#else
        throw Unsupported{};
#endif
    });
}

OpalResult OPAL_C_CALL opal_context_set_decorated(OpalContext *handle,
                                                  int32_t enabled) {
    return guard(__func__, [&] { object(handle)->setDecorated(enabled != 0); });
}

OpalResult OPAL_C_CALL opal_context_set_resizable(OpalContext *handle,
                                                  int32_t enabled) {
    return guard(__func__, [&] { object(handle)->setResizable(enabled != 0); });
}

OpalResult OPAL_C_CALL opal_context_set_transparent(OpalContext *handle,
                                                    int32_t enabled) {
    return guard(__func__,
                 [&] { object(handle)->setTransparent(enabled != 0); });
}

OpalResult OPAL_C_CALL opal_context_set_always_on_top(OpalContext *handle,
                                                      int32_t enabled) {
    return guard(__func__,
                 [&] { object(handle)->setAlwaysOnTop(enabled != 0); });
}

OpalResult OPAL_C_CALL opal_context_set_high_pixel_density(OpalContext *handle,
                                                           int32_t enabled) {
    return guard(__func__,
                 [&] { object(handle)->setHighPixelDensity(enabled != 0); });
}

OpalResult OPAL_C_CALL opal_context_set_hidden(OpalContext *handle,
                                               int32_t enabled) {
    return guard(__func__, [&] { object(handle)->setHidden(enabled != 0); });
}

OpalResult OPAL_C_CALL opal_context_set_samples(OpalContext *handle,
                                                int32_t samples) {
    return guard(__func__, [&] {
        require(samples >= 0);
        object(handle)->setSamples(samples);
    });
}

OpalResult OPAL_C_CALL opal_context_make_window(OpalContext *context, int width,
                                                int height, const char *title) {
    return guard(__func__, [&] {
        require(width > 0 && height > 0 && title);
        require(object(context)->getWindow() == nullptr,
                "Context already has a window");
        if (!object(context)->makeWindow(width, height, title))
            throw InitializationFailure(SDL_GetError());
    });
}

OpalResult OPAL_C_CALL opal_context_make_window_on_display(
    OpalContext *handle, int32_t width, int32_t height, const char *title,
    uint32_t display_id) {
    return guard(__func__, [&] {
        require(width > 0 && height > 0 && title);
        require(object(handle)->getWindow() == nullptr,
                "Context already has a window");
        if (!object(handle)->makeWindow(width, height, title, display_id))
            throw InitializationFailure(SDL_GetError());
    });
}

OpalResult OPAL_C_CALL opal_context_make_window_from_native(
    OpalContext *handle, void *window, OpalNativeWindowType type, int32_t width,
    int32_t height, const char *title) {
    return guard(__func__, [&] {
        require(window && title && width > 0 && height > 0);
        if (!object(handle)->makeWindowFromNative(
                window, checked_enum<opal::NativeWindowType>(type, 4), width,
                height, title))
            throw InitializationFailure(SDL_GetError());
    });
}

OpalResult OPAL_C_CALL opal_context_adopt_window(OpalContext *handle,
                                                 void *window,
                                                 int32_t take_ownership) {
    return guard(__func__, [&] {
        require(window);
        object(handle)->adoptWindow(static_cast<SDL_Window *>(window),
                                    take_ownership != 0);
    });
}

OpalResult OPAL_C_CALL opal_context_set_metal_target_view(OpalContext *handle,
                                                          void *view) {
    return guard(__func__, [&] { object(handle)->setMetalTargetView(view); });
}

OpalResult OPAL_C_CALL opal_context_get_window(OpalContext *handle,
                                               void **out) {
    return guard(__func__, [&] {
        require(out);
        *out = object(handle)->getWindow();
    });
}

OpalResult OPAL_C_CALL opal_context_get_metal_target_view(OpalContext *handle,
                                                          void **out) {
    return guard(__func__, [&] {
        require(out);
        *out = object(handle)->getMetalTargetView();
    });
}

OpalResult OPAL_C_CALL opal_context_get_gl_context(OpalContext *handle,
                                                   void **out) {
    return guard(__func__, [&] {
        require(out);
        *out = object(handle)->glContext;
    });
}

OpalResult OPAL_C_CALL opal_context_make_current(OpalContext *handle) {
    return guard(__func__, [&] { object(handle)->makeCurrent(); });
}

OpalResult OPAL_C_CALL opal_context_swap_window(OpalContext *handle) {
    return guard(__func__, [&] {
#if defined(OPENGL)
        require(object(handle)->getWindow());
        if (!SDL_GL_SwapWindow(object(handle)->getWindow()))
            throw std::runtime_error(SDL_GetError());
#else
        throw Unsupported{};
#endif
    });
}

OpalResult OPAL_C_CALL opal_device_acquire(OpalContext *context,
                                           OpalDevice **out) {
    return guard(__func__, [&] {
        require(out);
        *out = nullptr;
        require(active_device.expired() &&
                    opal::Device::globalInstance == nullptr,
                "Only one live device is supported; destroy all device "
                "resources before acquiring another");
        require(object(context)->getWindow() ||
                object(context)->getMetalTargetView());
        auto device = opal::Device::acquire(object(context));
        output(out, device);
        active_device = device;
    });
}

OpalResult OPAL_C_CALL opal_command_buffer_acquire(OpalDevice *device,
                                                   OpalCommandBuffer **out) {
    return guard(__func__, [&] {
        require(out);
        *out = nullptr;
        output(out, object(device)->acquireCommandBuffer(), object(device));
    });
}

OpalResult OPAL_C_CALL opal_framebuffer_get_default(OpalDevice *device,
                                                    OpalFramebuffer **out) {
    return guard(__func__, [&] {
        require(out);
        *out = nullptr;
        output(out, object(device)->getDefaultFramebuffer(), object(device));
    });
}

OpalResult OPAL_C_CALL opal_device_submit_command_buffer(
    OpalDevice *handle, OpalCommandBuffer *commands) {
    return guard(__func__, [&] {
        object(handle)->submitCommandBuffer(object(commands));
    });
}

OpalResult OPAL_C_CALL opal_device_get_info(OpalDevice *handle,
                                            OpalDeviceInfoField field,
                                            char *buffer, size_t capacity,
                                            size_t *required) {
    return guard(__func__, [&] {
        auto info = object(handle)->getDeviceInfo();
        require(field >= 0 && field < 5);
        const std::string *fields[] = {
            &info.deviceName, &info.vendorName, &info.driverVersion,
            &info.renderingVersion, &info.opalVersion};
        copy_string(*fields[field], buffer, capacity, required);
    });
}

OpalResult OPAL_C_CALL opal_device_supports_ray_tracing(OpalDevice *handle,
                                                        int32_t *out) {
    return guard(__func__, [&] {
        require(out);
        *out = object(handle)->supportsRayTracing();
    });
}

OpalResult OPAL_C_CALL opal_device_get_frame_count(OpalDevice *handle,
                                                   int64_t *out) {
    return guard(__func__, [&] {
        require(out);
        *out = object(handle)->frameCount;
    });
}

OpalResult OPAL_C_CALL opal_device_get_metal_device(OpalDevice *handle,
                                                    void **out) {
    return guard(__func__, [&] {
        require(out);
        *out = nullptr;
#if defined(METAL)
        require(out);
        *out = object(handle)->getMetalDevice();
#else
        throw Unsupported{};
#endif
    });
}

OpalResult OPAL_C_CALL opal_texture_create(
    OpalTextureType type, OpalTextureFormat format, int32_t width,
    int32_t height, OpalTextureDataFormat data_format, const void *data,
    uint32_t mip_levels, OpalTexture **out) {
    return guard(__func__, [&] {
        require(out);
        *out = nullptr;
        require(width > 0 && height > 0 && mip_levels > 0);
        auto owner = current_device();
        output(out,
               opal::Texture::create(
                   checked_enum<opal::TextureType>(type, 5),
                   checked_enum<opal::TextureFormat>(format, 11), width, height,
                   checked_enum<opal::TextureDataFormat>(data_format, 6), data,
                   mip_levels),
               std::move(owner));
    });
}

OpalResult OPAL_C_CALL opal_texture_create_multisampled(
    OpalTextureFormat format, int32_t width, int32_t height, int32_t samples,
    OpalTexture **out) {
    return guard(__func__, [&] {
        require(out);
        *out = nullptr;
        require(width > 0 && height > 0 && samples > 0);
        auto owner = current_device();
        output(out,
               opal::Texture::createMultisampled(
                   checked_enum<opal::TextureFormat>(format, 11), width, height,
                   samples),
               std::move(owner));
    });
}

OpalResult OPAL_C_CALL opal_texture_create_depth_cubemap(
    OpalTextureFormat format, int32_t resolution, OpalTexture **out) {
    return guard(__func__, [&] {
        require(out);
        *out = nullptr;
        require(resolution > 0);
        auto owner = current_device();
        output(out,
               opal::Texture::createDepthCubemap(
                   checked_enum<opal::TextureFormat>(format, 11), resolution),
               std::move(owner));
    });
}

OpalResult OPAL_C_CALL opal_texture_create_3d(
    OpalTextureFormat format, int32_t width, int32_t height, int32_t depth,
    OpalTextureDataFormat data_format, const void *data, OpalTexture **out) {
    return guard(__func__, [&] {
        require(out);
        *out = nullptr;
        require(width > 0 && height > 0 && depth > 0);
        auto owner = current_device();
        output(out,
               opal::Texture::create3D(
                   checked_enum<opal::TextureFormat>(format, 11), width, height,
                   depth, checked_enum<opal::TextureDataFormat>(data_format, 6),
                   data),
               std::move(owner));
    });
}

OpalResult OPAL_C_CALL opal_texture_get_supported_sample_count(
    const OpalTextureFormat *formats, size_t count, int32_t requested,
    int32_t *out) {
    return guard(__func__, [&] {
#if defined(VULKAN)
        require(out && requested > 0 && count > 0 && formats);
        std::vector<opal::TextureFormat> values;
        for (size_t i = 0; i < count; ++i)
            values.push_back(checked_enum<opal::TextureFormat>(formats[i], 11));
        *out = opal::Texture::getSupportedSampleCount(values, requested);
#else
        throw Unsupported{};
#endif
    });
}

OpalResult OPAL_C_CALL opal_texture_update_data(OpalTexture *handle,
                                                const void *data, int32_t width,
                                                int32_t height,
                                                OpalTextureDataFormat format) {
    return guard(__func__, [&] {
        require(data && width > 0 && height > 0);
        object(handle)->updateData(
            data, width, height,
            checked_enum<opal::TextureDataFormat>(format, 6));
    });
}

OpalResult OPAL_C_CALL opal_texture_update_data_3d(
    OpalTexture *handle, const void *data, int32_t width, int32_t height,
    int32_t depth, OpalTextureDataFormat format) {
    return guard(__func__, [&] {
        require(data && width > 0 && height > 0);
        require(depth > 0);
        object(handle)->updateData3D(
            data, width, height, depth,
            checked_enum<opal::TextureDataFormat>(format, 6));
    });
}

OpalResult OPAL_C_CALL opal_texture_update_face(OpalTexture *handle,
                                                const void *data, int32_t face,
                                                int32_t width, int32_t height,
                                                OpalTextureDataFormat format) {
    return guard(__func__, [&] {
        require(data && width > 0 && height > 0);
        require(face >= 0 && face < 6);
        object(handle)->updateFace(
            face, data, width, height,
            checked_enum<opal::TextureDataFormat>(format, 6));
    });
}

OpalResult OPAL_C_CALL opal_texture_change_format(OpalTexture *handle,
                                                  OpalTextureFormat format) {
    return guard(__func__, [&] {
        object(handle)->changeFormat(
            checked_enum<opal::TextureFormat>(format, 11));
    });
}

OpalResult OPAL_C_CALL opal_texture_change_border_color(OpalTexture *handle,
                                                        const float color[4]) {
    return guard(__func__, [&] {
        require(color);
        object(handle)->changeBorderColor(
            glm::vec4(color[0], color[1], color[2], color[3]));
    });
}

OpalResult OPAL_C_CALL opal_texture_read_data(OpalTexture *handle, void *buffer,
                                              OpalTextureDataFormat format) {
    return guard(__func__, [&] {
        require(buffer);
        object(handle)->readData(
            buffer, checked_enum<opal::TextureDataFormat>(format, 6));
    });
}

OpalResult OPAL_C_CALL opal_texture_generate_mipmaps(OpalTexture *handle,
                                                     uint32_t levels) {
    return guard(__func__, [&] {
        require(levels > 0);
        object(handle)->generateMipmaps(levels);
    });
}

OpalResult OPAL_C_CALL
opal_texture_automatically_generate_mipmaps(OpalTexture *handle) {
    return guard(__func__,
                 [&] { object(handle)->automaticallyGenerateMipmaps(); });
}

OpalResult OPAL_C_CALL opal_texture_set_wrap_mode(OpalTexture *handle,
                                                  OpalTextureAxis axis,
                                                  OpalTextureWrapMode mode) {
    return guard(__func__, [&] {
        object(handle)->setWrapMode(
            checked_enum<opal::TextureAxis>(axis, 3),
            checked_enum<opal::TextureWrapMode>(mode, 4));
    });
}

OpalResult OPAL_C_CALL opal_texture_set_filter_mode(
    OpalTexture *handle, OpalTextureFilterMode min_filter,
    OpalTextureFilterMode mag_filter) {
    return guard(__func__, [&] {
        object(handle)->setFilterMode(
            checked_enum<opal::TextureFilterMode>(min_filter, 4),
            checked_enum<opal::TextureFilterMode>(mag_filter, 4));
    });
}

OpalResult OPAL_C_CALL opal_texture_set_parameters(
    OpalTexture *handle, OpalTextureWrapMode s, OpalTextureWrapMode t,
    OpalTextureFilterMode min_filter, OpalTextureFilterMode mag_filter) {
    return guard(__func__, [&] {
        object(handle)->setParameters(
            checked_enum<opal::TextureWrapMode>(s, 4),
            checked_enum<opal::TextureWrapMode>(t, 4),
            checked_enum<opal::TextureFilterMode>(min_filter, 4),
            checked_enum<opal::TextureFilterMode>(mag_filter, 4));
    });
}

OpalResult OPAL_C_CALL opal_texture_set_parameters_3d(
    OpalTexture *handle, OpalTextureWrapMode s, OpalTextureWrapMode t,
    OpalTextureWrapMode r, OpalTextureFilterMode min_filter,
    OpalTextureFilterMode mag_filter) {
    return guard(__func__, [&] {
        object(handle)->setParameters3D(
            checked_enum<opal::TextureWrapMode>(s, 4),
            checked_enum<opal::TextureWrapMode>(t, 4),
            checked_enum<opal::TextureWrapMode>(r, 4),
            checked_enum<opal::TextureFilterMode>(min_filter, 4),
            checked_enum<opal::TextureFilterMode>(mag_filter, 4));
    });
}

OpalResult OPAL_C_CALL opal_texture_get_info(
    OpalTexture *handle, OpalTextureType *type, OpalTextureFormat *format,
    int32_t *width, int32_t *height, uint32_t *mip_levels, int32_t *samples) {
    return guard(__func__, [&] {
        auto &t = object(handle);
        require(type && format && width && height && mip_levels && samples);
        *type = static_cast<int32_t>(t->type);
        *format = static_cast<int32_t>(t->format);
        *width = t->width;
        *height = t->height;
        *mip_levels = t->mipLevels;
        *samples = t->samples;
    });
}

OpalResult OPAL_C_CALL opal_texture_get_metal_texture(OpalTexture *handle,
                                                      void **out) {
    return guard(__func__, [&] {
        require(out);
        *out = nullptr;
#if defined(METAL)
        require(out);
        *out = object(handle)->getMetalTexture();
#else
        throw Unsupported{};
#endif
    });
}

OpalResult OPAL_C_CALL opal_shader_create_from_source(const char *source,
                                                      OpalShaderType type,
                                                      const char *entry,
                                                      OpalShader **out) {
    return guard(__func__, [&] {
        require(out);
        *out = nullptr;
        require(source);
        auto owner = current_device();
        output(out,
               opal::Shader::createFromSource(
                   source, checked_enum<opal::ShaderType>(type, 12),
                   entry ? entry : "main0"),
               std::move(owner));
    });
}

OpalResult OPAL_C_CALL opal_shader_for_function(OpalShader *shader,
                                                const char *name,
                                                OpalShaderType type,
                                                OpalShader **out) {
    return guard(__func__, [&] {
        require(out);
        *out = nullptr;
        require(name);
        output(out,
               object(shader)->forFunction(
                   name, checked_enum<opal::ShaderType>(type, 12)),
               ownership(shader));
    });
}

OpalResult OPAL_C_CALL opal_shader_compile(OpalShader *handle) {
    return guard(__func__, [&] { object(handle)->compile(); });
}

OpalResult OPAL_C_CALL opal_shader_get_status(OpalShader *handle,
                                              int32_t *out) {
    return guard(__func__, [&] {
        require(out);
        *out = object(handle)->getShaderStatus();
    });
}

OpalResult OPAL_C_CALL opal_shader_get_log(OpalShader *handle, char *buffer,
                                           size_t capacity) {
    return guard(__func__, [&] {
        require(buffer && capacity > 0);
        buffer[0] = 0;
        object(handle)->getShaderLog(buffer, capacity);
        buffer[capacity - 1] = 0;
    });
}

OpalResult OPAL_C_CALL opal_shader_program_link(OpalShaderProgram *handle) {
    return guard(__func__, [&] { object(handle)->link(); });
}

OpalResult OPAL_C_CALL opal_shader_program_get_status(OpalShaderProgram *handle,
                                                      int32_t *out) {
    return guard(__func__, [&] {
        require(out);
        *out = object(handle)->getProgramStatus();
    });
}

OpalResult OPAL_C_CALL opal_shader_program_get_log(OpalShaderProgram *handle,
                                                   char *buffer,
                                                   size_t capacity) {
    return guard(__func__, [&] {
        require(buffer && capacity > 0);
        buffer[0] = 0;
        object(handle)->getProgramLog(buffer, capacity);
        buffer[capacity - 1] = 0;
    });
}

OpalResult OPAL_C_CALL opal_shader_program_create(OpalShaderProgram **out) {
    return guard(__func__, [&] {
        require(out);
        *out = nullptr;
        auto owner = current_device();
        output(out, opal::ShaderProgram::create(), std::move(owner));
    });
}

OpalResult OPAL_C_CALL opal_shader_program_attach_shader(
    OpalShaderProgram *handle, OpalShader *shader, int32_t caller_id) {
    return guard(__func__, [&] {
        object(handle)->attachShader(object(shader), caller_id);
    });
}

OpalResult OPAL_C_CALL opal_shader_program_use(OpalShaderProgram *handle) {
    return guard(__func__, [&] {
#if defined(OPENGL)
        object(handle)->use();
#else
        throw Unsupported{};
#endif
    });
}

OpalResult OPAL_C_CALL opal_shader_program_is_compute_program(
    OpalShaderProgram *handle, int32_t *out) {
    return guard(__func__, [&] {
        require(out);
        *out = object(handle)->isComputeProgram();
    });
}

OpalResult OPAL_C_CALL opal_shader_program_is_ray_tracing_program(
    OpalShaderProgram *handle, int32_t *out) {
    return guard(__func__, [&] {
        require(out);
        *out = object(handle)->isRayTracingProgram();
    });
}

OpalResult OPAL_C_CALL opal_pipeline_create(OpalPipeline **out) {
    return guard(__func__, [&] {
        require(out);
        *out = nullptr;
        auto owner = current_device();
        output(out, opal::Pipeline::create(), std::move(owner));
    });
}

OpalResult OPAL_C_CALL opal_pipeline_set_shader_program(
    OpalPipeline *handle, OpalShaderProgram *program) {
    return guard(__func__,
                 [&] { object(handle)->setShaderProgram(object(program)); });
}

OpalResult OPAL_C_CALL opal_pipeline_set_vertex_attributes(
    OpalPipeline *handle, const OpalVertexAttribute *attributes, size_t count,
    const OpalVertexBinding *binding) {
    return guard(__func__, [&] {
        require(binding && (attributes || count == 0));
        std::vector<opal::VertexAttribute> values;
        for (size_t i = 0; i < count; ++i)
            values.push_back(attribute(attributes[i]));
        opal::VertexBinding b{
            binding->stride,
            checked_enum<opal::VertexBindingInputRate>(binding->input_rate, 2)};
        object(handle)->setVertexAttributes(values, b);
    });
}

OpalResult OPAL_C_CALL opal_pipeline_set_primitive_style(
    OpalPipeline *handle, OpalPrimitiveStyle value) {
    return guard(__func__, [&] {
        object(handle)->setPrimitiveStyle(
            checked_enum<opal::PrimitiveStyle>(value, 7));
    });
}

OpalResult OPAL_C_CALL opal_pipeline_set_rasterizer_mode(
    OpalPipeline *handle, OpalRasterizerMode value) {
    return guard(__func__, [&] {
        object(handle)->setRasterizerMode(
            checked_enum<opal::RasterizerMode>(value, 3));
    });
}

OpalResult OPAL_C_CALL opal_pipeline_set_cull_mode(OpalPipeline *handle,
                                                   OpalCullMode value) {
    return guard(__func__, [&] {
        object(handle)->setCullMode(checked_enum<opal::CullMode>(value, 4));
    });
}

OpalResult OPAL_C_CALL opal_pipeline_set_front_face(OpalPipeline *handle,
                                                    OpalFrontFace value) {
    return guard(__func__, [&] {
        object(handle)->setFrontFace(checked_enum<opal::FrontFace>(value, 2));
    });
}

OpalResult OPAL_C_CALL opal_pipeline_set_depth_compare_op(OpalPipeline *handle,
                                                          OpalCompareOp value) {
    return guard(__func__, [&] {
        object(handle)->setDepthCompareOp(
            checked_enum<opal::CompareOp>(value, 8));
    });
}

OpalResult OPAL_C_CALL opal_pipeline_set_blend_equation(
    OpalPipeline *handle, OpalBlendEquation value) {
    return guard(__func__, [&] {
        object(handle)->setBlendEquation(
            checked_enum<opal::BlendEquation>(value, 5));
    });
}

OpalResult OPAL_C_CALL opal_pipeline_set_logic_op(OpalPipeline *handle,
                                                  OpalLogicOp value) {
    return guard(__func__, [&] {
        object(handle)->setLogicOp(checked_enum<opal::LogicOp>(value, 16));
    });
}

OpalResult OPAL_C_CALL opal_pipeline_enable_depth_test(OpalPipeline *handle,
                                                       int32_t enabled) {
    return guard(__func__,
                 [&] { object(handle)->enableDepthTest(enabled != 0); });
}

OpalResult OPAL_C_CALL opal_pipeline_enable_depth_write(OpalPipeline *handle,
                                                        int32_t enabled) {
    return guard(__func__,
                 [&] { object(handle)->enableDepthWrite(enabled != 0); });
}

OpalResult OPAL_C_CALL opal_pipeline_enable_blending(OpalPipeline *handle,
                                                     int32_t enabled) {
    return guard(__func__,
                 [&] { object(handle)->enableBlending(enabled != 0); });
}

OpalResult OPAL_C_CALL opal_pipeline_enable_logic_op(OpalPipeline *handle,
                                                     int32_t enabled) {
    return guard(__func__,
                 [&] { object(handle)->enableLogicOp(enabled != 0); });
}

OpalResult OPAL_C_CALL opal_pipeline_enable_multisampling(OpalPipeline *handle,
                                                          int32_t enabled) {
    return guard(__func__,
                 [&] { object(handle)->enableMultisampling(enabled != 0); });
}

OpalResult OPAL_C_CALL opal_pipeline_enable_polygon_offset(OpalPipeline *handle,
                                                           int32_t enabled) {
    return guard(__func__,
                 [&] { object(handle)->enablePolygonOffset(enabled != 0); });
}

OpalResult OPAL_C_CALL opal_pipeline_set_patch_vertices(OpalPipeline *handle,
                                                        int32_t count) {
    return guard(__func__, [&] {
        require(count > 0);
        object(handle)->setPatchVertices(count);
    });
}

OpalResult OPAL_C_CALL opal_pipeline_set_viewport(OpalPipeline *handle,
                                                  int32_t x, int32_t y,
                                                  int32_t width,
                                                  int32_t height) {
    return guard(__func__, [&] {
        require(width >= 0 && height >= 0);
        object(handle)->setViewport(x, y, width, height);
    });
}

OpalResult OPAL_C_CALL opal_pipeline_set_line_width(OpalPipeline *handle,
                                                    float width) {
    return guard(__func__, [&] {
        require(width > 0);
        object(handle)->setLineWidth(width);
    });
}

OpalResult OPAL_C_CALL opal_pipeline_set_compute_threadgroup_size(
    OpalPipeline *handle, uint32_t x, uint32_t y, uint32_t z) {
    return guard(__func__, [&] {
        require(x && y && z);
        object(handle)->setComputeThreadgroupSize(x, y, z);
    });
}

OpalResult OPAL_C_CALL opal_pipeline_set_blend_func(OpalPipeline *handle,
                                                    OpalBlendFunc src,
                                                    OpalBlendFunc dst) {
    return guard(__func__, [&] {
        object(handle)->setBlendFunc(checked_enum<opal::BlendFunc>(src, 10),
                                     checked_enum<opal::BlendFunc>(dst, 10));
    });
}

OpalResult OPAL_C_CALL opal_pipeline_set_color_write_mask(OpalPipeline *handle,
                                                          int32_t red,
                                                          int32_t green,
                                                          int32_t blue,
                                                          int32_t alpha) {
    return guard(__func__, [&] {
        object(handle)->setColorWriteMask(red != 0, green != 0, blue != 0,
                                          alpha != 0);
    });
}

OpalResult OPAL_C_CALL opal_pipeline_set_polygon_offset(OpalPipeline *handle,
                                                        float factor,
                                                        float units) {
    return guard(__func__,
                 [&] { object(handle)->setPolygonOffset(factor, units); });
}

OpalResult OPAL_C_CALL opal_pipeline_enable_clip_distance(OpalPipeline *handle,
                                                          int32_t index,
                                                          int32_t enabled) {
    return guard(__func__, [&] {
        require(index >= 0);
        object(handle)->enableClipDistance(index, enabled != 0);
    });
}

OpalResult OPAL_C_CALL opal_pipeline_get_patch_vertices(OpalPipeline *handle,
                                                        int32_t *out) {
    return guard(__func__, [&] {
        require(out);
        *out = object(handle)->getPatchVertices();
    });
}

OpalResult OPAL_C_CALL opal_pipeline_get_compute_threadgroup_size_x(
    OpalPipeline *handle, uint32_t *out) {
    return guard(__func__, [&] {
        require(out);
        *out = object(handle)->getComputeThreadgroupSizeX();
    });
}

OpalResult OPAL_C_CALL opal_pipeline_get_compute_threadgroup_size_y(
    OpalPipeline *handle, uint32_t *out) {
    return guard(__func__, [&] {
        require(out);
        *out = object(handle)->getComputeThreadgroupSizeY();
    });
}

OpalResult OPAL_C_CALL opal_pipeline_get_compute_threadgroup_size_z(
    OpalPipeline *handle, uint32_t *out) {
    return guard(__func__, [&] {
        require(out);
        *out = object(handle)->getComputeThreadgroupSizeZ();
    });
}

OpalResult OPAL_C_CALL opal_pipeline_build(OpalPipeline *handle) {
    return guard(__func__, [&] { object(handle)->build(); });
}

OpalResult OPAL_C_CALL opal_pipeline_bind(OpalPipeline *handle) {
    return guard(__func__, [&] { object(handle)->bind(); });
}

OpalResult OPAL_C_CALL opal_pipeline_set_uniform1i(OpalPipeline *handle,
                                                   const char *name,
                                                   int32_t value) {
    return guard(__func__, [&] {
        require(name);
        object(handle)->setUniform1i(name, value);
    });
}

OpalResult OPAL_C_CALL opal_pipeline_set_uniform1f(OpalPipeline *handle,
                                                   const char *name,
                                                   float value) {
    return guard(__func__, [&] {
        require(name);
        object(handle)->setUniform1f(name, value);
    });
}

OpalResult OPAL_C_CALL opal_pipeline_set_uniform_bool(OpalPipeline *handle,
                                                      const char *name,
                                                      int32_t value) {
    return guard(__func__, [&] {
        require(name);
        object(handle)->setUniformBool(name, value != 0);
    });
}

OpalResult OPAL_C_CALL opal_pipeline_set_uniform2f(OpalPipeline *handle,
                                                   const char *name, float x,
                                                   float y) {
    return guard(__func__, [&] {
        require(name);
        object(handle)->setUniform2f(name, x, y);
    });
}

OpalResult OPAL_C_CALL opal_pipeline_set_uniform3f(OpalPipeline *handle,
                                                   const char *name, float x,
                                                   float y, float z) {
    return guard(__func__, [&] {
        require(name);
        object(handle)->setUniform3f(name, x, y, z);
    });
}

OpalResult OPAL_C_CALL opal_pipeline_set_uniform4f(OpalPipeline *handle,
                                                   const char *name, float x,
                                                   float y, float z, float w) {
    return guard(__func__, [&] {
        require(name);
        object(handle)->setUniform4f(name, x, y, z, w);
    });
}

OpalResult OPAL_C_CALL opal_pipeline_set_uniform_mat4f(OpalPipeline *handle,
                                                       const char *name,
                                                       const float values[16]) {
    return guard(__func__, [&] {
        require(name);
        object(handle)->setUniformMat4f(name, matrix(values));
    });
}

OpalResult OPAL_C_CALL opal_pipeline_bind_buffer_data(OpalPipeline *handle,
                                                      const char *name,
                                                      const void *data,
                                                      size_t size) {
    return guard(__func__, [&] {
        require(name && data && size > 0);
        object(handle)->bindBufferData(name, data, size);
    });
}

OpalResult OPAL_C_CALL opal_pipeline_bind_buffer(OpalPipeline *handle,
                                                 const char *name,
                                                 OpalBuffer *buffer,
                                                 int32_t caller_id) {
    return guard(__func__, [&] {
        require(name);
        object(handle)->bindBuffer(name, object(buffer), caller_id);
    });
}

OpalResult OPAL_C_CALL opal_pipeline_bind_shader_read_write_buffer(
    OpalPipeline *handle, const char *name, OpalBuffer *buffer,
    int32_t caller_id) {
    return guard(__func__, [&] {
        require(name);
        object(handle)->bindShaderReadWriteBuffer(name, object(buffer),
                                                  caller_id);
    });
}

OpalResult OPAL_C_CALL opal_pipeline_bind_texture(OpalPipeline *handle,
                                                  const char *name,
                                                  OpalTexture *texture,
                                                  int32_t unit,
                                                  int32_t caller_id) {
    return guard(__func__, [&] {
        require(name && unit >= 0);
        object(handle)->bindTexture(name, object(texture), unit, caller_id);
    });
}

OpalResult OPAL_C_CALL opal_pipeline_bind_texture_2d(OpalPipeline *handle,
                                                     const char *name,
                                                     uint32_t texture_id,
                                                     int32_t unit,
                                                     int32_t caller_id) {
    return guard(__func__, [&] {
        require(name && unit >= 0);
        object(handle)->bindTexture2D(name, texture_id, unit, caller_id);
    });
}

OpalResult OPAL_C_CALL opal_pipeline_bind_texture_3d(OpalPipeline *handle,
                                                     const char *name,
                                                     uint32_t texture_id,
                                                     int32_t unit,
                                                     int32_t caller_id) {
    return guard(__func__, [&] {
        require(name && unit >= 0);
        object(handle)->bindTexture3D(name, texture_id, unit, caller_id);
    });
}

OpalResult OPAL_C_CALL opal_pipeline_bind_texture_cubemap(OpalPipeline *handle,
                                                          const char *name,
                                                          uint32_t texture_id,
                                                          int32_t unit,
                                                          int32_t caller_id) {
    return guard(__func__, [&] {
        require(name && unit >= 0);
        object(handle)->bindTextureCubemap(name, texture_id, unit, caller_id);
    });
}

OpalResult OPAL_C_CALL opal_pipeline_bind_texture_array(
    OpalPipeline *handle, OpalTexture *const *textures, size_t count,
    uint32_t buffer_index) {
    return guard(__func__, [&] {
        require(textures || !count);
        std::vector<std::shared_ptr<opal::Texture>> values;
        for (size_t i = 0; i < count; ++i)
            values.push_back(object(textures[i]));
        object(handle)->bindTextureArray(values, buffer_index);
    });
}

OpalResult OPAL_C_CALL opal_buffer_create(OpalBufferUsage usage, size_t size,
                                          const void *data,
                                          OpalMemoryUsageType memory_usage,
                                          int32_t caller_id, OpalBuffer **out) {
    return guard(__func__, [&] {
        require(out);
        *out = nullptr;
        require(size > 0);
        auto owner = current_device();
        output(out,
               opal::Buffer::create(
                   checked_enum<opal::BufferUsage>(usage, 7), size, data,
                   checked_enum<opal::MemoryUsageType>(memory_usage, 3),
                   caller_id),
               std::move(owner));
    });
}

OpalResult OPAL_C_CALL opal_buffer_update_data(OpalBuffer *handle,
                                               size_t offset, size_t size,
                                               const void *data) {
    return guard(__func__, [&] {
        require(data && size > 0);
        object(handle)->updateData(offset, size, data);
    });
}

OpalResult OPAL_C_CALL opal_buffer_bind(OpalBuffer *handle, int32_t caller_id) {
    return guard(__func__, [&] { object(handle)->bind(caller_id); });
}

OpalResult OPAL_C_CALL opal_buffer_unbind(OpalBuffer *handle,
                                          int32_t caller_id) {
    return guard(__func__, [&] { object(handle)->unbind(caller_id); });
}

OpalResult OPAL_C_CALL opal_drawing_state_create(OpalBuffer *vertex_buffer,
                                                 OpalBuffer *index_buffer,
                                                 OpalDrawingState **out) {
    return guard(__func__, [&] {
        require(out);
        *out = nullptr;
        auto owner = current_device();
        output(out,
               opal::DrawingState::create(object(vertex_buffer),
                                          index_buffer ? object(index_buffer)
                                                       : nullptr),
               std::move(owner));
    });
}

OpalResult OPAL_C_CALL opal_drawing_state_set_buffers(
    OpalDrawingState *handle, OpalBuffer *vertex_buffer,
    OpalBuffer *index_buffer) {
    return guard(__func__, [&] {
        object(handle)->setBuffers(object(vertex_buffer),
                                   index_buffer ? object(index_buffer)
                                                : nullptr);
    });
}

OpalResult OPAL_C_CALL opal_drawing_state_set_instance_buffer(
    OpalDrawingState *handle, OpalBuffer *buffer) {
    return guard(__func__, [&] {
        object(handle)->instanceBuffer = buffer ? object(buffer) : nullptr;
    });
}

OpalResult OPAL_C_CALL opal_drawing_state_bind(OpalDrawingState *handle) {
    return guard(__func__, [&] { object(handle)->bind(); });
}

OpalResult OPAL_C_CALL opal_drawing_state_unbind(OpalDrawingState *handle) {
    return guard(__func__, [&] { object(handle)->unbind(); });
}

OpalResult OPAL_C_CALL opal_drawing_state_configure_attributes(
    OpalDrawingState *handle, const OpalVertexAttributeBinding *bindings,
    size_t count) {
    return guard(__func__, [&] {
        require(bindings || !count);
        std::vector<opal::VertexAttributeBinding> values;
        for (size_t i = 0; i < count; ++i)
            values.push_back({attribute(bindings[i].attribute),
                              bindings[i].source_buffer
                                  ? object(bindings[i].source_buffer)
                                  : nullptr});
        object(handle)->configureAttributes(values);
    });
}

OpalResult OPAL_C_CALL opal_depth_stencil_buffer_create(
    int32_t width, int32_t height, OpalTextureFormat format,
    OpalDepthStencilBuffer **out) {
    return guard(__func__, [&] {
        require(out);
        *out = nullptr;
        require(width > 0 && height > 0);
        auto owner = current_device();
        output(
            out,
            opal::DepthStencilBuffer::create(
                width, height, checked_enum<opal::TextureFormat>(format, 11)),
            std::move(owner));
    });
}

OpalResult OPAL_C_CALL opal_texture_get_depth_stencil_texture(
    OpalDepthStencilBuffer *buffer, OpalTexture **out) {
    return guard(__func__, [&] {
        require(out);
        *out = nullptr;
        output(out, object(buffer)->texture, ownership(buffer));
    });
}

OpalResult OPAL_C_CALL opal_framebuffer_create(int32_t width, int32_t height,
                                               OpalFramebuffer **out) {
    return guard(__func__, [&] {
        require(out);
        *out = nullptr;
        require(width > 0 && height > 0);
        auto owner = current_device();
        output(out, opal::Framebuffer::create(width, height), std::move(owner));
    });
}

OpalResult OPAL_C_CALL opal_framebuffer_create_empty(OpalFramebuffer **out) {
    return guard(__func__, [&] {
        require(out);
        *out = nullptr;
        auto owner = current_device();
        output(out, opal::Framebuffer::create(), std::move(owner));
    });
}

OpalResult OPAL_C_CALL opal_framebuffer_add_attachment(OpalFramebuffer *handle,
                                                       OpalAttachmentType type,
                                                       OpalTexture *texture) {
    return guard(__func__, [&] {
        object(handle)->addAttachment(
            {checked_enum<opal::Attachment::Type>(type, 4), object(texture)});
    });
}

OpalResult OPAL_C_CALL opal_framebuffer_attach_depth_stencil_buffer(
    OpalFramebuffer *handle, OpalDepthStencilBuffer *buffer) {
    return guard(__func__, [&] {
        object(handle)->attachDepthStencilBuffer(object(buffer));
    });
}

OpalResult OPAL_C_CALL opal_framebuffer_attach_texture(OpalFramebuffer *handle,
                                                       OpalTexture *texture,
                                                       int32_t index) {
    return guard(__func__, [&] {
        require(index >= 0);
        object(handle)->attachTexture(object(texture), index);
    });
}

OpalResult OPAL_C_CALL opal_framebuffer_attach_cubemap(
    OpalFramebuffer *handle, OpalTexture *texture, OpalAttachmentType type) {
    return guard(__func__, [&] {
        object(handle)->attachCubemap(
            object(texture), checked_enum<opal::Attachment::Type>(type, 4));
    });
}

OpalResult OPAL_C_CALL opal_framebuffer_attach_cubemap_face(
    OpalFramebuffer *handle, OpalTexture *texture, int32_t face,
    OpalAttachmentType type) {
    return guard(__func__, [&] {
        require(face >= 0 && face < 6);
        object(handle)->attachCubemapFace(
            object(texture), face,
            checked_enum<opal::Attachment::Type>(type, 4));
    });
}

OpalResult OPAL_C_CALL
opal_framebuffer_disable_color_buffer(OpalFramebuffer *handle) {
    return guard(__func__, [&] { object(handle)->disableColorBuffer(); });
}

OpalResult OPAL_C_CALL opal_framebuffer_set_viewport(OpalFramebuffer *handle) {
    return guard(__func__, [&] { object(handle)->setViewport(); });
}

OpalResult OPAL_C_CALL opal_framebuffer_bind(OpalFramebuffer *handle) {
    return guard(__func__, [&] { object(handle)->bind(); });
}

OpalResult OPAL_C_CALL opal_framebuffer_unbind(OpalFramebuffer *handle) {
    return guard(__func__, [&] { object(handle)->unbind(); });
}

OpalResult OPAL_C_CALL opal_framebuffer_bind_for_read(OpalFramebuffer *handle) {
    return guard(__func__, [&] { object(handle)->bindForRead(); });
}

OpalResult OPAL_C_CALL opal_framebuffer_bind_for_draw(OpalFramebuffer *handle) {
    return guard(__func__, [&] { object(handle)->bindForDraw(); });
}

OpalResult OPAL_C_CALL
opal_framebuffer_set_custom_viewport(OpalFramebuffer *handle, int32_t x,
                                     int32_t y, int32_t width, int32_t height) {
    return guard(__func__, [&] {
        require(width >= 0 && height >= 0);
        object(handle)->setViewport(x, y, width, height);
    });
}

OpalResult OPAL_C_CALL
opal_framebuffer_set_draw_buffers(OpalFramebuffer *handle, int32_t count) {
    return guard(__func__, [&] {
        require(count >= 0);
        object(handle)->setDrawBuffers(count);
    });
}

OpalResult OPAL_C_CALL
opal_framebuffer_get_draw_buffer_count(OpalFramebuffer *handle, int32_t *out) {
    return guard(__func__, [&] {
        require(out);
        *out = object(handle)->getDrawBufferCount();
    });
}

OpalResult OPAL_C_CALL opal_framebuffer_get_status(OpalFramebuffer *handle,
                                                   int32_t *out) {
    return guard(__func__, [&] {
        require(out);
        *out = object(handle)->getStatus();
    });
}

OpalResult OPAL_C_CALL opal_framebuffer_get_size(OpalFramebuffer *handle,
                                                 int32_t *width,
                                                 int32_t *height) {
    return guard(__func__, [&] {
        require(width && height);
        auto &f = object(handle);
        *width = f->width;
        *height = f->height;
    });
}

OpalResult OPAL_C_CALL opal_render_pass_create(OpalRenderPass **out) {
    return guard(__func__, [&] {
        require(out);
        *out = nullptr;
        auto owner = current_device();
        output(out, opal::RenderPass::create(), std::move(owner));
    });
}

OpalResult OPAL_C_CALL opal_render_pass_set_framebuffer(
    OpalRenderPass *handle, OpalFramebuffer *framebuffer) {
    return guard(__func__,
                 [&] { object(handle)->setFramebuffer(object(framebuffer)); });
}

OpalResult OPAL_C_CALL opal_resolve_action_create(OpalFramebuffer *source,
                                                  OpalFramebuffer *destination,
                                                  OpalResolveAction **out) {
    return guard(__func__, [&] {
        require(out);
        *out = nullptr;
        auto owner = current_device();
        output(out,
               opal::ResolveAction::create(object(source), object(destination)),
               std::move(owner));
    });
}

OpalResult OPAL_C_CALL opal_resolve_action_create_for_color_attachment(
    OpalFramebuffer *source, OpalFramebuffer *destination, int32_t index,
    OpalResolveAction **out) {
    return guard(__func__, [&] {
        require(out);
        *out = nullptr;
        require(index >= 0);
        auto owner = current_device();
        output(out,
               opal::ResolveAction::createForColorAttachment(
                   object(source), object(destination), index),
               std::move(owner));
    });
}

OpalResult OPAL_C_CALL opal_resolve_action_create_for_depth(
    OpalFramebuffer *source, OpalFramebuffer *destination,
    OpalResolveAction **out) {
    return guard(__func__, [&] {
        require(out);
        *out = nullptr;
        auto owner = current_device();
        output(out,
               opal::ResolveAction::createForDepth(object(source),
                                                   object(destination)),
               std::move(owner));
    });
}

OpalResult OPAL_C_CALL
opal_resolve_action_set_options(OpalResolveAction *handle, int32_t color_index,
                                int32_t resolve_depth, int32_t resolve_color) {
    return guard(__func__, [&] {
        require(color_index >= -1);
        auto &a = object(handle);
        a->colorAttachmentIndex = color_index;
        a->resolveDepth = resolve_depth != 0;
        a->resolveColor = resolve_color != 0;
    });
}

OpalResult OPAL_C_CALL opal_command_buffer_start(OpalCommandBuffer *handle) {
    return guard(__func__, [&] {
        object(handle);
        require(!handle->recording, "Command buffer is already recording");
        handle->value->start();
        handle->recording = true;
        handle->pass_active = false;
        handle->pipeline_bound = false;
    });
}

OpalResult OPAL_C_CALL opal_command_buffer_end_pass(OpalCommandBuffer *handle) {
    return guard(__func__, [&] {
        object(handle);
        require(handle->recording,
                "Start command recording before ending encoders");
        handle->value->endPass();
        handle->pass_active = false;
    });
}

OpalResult OPAL_C_CALL opal_command_buffer_commit(OpalCommandBuffer *handle) {
    return guard(__func__, [&] {
        object(handle);
        require(handle->recording, "Start command recording before committing");
        handle->value->commit();
        handle->recording = false;
        handle->pass_active = false;
    });
}

OpalResult OPAL_C_CALL
opal_command_buffer_wait_for_submitted_work(OpalCommandBuffer *handle) {
    return guard(__func__, [&] { object(handle)->waitForSubmittedWork(); });
}

OpalResult OPAL_C_CALL
opal_command_buffer_unbind_pipeline(OpalCommandBuffer *handle) {
    return guard(__func__, [&] {
        object(handle)->unbindPipeline();
        handle->pipeline_bound = false;
    });
}

OpalResult OPAL_C_CALL
opal_command_buffer_reset_scissor(OpalCommandBuffer *handle) {
    return guard(__func__, [&] { object(handle)->resetScissor(); });
}

OpalResult OPAL_C_CALL
opal_command_buffer_unbind_drawing_state(OpalCommandBuffer *handle) {
    return guard(__func__, [&] { object(handle)->unbindDrawingState(); });
}

OpalResult OPAL_C_CALL
opal_command_buffer_compute_barrier(OpalCommandBuffer *handle) {
    return guard(__func__, [&] { object(handle)->computeBarrier(); });
}

OpalResult OPAL_C_CALL opal_command_buffer_begin_pass(OpalCommandBuffer *handle,
                                                      OpalRenderPass *pass) {
    return guard(__func__, [&] {
        object(handle);
        require(handle->recording && !handle->pass_active,
                "Begin a pass only while recording, outside another pass");
        require(object(pass)->framebuffer != nullptr,
                "Render pass has no framebuffer");
        handle->value->beginPass(object(pass));
        handle->pass_active = true;
    });
}

OpalResult OPAL_C_CALL opal_command_buffer_bind_pipeline(
    OpalCommandBuffer *handle, OpalPipeline *pipeline) {
    return guard(__func__, [&] {
        object(handle);
        require(handle->recording,
                "Start command recording before binding a pipeline");
        require(object(pipeline)->shaderProgram != nullptr,
                "Pipeline has no shader program");
        handle->value->bindPipeline(object(pipeline));
        handle->pipeline_bound = true;
    });
}

OpalResult OPAL_C_CALL opal_command_buffer_bind_drawing_state(
    OpalCommandBuffer *handle, OpalDrawingState *state) {
    return guard(__func__,
                 [&] { object(handle)->bindDrawingState(object(state)); });
}

OpalResult OPAL_C_CALL opal_command_buffer_generate_mipmaps(
    OpalCommandBuffer *handle, OpalTexture *texture) {
    return guard(__func__,
                 [&] { object(handle)->generateMipmaps(object(texture)); });
}

OpalResult OPAL_C_CALL opal_command_buffer_perform_resolve(
    OpalCommandBuffer *handle, OpalResolveAction *action) {
    return guard(__func__,
                 [&] { object(handle)->performResolve(object(action)); });
}

OpalResult OPAL_C_CALL opal_command_buffer_begin_sampled(
    OpalCommandBuffer *handle, OpalFramebuffer *read_framebuffer,
    OpalFramebuffer *write_framebuffer) {
    return guard(__func__, [&] {
        object(handle)->beginSampled(object(read_framebuffer),
                                     object(write_framebuffer));
    });
}

OpalResult OPAL_C_CALL
opal_command_buffer_set_scissor(OpalCommandBuffer *handle, int32_t x, int32_t y,
                                int32_t width, int32_t height) {
    return guard(__func__, [&] {
        require(width >= 0 && height >= 0);
        object(handle)->setScissor(x, y, width, height);
    });
}

OpalResult OPAL_C_CALL opal_command_buffer_draw(
    OpalCommandBuffer *handle, uint32_t vertex_count, uint32_t instance_count,
    uint32_t first_vertex, uint32_t first_instance, int32_t object_id) {
    return guard(__func__, [&] {
        object(handle);
        require(handle->recording && handle->pass_active &&
                    handle->pipeline_bound,
                "Drawing requires recording, an active render pass, and a "
                "bound pipeline");
        object(handle)->draw(vertex_count, instance_count, first_vertex,
                             first_instance, object_id);
    });
}

OpalResult OPAL_C_CALL opal_command_buffer_draw_indexed(
    OpalCommandBuffer *handle, uint32_t index_count, uint32_t instance_count,
    uint32_t first_index, int32_t vertex_offset, uint32_t first_instance,
    int32_t object_id) {
    return guard(__func__, [&] {
        object(handle);
        require(handle->recording && handle->pass_active &&
                    handle->pipeline_bound,
                "Drawing requires recording, an active render pass, and a "
                "bound pipeline");
        object(handle)->drawIndexed(index_count, instance_count, first_index,
                                    vertex_offset, first_instance, object_id);
    });
}

OpalResult OPAL_C_CALL opal_command_buffer_draw_patches(
    OpalCommandBuffer *handle, uint32_t vertex_count, uint32_t first_vertex,
    int32_t object_id) {
    return guard(__func__, [&] {
        object(handle);
        require(handle->recording && handle->pass_active &&
                    handle->pipeline_bound,
                "Drawing requires recording, an active render pass, and a "
                "bound pipeline");
        object(handle)->drawPatches(vertex_count, first_vertex, object_id);
    });
}

OpalResult OPAL_C_CALL opal_command_buffer_dispatch(OpalCommandBuffer *handle,
                                                    uint32_t x, uint32_t y,
                                                    uint32_t z) {
    return guard(__func__, [&] {
        require(x && y && z);
#if defined(OPENGL)
        throw Unsupported{};
#else
        object(handle);
        require(handle->recording && handle->pipeline_bound,
                "Dispatch requires recording and a bound pipeline");
        object(handle)->dispatch(x, y, z);
#endif
    });
}

OpalResult OPAL_C_CALL
opal_command_buffer_dispatch_rays(OpalCommandBuffer *handle, uint32_t width,
                                  uint32_t height, uint32_t depth) {
    return guard(__func__, [&] {
        require(width && height && depth);
#if defined(VULKAN)
        if (!ownership(handle)->supportsRayTracing())
            throw Unsupported{};
        object(handle);
        require(handle->recording && handle->pipeline_bound,
                "Dispatch requires recording and a bound pipeline");
        object(handle)->dispatchRays(width, height, depth);
#else
        throw Unsupported{};
#endif
    });
}

OpalResult OPAL_C_CALL opal_command_buffer_clear_color(
    OpalCommandBuffer *handle, float r, float g, float b, float a) {
    return guard(__func__, [&] { object(handle)->clearColor(r, g, b, a); });
}

OpalResult OPAL_C_CALL
opal_command_buffer_clear_depth(OpalCommandBuffer *handle, float depth) {
    return guard(__func__, [&] { object(handle)->clearDepth(depth); });
}

OpalResult OPAL_C_CALL opal_command_buffer_clear(OpalCommandBuffer *handle,
                                                 float r, float g, float b,
                                                 float a, float depth) {
    return guard(__func__, [&] { object(handle)->clear(r, g, b, a, depth); });
}

OpalResult OPAL_C_CALL opal_command_buffer_perform_spatial_upscale(
    OpalCommandBuffer *handle, OpalTexture *source, int32_t *out) {
    return guard(__func__, [&] {
        require(out);
        *out = object(handle)->performSpatialUpscale(object(source));
    });
}

OpalResult OPAL_C_CALL opal_command_buffer_get_and_reset_draw_call_count(
    OpalCommandBuffer *handle, int32_t *out) {
    return guard(__func__, [&] {
        require(out);
        *out = object(handle)->getAndResetDrawCallCount();
    });
}

#if defined(METAL) || defined(VULKAN)
namespace {
std::vector<opal::AccelerationStructureInstance>
instances(const OpalAccelerationStructureInstance *values, size_t count) {
    require(values || !count);
    std::vector<opal::AccelerationStructureInstance> result;
    for (size_t i = 0; i < count; ++i) {
        const auto &v = values[i];
        result.push_back({object(v.blas), matrix(v.transform), v.instance_id,
                          v.mask, v.cull_disable != 0});
    }
    return result;
}
}
#endif

OpalResult OPAL_C_CALL opal_primitive_acceleration_structure_create(
    const OpalPrimitiveVertex *vertices, size_t vertex_count,
    const uint32_t *indices, size_t index_count,
    OpalPrimitiveAccelerationStructure **out) {
    return guard(__func__, [&] {
        require(out);
        *out = nullptr;
#if defined(METAL) || defined(VULKAN)
        require(out);
        *out = nullptr;
        require(vertices && vertex_count > 0 && indices && index_count > 0 &&
                index_count % 3 == 0);
        std::vector<opal::PrimitiveVertex> values(vertex_count);
        for (size_t i = 0; i < vertex_count; ++i) {
            const auto &a = vertices[i];
            values[i] = {{a.position[0], a.position[1], a.position[2]},
                         {a.normal[0], a.normal[1], a.normal[2]},
                         {a.tangent[0], a.tangent[1], a.tangent[2]},
                         {a.bitangent[0], a.bitangent[1], a.bitangent[2]},
                         {a.uv[0], a.uv[1]}};
        }
        for (size_t i = 0; i < index_count; ++i)
            require(indices[i] < vertex_count);
        auto owner = current_device();
        output(out,
               opal::PrimitiveAccelerationStructure::create(
                   values, copy_array(indices, index_count)),
               std::move(owner));
#else
        throw Unsupported{};
#endif
    });
}

OpalResult OPAL_C_CALL opal_primitive_acceleration_structure_create_positions(
    const float *positions, size_t position_count, const uint32_t *indices,
    size_t index_count, OpalPrimitiveAccelerationStructure **out) {
    return guard(__func__, [&] {
        require(out);
        *out = nullptr;
#if defined(METAL) || defined(VULKAN)
        require(out);
        *out = nullptr;
        require(positions && position_count > 0 && position_count % 3 == 0 &&
                indices && index_count > 0 && index_count % 3 == 0);
        for (size_t i = 0; i < index_count; ++i)
            require(indices[i] < position_count / 3);
        auto owner = current_device();
        output(out,
               opal::PrimitiveAccelerationStructure::create(
                   copy_array(positions, position_count),
                   copy_array(indices, index_count)),
               std::move(owner));
#else
        throw Unsupported{};
#endif
    });
}

OpalResult OPAL_C_CALL opal_primitive_acceleration_structure_create_geometries(
    const OpalGeometry *geometries, size_t count,
    OpalPrimitiveAccelerationStructure **out) {
    return guard(__func__, [&] {
        require(out);
        *out = nullptr;
#if defined(METAL) || defined(VULKAN)
        require(out);
        *out = nullptr;
        require(geometries && count > 0);
        std::vector<std::vector<float>> positions;
        std::vector<std::vector<uint32_t>> indices;
        for (size_t i = 0; i < count; ++i) {
            const auto &g = geometries[i];
            require(g.positions && g.position_count > 0 &&
                    g.position_count % 3 == 0 && g.indices &&
                    g.index_count > 0 && g.index_count % 3 == 0);
            for (size_t j = 0; j < g.index_count; ++j)
                require(g.indices[j] < g.position_count / 3);
            positions.push_back(copy_array(g.positions, g.position_count));
            indices.push_back(copy_array(g.indices, g.index_count));
        }
        auto owner = current_device();
        output(out,
               opal::PrimitiveAccelerationStructure::create(positions, indices),
               std::move(owner));
#else
        throw Unsupported{};
#endif
    });
}

OpalResult OPAL_C_CALL opal_instance_acceleration_structure_create(
    const OpalAccelerationStructureInstance *values, size_t count,
    OpalInstanceAccelerationStructure **out) {
    return guard(__func__, [&] {
        require(out);
        *out = nullptr;
#if defined(METAL) || defined(VULKAN)
        require(out);
        *out = nullptr;
        auto owner = current_device();
        output(out,
               opal::InstanceAccelerationStructure::create(
                   instances(values, count)),
               std::move(owner));
#else
        throw Unsupported{};
#endif
    });
}

OpalResult OPAL_C_CALL opal_primitive_acceleration_structure_is_built(
    OpalPrimitiveAccelerationStructure *handle, int32_t *out) {
    return guard(__func__, [&] {
#if defined(METAL) || defined(VULKAN)
        require(out);
        *out = object(handle)->isBuilt;
#else
        throw Unsupported{};
#endif
    });
}

OpalResult OPAL_C_CALL opal_primitive_acceleration_structure_native_handle(
    OpalPrimitiveAccelerationStructure *handle, uint64_t *out) {
    return guard(__func__, [&] {
#if defined(VULKAN)
        require(out);
        *out = object(handle)->nativeHandle();
#else
        throw Unsupported{};
#endif
    });
}

OpalResult OPAL_C_CALL
opal_command_buffer_build_primitive_acceleration_structure(
    OpalCommandBuffer *handle, OpalPrimitiveAccelerationStructure *structure) {
    return guard(__func__, [&] {
#if defined(METAL) || defined(VULKAN)
        object(handle)->buildPrimitiveAccelerationStructure(object(structure));
#else
        throw Unsupported{};
#endif
    });
}

OpalResult OPAL_C_CALL
opal_command_buffer_bind_primitive_acceleration_structure(
    OpalCommandBuffer *handle, OpalPrimitiveAccelerationStructure *structure,
    uint32_t binding) {
    return guard(__func__, [&] {
#if defined(METAL)
        object(handle)->bindPrimitiveAccelerationStructure(object(structure),
                                                           binding);
#else
        throw Unsupported{};
#endif
    });
}

OpalResult OPAL_C_CALL opal_instance_acceleration_structure_is_built(
    OpalInstanceAccelerationStructure *handle, int32_t *out) {
    return guard(__func__, [&] {
#if defined(METAL) || defined(VULKAN)
        require(out);
        *out = object(handle)->isBuilt;
#else
        throw Unsupported{};
#endif
    });
}

OpalResult OPAL_C_CALL opal_instance_acceleration_structure_native_handle(
    OpalInstanceAccelerationStructure *handle, uint64_t *out) {
    return guard(__func__, [&] {
#if defined(VULKAN)
        require(out);
        *out = object(handle)->nativeHandle();
#else
        throw Unsupported{};
#endif
    });
}

OpalResult OPAL_C_CALL
opal_command_buffer_build_instance_acceleration_structure(
    OpalCommandBuffer *handle, OpalInstanceAccelerationStructure *structure) {
    return guard(__func__, [&] {
#if defined(METAL) || defined(VULKAN)
        object(handle)->buildInstanceAccelerationStructure(object(structure));
#else
        throw Unsupported{};
#endif
    });
}

OpalResult OPAL_C_CALL opal_command_buffer_bind_instance_acceleration_structure(
    OpalCommandBuffer *handle, OpalInstanceAccelerationStructure *structure,
    uint32_t binding) {
    return guard(__func__, [&] {
#if defined(METAL) || defined(VULKAN)
        object(handle)->bindInstanceAccelerationStructure(object(structure),
                                                          binding);
#else
        throw Unsupported{};
#endif
    });
}

OpalResult OPAL_C_CALL opal_instance_acceleration_structure_build(
    OpalCommandBuffer *commands,
    OpalPrimitiveAccelerationStructure *const *blases, size_t blas_count,
    const OpalAccelerationStructureInstance *values, size_t count,
    OpalInstanceAccelerationStructure **out) {
    return guard(__func__, [&] {
        require(out);
        *out = nullptr;
#if defined(METAL) || defined(VULKAN)
        require(out);
        *out = nullptr;
        require(blases || !blas_count);
        std::vector<std::shared_ptr<opal::PrimitiveAccelerationStructure>>
            objects;
        for (size_t i = 0; i < blas_count; ++i)
            objects.push_back(object(blases[i]));
        output(out,
               object(commands)->buildAccelerationStructures(
                   objects, instances(values, count)),
               ownership(commands));
#else
        throw Unsupported{};
#endif
    });
}

OpalResult OPAL_C_CALL opal_texture_get_id(OpalTexture *handle, uint32_t *out) {
    return guard(__func__, [&] {
        require(out);
        *out = object(handle)->textureID;
    });
}

OpalResult OPAL_C_CALL opal_buffer_get_id(OpalBuffer *handle, uint32_t *out) {
    return guard(__func__, [&] {
        require(out);
        *out = object(handle)->bufferID;
    });
}

OpalResult OPAL_C_CALL opal_shader_get_id(OpalShader *handle, uint32_t *out) {
    return guard(__func__, [&] {
        require(out);
        *out = object(handle)->shaderID;
    });
}

OpalResult OPAL_C_CALL opal_shader_program_get_id(OpalShaderProgram *handle,
                                                  uint32_t *out) {
    return guard(__func__, [&] {
        require(out);
        *out = object(handle)->programID;
    });
}

OpalResult OPAL_C_CALL opal_framebuffer_get_id(OpalFramebuffer *handle,
                                               uint32_t *out) {
    return guard(__func__, [&] {
        require(out);
        *out = object(handle)->framebufferID;
    });
}

OpalResult OPAL_C_CALL opal_depth_stencil_buffer_get_id(
    OpalDepthStencilBuffer *handle, uint32_t *out) {
    return guard(__func__, [&] {
        require(out);
        *out = object(handle)->renderbufferID;
    });
}

OpalResult OPAL_C_CALL opal_drawing_state_get_id(OpalDrawingState *handle,
                                                 uint32_t *out) {
    return guard(__func__, [&] {
        require(out);
        *out = object(handle)->index;
    });
}

namespace {
struct LogRegistration {
    OpalLogCallback callback;
    void *user;
};
struct ResourceRegistration {
    OpalResourceCallback callback;
    void *user;
};
struct DrawRegistration {
    OpalDrawCallback callback;
    void *user;
};
template <class T> class CallbackRegistration {
  public:
    std::shared_ptr<T> load() {
        std::lock_guard lock(mutex);
        return value;
    }
    void store(std::shared_ptr<T> registration) {
        std::lock_guard lock(mutex);
        value = std::move(registration);
    }

  private:
    std::mutex mutex;
    std::shared_ptr<T> value;
};
CallbackRegistration<LogRegistration> log_registration;
CallbackRegistration<ResourceRegistration> resource_registration;
CallbackRegistration<DrawRegistration> draw_registration;
void log_bridge(opal::LogLevel level, const char *message) {
    auto r = log_registration.load();
    if (r && r->callback)
        r->callback(static_cast<int32_t>(level), message, r->user);
}
void resource_bridge(const opal::ResourceEvent &event) {
    auto r = resource_registration.load();
    if (!r || !r->callback)
        return;
    OpalResourceEvent value{
        event.callerObject.c_str(), static_cast<int32_t>(event.type),
        static_cast<int32_t>(event.operation), event.frameNumber, event.sizeMb};
    r->callback(&value, r->user);
}
void draw_bridge(const opal::DrawEvent &event) {
    auto r = draw_registration.load();
    if (!r || !r->callback)
        return;
    OpalDrawEvent value{event.callerObject.c_str(),
                        static_cast<int32_t>(event.type), event.frameNumber};
    r->callback(&value, r->user);
}
}

OpalResult OPAL_C_CALL opal_set_log_callback(OpalLogCallback callback,
                                             void *user_data) {
    return guard(__func__, [&] {
        log_registration.store(std::make_shared<LogRegistration>(
            LogRegistration{callback, user_data}));
        opal::setLogCallback(callback ? log_bridge : nullptr);
    });
}

OpalResult OPAL_C_CALL opal_set_resource_callback(OpalResourceCallback callback,
                                                  void *user_data) {
    return guard(__func__, [&] {
        resource_registration.store(std::make_shared<ResourceRegistration>(
            ResourceRegistration{callback, user_data}));
        opal::setResourceCallback(callback ? resource_bridge : nullptr);
    });
}

OpalResult OPAL_C_CALL opal_set_draw_callback(OpalDrawCallback callback,
                                              void *user_data) {
    return guard(__func__, [&] {
        draw_registration.store(std::make_shared<DrawRegistration>(
            DrawRegistration{callback, user_data}));
        opal::setDrawCallback(callback ? draw_bridge : nullptr);
    });
}

static_assert(static_cast<int32_t>(opal::LogLevel::Info) ==
              OPAL_LOG_LEVEL_INFO);

static_assert(static_cast<int32_t>(opal::LogLevel::Warning) ==
              OPAL_LOG_LEVEL_WARNING);

static_assert(static_cast<int32_t>(opal::LogLevel::Error) ==
              OPAL_LOG_LEVEL_ERROR);

static_assert(static_cast<int32_t>(opal::ResourceType::Texture) ==
              OPAL_RESOURCE_TYPE_TEXTURE);

static_assert(static_cast<int32_t>(opal::ResourceType::Buffer) ==
              OPAL_RESOURCE_TYPE_BUFFER);

static_assert(static_cast<int32_t>(opal::ResourceType::Shader) ==
              OPAL_RESOURCE_TYPE_SHADER);

static_assert(static_cast<int32_t>(opal::ResourceType::Mesh) ==
              OPAL_RESOURCE_TYPE_MESH);

static_assert(static_cast<int32_t>(opal::ResourceOperation::Created) ==
              OPAL_RESOURCE_OPERATION_CREATED);

static_assert(static_cast<int32_t>(opal::ResourceOperation::Loaded) ==
              OPAL_RESOURCE_OPERATION_LOADED);

static_assert(static_cast<int32_t>(opal::ResourceOperation::Unloaded) ==
              OPAL_RESOURCE_OPERATION_UNLOADED);

static_assert(static_cast<int32_t>(opal::DrawType::Draw) ==
              OPAL_DRAW_TYPE_DRAW);

static_assert(static_cast<int32_t>(opal::DrawType::Indexed) ==
              OPAL_DRAW_TYPE_INDEXED);

static_assert(static_cast<int32_t>(opal::DrawType::Patch) ==
              OPAL_DRAW_TYPE_PATCH);

static_assert(static_cast<int32_t>(opal::OpenGLProfile::Core) ==
              OPAL_OPEN_GL_PROFILE_CORE);

static_assert(static_cast<int32_t>(opal::OpenGLProfile::Compatibility) ==
              OPAL_OPEN_GL_PROFILE_COMPATIBILITY);

static_assert(static_cast<int32_t>(opal::NativeWindowType::None) ==
              OPAL_NATIVE_WINDOW_TYPE_NONE);

static_assert(static_cast<int32_t>(opal::NativeWindowType::Win32) ==
              OPAL_NATIVE_WINDOW_TYPE_WIN32);

static_assert(static_cast<int32_t>(opal::NativeWindowType::X11) ==
              OPAL_NATIVE_WINDOW_TYPE_X11);

static_assert(static_cast<int32_t>(opal::NativeWindowType::Wayland) ==
              OPAL_NATIVE_WINDOW_TYPE_WAYLAND);

static_assert(static_cast<int32_t>(opal::TextureType::Texture2D) ==
              OPAL_TEXTURE_TYPE_TEXTURE_2D);

static_assert(static_cast<int32_t>(opal::TextureType::TextureCubeMap) ==
              OPAL_TEXTURE_TYPE_TEXTURE_CUBE_MAP);

static_assert(static_cast<int32_t>(opal::TextureType::Texture3D) ==
              OPAL_TEXTURE_TYPE_TEXTURE_3D);

static_assert(static_cast<int32_t>(opal::TextureType::Texture2DArray) ==
              OPAL_TEXTURE_TYPE_TEXTURE_2D_ARRAY);

static_assert(static_cast<int32_t>(opal::TextureType::Texture2DMultisample) ==
              OPAL_TEXTURE_TYPE_TEXTURE_2D_MULTISAMPLE);

static_assert(static_cast<int32_t>(opal::TextureFormat::Rgba8) ==
              OPAL_TEXTURE_FORMAT_RGBA8);

static_assert(static_cast<int32_t>(opal::TextureFormat::sRgba8) ==
              OPAL_TEXTURE_FORMAT_S_RGBA8);

static_assert(static_cast<int32_t>(opal::TextureFormat::Rgb8) ==
              OPAL_TEXTURE_FORMAT_RGB8);

static_assert(static_cast<int32_t>(opal::TextureFormat::sRgb8) ==
              OPAL_TEXTURE_FORMAT_S_RGB8);

static_assert(static_cast<int32_t>(opal::TextureFormat::Rgba16F) ==
              OPAL_TEXTURE_FORMAT_RGBA16F);

static_assert(static_cast<int32_t>(opal::TextureFormat::Rgb16F) ==
              OPAL_TEXTURE_FORMAT_RGB16F);

static_assert(static_cast<int32_t>(opal::TextureFormat::Depth24Stencil8) ==
              OPAL_TEXTURE_FORMAT_DEPTH24_STENCIL8);

static_assert(static_cast<int32_t>(opal::TextureFormat::DepthComponent24) ==
              OPAL_TEXTURE_FORMAT_DEPTH_COMPONENT24);

static_assert(static_cast<int32_t>(opal::TextureFormat::Depth32F) ==
              OPAL_TEXTURE_FORMAT_DEPTH32F);

static_assert(static_cast<int32_t>(opal::TextureFormat::Red8) ==
              OPAL_TEXTURE_FORMAT_RED8);

static_assert(static_cast<int32_t>(opal::TextureFormat::Red16F) ==
              OPAL_TEXTURE_FORMAT_RED16F);

static_assert(static_cast<int32_t>(opal::TextureWrapMode::Repeat) ==
              OPAL_TEXTURE_WRAP_MODE_REPEAT);

static_assert(static_cast<int32_t>(opal::TextureWrapMode::MirroredRepeat) ==
              OPAL_TEXTURE_WRAP_MODE_MIRRORED_REPEAT);

static_assert(static_cast<int32_t>(opal::TextureWrapMode::ClampToEdge) ==
              OPAL_TEXTURE_WRAP_MODE_CLAMP_TO_EDGE);

static_assert(static_cast<int32_t>(opal::TextureWrapMode::ClampToBorder) ==
              OPAL_TEXTURE_WRAP_MODE_CLAMP_TO_BORDER);

static_assert(static_cast<int32_t>(opal::TextureFilterMode::Nearest) ==
              OPAL_TEXTURE_FILTER_MODE_NEAREST);

static_assert(static_cast<int32_t>(opal::TextureFilterMode::Linear) ==
              OPAL_TEXTURE_FILTER_MODE_LINEAR);

static_assert(
    static_cast<int32_t>(opal::TextureFilterMode::NearestMipmapNearest) ==
    OPAL_TEXTURE_FILTER_MODE_NEAREST_MIPMAP_NEAREST);

static_assert(
    static_cast<int32_t>(opal::TextureFilterMode::LinearMipmapLinear) ==
    OPAL_TEXTURE_FILTER_MODE_LINEAR_MIPMAP_LINEAR);

static_assert(static_cast<int32_t>(opal::TextureAxis::S) ==
              OPAL_TEXTURE_AXIS_S);

static_assert(static_cast<int32_t>(opal::TextureAxis::T) ==
              OPAL_TEXTURE_AXIS_T);

static_assert(static_cast<int32_t>(opal::TextureAxis::R) ==
              OPAL_TEXTURE_AXIS_R);

static_assert(static_cast<int32_t>(opal::TextureDataFormat::Rgba) ==
              OPAL_TEXTURE_DATA_FORMAT_RGBA);

static_assert(static_cast<int32_t>(opal::TextureDataFormat::Rgb) ==
              OPAL_TEXTURE_DATA_FORMAT_RGB);

static_assert(static_cast<int32_t>(opal::TextureDataFormat::Red) ==
              OPAL_TEXTURE_DATA_FORMAT_RED);

static_assert(static_cast<int32_t>(opal::TextureDataFormat::Bgr) ==
              OPAL_TEXTURE_DATA_FORMAT_BGR);

static_assert(static_cast<int32_t>(opal::TextureDataFormat::Bgra) ==
              OPAL_TEXTURE_DATA_FORMAT_BGRA);

static_assert(static_cast<int32_t>(opal::TextureDataFormat::DepthComponent) ==
              OPAL_TEXTURE_DATA_FORMAT_DEPTH_COMPONENT);

static_assert(static_cast<int32_t>(opal::ShaderType::Vertex) ==
              OPAL_SHADER_TYPE_VERTEX);

static_assert(static_cast<int32_t>(opal::ShaderType::Fragment) ==
              OPAL_SHADER_TYPE_FRAGMENT);

static_assert(static_cast<int32_t>(opal::ShaderType::Geometry) ==
              OPAL_SHADER_TYPE_GEOMETRY);

static_assert(static_cast<int32_t>(opal::ShaderType::TessellationControl) ==
              OPAL_SHADER_TYPE_TESSELLATION_CONTROL);

static_assert(static_cast<int32_t>(opal::ShaderType::TessellationEvaluation) ==
              OPAL_SHADER_TYPE_TESSELLATION_EVALUATION);

static_assert(static_cast<int32_t>(opal::ShaderType::Compute) ==
              OPAL_SHADER_TYPE_COMPUTE);

static_assert(static_cast<int32_t>(opal::ShaderType::RayGeneration) ==
              OPAL_SHADER_TYPE_RAY_GENERATION);

static_assert(static_cast<int32_t>(opal::ShaderType::Miss) ==
              OPAL_SHADER_TYPE_MISS);

static_assert(static_cast<int32_t>(opal::ShaderType::ClosestHit) ==
              OPAL_SHADER_TYPE_CLOSEST_HIT);

static_assert(static_cast<int32_t>(opal::ShaderType::AnyHit) ==
              OPAL_SHADER_TYPE_ANY_HIT);

static_assert(static_cast<int32_t>(opal::ShaderType::Intersection) ==
              OPAL_SHADER_TYPE_INTERSECTION);

static_assert(static_cast<int32_t>(opal::ShaderType::Callable) ==
              OPAL_SHADER_TYPE_CALLABLE);

static_assert(static_cast<int32_t>(opal::VertexAttributeType::Float) ==
              OPAL_VERTEX_ATTRIBUTE_TYPE_FLOAT);

static_assert(static_cast<int32_t>(opal::VertexAttributeType::Double) ==
              OPAL_VERTEX_ATTRIBUTE_TYPE_DOUBLE);

static_assert(static_cast<int32_t>(opal::VertexAttributeType::Int) ==
              OPAL_VERTEX_ATTRIBUTE_TYPE_INT);

static_assert(static_cast<int32_t>(opal::VertexAttributeType::UnsignedInt) ==
              OPAL_VERTEX_ATTRIBUTE_TYPE_UNSIGNED_INT);

static_assert(static_cast<int32_t>(opal::VertexAttributeType::Short) ==
              OPAL_VERTEX_ATTRIBUTE_TYPE_SHORT);

static_assert(static_cast<int32_t>(opal::VertexAttributeType::UnsignedShort) ==
              OPAL_VERTEX_ATTRIBUTE_TYPE_UNSIGNED_SHORT);

static_assert(static_cast<int32_t>(opal::VertexAttributeType::Byte) ==
              OPAL_VERTEX_ATTRIBUTE_TYPE_BYTE);

static_assert(static_cast<int32_t>(opal::VertexAttributeType::UnsignedByte) ==
              OPAL_VERTEX_ATTRIBUTE_TYPE_UNSIGNED_BYTE);

static_assert(static_cast<int32_t>(opal::PrimitiveStyle::Points) ==
              OPAL_PRIMITIVE_STYLE_POINTS);

static_assert(static_cast<int32_t>(opal::PrimitiveStyle::Lines) ==
              OPAL_PRIMITIVE_STYLE_LINES);

static_assert(static_cast<int32_t>(opal::PrimitiveStyle::LineStrip) ==
              OPAL_PRIMITIVE_STYLE_LINE_STRIP);

static_assert(static_cast<int32_t>(opal::PrimitiveStyle::Triangles) ==
              OPAL_PRIMITIVE_STYLE_TRIANGLES);

static_assert(static_cast<int32_t>(opal::PrimitiveStyle::TriangleStrip) ==
              OPAL_PRIMITIVE_STYLE_TRIANGLE_STRIP);

static_assert(static_cast<int32_t>(opal::PrimitiveStyle::TriangleFan) ==
              OPAL_PRIMITIVE_STYLE_TRIANGLE_FAN);

static_assert(static_cast<int32_t>(opal::PrimitiveStyle::Patches) ==
              OPAL_PRIMITIVE_STYLE_PATCHES);

static_assert(static_cast<int32_t>(opal::RasterizerMode::Fill) ==
              OPAL_RASTERIZER_MODE_FILL);

static_assert(static_cast<int32_t>(opal::RasterizerMode::Line) ==
              OPAL_RASTERIZER_MODE_LINE);

static_assert(static_cast<int32_t>(opal::RasterizerMode::Point) ==
              OPAL_RASTERIZER_MODE_POINT);

static_assert(static_cast<int32_t>(opal::VertexBindingInputRate::Vertex) ==
              OPAL_VERTEX_BINDING_INPUT_RATE_VERTEX);

static_assert(static_cast<int32_t>(opal::VertexBindingInputRate::Instance) ==
              OPAL_VERTEX_BINDING_INPUT_RATE_INSTANCE);

static_assert(static_cast<int32_t>(opal::CullMode::None) ==
              OPAL_CULL_MODE_NONE);

static_assert(static_cast<int32_t>(opal::CullMode::Front) ==
              OPAL_CULL_MODE_FRONT);

static_assert(static_cast<int32_t>(opal::CullMode::Back) ==
              OPAL_CULL_MODE_BACK);

static_assert(static_cast<int32_t>(opal::CullMode::FrontAndBack) ==
              OPAL_CULL_MODE_FRONT_AND_BACK);

static_assert(static_cast<int32_t>(opal::FrontFace::Clockwise) ==
              OPAL_FRONT_FACE_CLOCKWISE);

static_assert(static_cast<int32_t>(opal::FrontFace::CounterClockwise) ==
              OPAL_FRONT_FACE_COUNTER_CLOCKWISE);

static_assert(static_cast<int32_t>(opal::CompareOp::Never) ==
              OPAL_COMPARE_OP_NEVER);

static_assert(static_cast<int32_t>(opal::CompareOp::Less) ==
              OPAL_COMPARE_OP_LESS);

static_assert(static_cast<int32_t>(opal::CompareOp::Equal) ==
              OPAL_COMPARE_OP_EQUAL);

static_assert(static_cast<int32_t>(opal::CompareOp::LessEqual) ==
              OPAL_COMPARE_OP_LESS_EQUAL);

static_assert(static_cast<int32_t>(opal::CompareOp::Greater) ==
              OPAL_COMPARE_OP_GREATER);

static_assert(static_cast<int32_t>(opal::CompareOp::NotEqual) ==
              OPAL_COMPARE_OP_NOT_EQUAL);

static_assert(static_cast<int32_t>(opal::CompareOp::GreaterEqual) ==
              OPAL_COMPARE_OP_GREATER_EQUAL);

static_assert(static_cast<int32_t>(opal::CompareOp::Always) ==
              OPAL_COMPARE_OP_ALWAYS);

static_assert(static_cast<int32_t>(opal::BlendFunc::Zero) ==
              OPAL_BLEND_FUNC_ZERO);

static_assert(static_cast<int32_t>(opal::BlendFunc::One) ==
              OPAL_BLEND_FUNC_ONE);

static_assert(static_cast<int32_t>(opal::BlendFunc::SrcColor) ==
              OPAL_BLEND_FUNC_SRC_COLOR);

static_assert(static_cast<int32_t>(opal::BlendFunc::OneMinusSrcColor) ==
              OPAL_BLEND_FUNC_ONE_MINUS_SRC_COLOR);

static_assert(static_cast<int32_t>(opal::BlendFunc::DstColor) ==
              OPAL_BLEND_FUNC_DST_COLOR);

static_assert(static_cast<int32_t>(opal::BlendFunc::OneMinusDstColor) ==
              OPAL_BLEND_FUNC_ONE_MINUS_DST_COLOR);

static_assert(static_cast<int32_t>(opal::BlendFunc::SrcAlpha) ==
              OPAL_BLEND_FUNC_SRC_ALPHA);

static_assert(static_cast<int32_t>(opal::BlendFunc::OneMinusSrcAlpha) ==
              OPAL_BLEND_FUNC_ONE_MINUS_SRC_ALPHA);

static_assert(static_cast<int32_t>(opal::BlendFunc::DstAlpha) ==
              OPAL_BLEND_FUNC_DST_ALPHA);

static_assert(static_cast<int32_t>(opal::BlendFunc::OneMinusDstAlpha) ==
              OPAL_BLEND_FUNC_ONE_MINUS_DST_ALPHA);

static_assert(static_cast<int32_t>(opal::BlendEquation::Add) ==
              OPAL_BLEND_EQUATION_ADD);

static_assert(static_cast<int32_t>(opal::BlendEquation::Subtract) ==
              OPAL_BLEND_EQUATION_SUBTRACT);

static_assert(static_cast<int32_t>(opal::BlendEquation::ReverseSubtract) ==
              OPAL_BLEND_EQUATION_REVERSE_SUBTRACT);

static_assert(static_cast<int32_t>(opal::BlendEquation::Min) ==
              OPAL_BLEND_EQUATION_MIN);

static_assert(static_cast<int32_t>(opal::BlendEquation::Max) ==
              OPAL_BLEND_EQUATION_MAX);

static_assert(static_cast<int32_t>(opal::LogicOp::Clear) ==
              OPAL_LOGIC_OP_CLEAR);

static_assert(static_cast<int32_t>(opal::LogicOp::And) == OPAL_LOGIC_OP_AND);

static_assert(static_cast<int32_t>(opal::LogicOp::AndReverse) ==
              OPAL_LOGIC_OP_AND_REVERSE);

static_assert(static_cast<int32_t>(opal::LogicOp::Copy) == OPAL_LOGIC_OP_COPY);

static_assert(static_cast<int32_t>(opal::LogicOp::AndInverted) ==
              OPAL_LOGIC_OP_AND_INVERTED);

static_assert(static_cast<int32_t>(opal::LogicOp::NoOp) == OPAL_LOGIC_OP_NO_OP);

static_assert(static_cast<int32_t>(opal::LogicOp::Xor) == OPAL_LOGIC_OP_XOR);

static_assert(static_cast<int32_t>(opal::LogicOp::Or) == OPAL_LOGIC_OP_OR);

static_assert(static_cast<int32_t>(opal::LogicOp::Nor) == OPAL_LOGIC_OP_NOR);

static_assert(static_cast<int32_t>(opal::LogicOp::Equivalent) ==
              OPAL_LOGIC_OP_EQUIVALENT);

static_assert(static_cast<int32_t>(opal::LogicOp::Invert) ==
              OPAL_LOGIC_OP_INVERT);

static_assert(static_cast<int32_t>(opal::LogicOp::OrReverse) ==
              OPAL_LOGIC_OP_OR_REVERSE);

static_assert(static_cast<int32_t>(opal::LogicOp::CopyInverted) ==
              OPAL_LOGIC_OP_COPY_INVERTED);

static_assert(static_cast<int32_t>(opal::LogicOp::OrInverted) ==
              OPAL_LOGIC_OP_OR_INVERTED);

static_assert(static_cast<int32_t>(opal::LogicOp::Nand) == OPAL_LOGIC_OP_NAND);

static_assert(static_cast<int32_t>(opal::LogicOp::Set) == OPAL_LOGIC_OP_SET);

static_assert(static_cast<int32_t>(opal::BufferUsage::VertexBuffer) ==
              OPAL_BUFFER_USAGE_VERTEX_BUFFER);

static_assert(static_cast<int32_t>(opal::BufferUsage::IndexArray) ==
              OPAL_BUFFER_USAGE_INDEX_ARRAY);

static_assert(static_cast<int32_t>(opal::BufferUsage::GeneralPurpose) ==
              OPAL_BUFFER_USAGE_GENERAL_PURPOSE);

static_assert(static_cast<int32_t>(opal::BufferUsage::UniformBuffer) ==
              OPAL_BUFFER_USAGE_UNIFORM_BUFFER);

static_assert(static_cast<int32_t>(opal::BufferUsage::ShaderRead) ==
              OPAL_BUFFER_USAGE_SHADER_READ);

static_assert(static_cast<int32_t>(opal::BufferUsage::ShaderReadWrite) ==
              OPAL_BUFFER_USAGE_SHADER_READ_WRITE);

static_assert(static_cast<int32_t>(opal::BufferUsage::RayTracing) ==
              OPAL_BUFFER_USAGE_RAY_TRACING);

static_assert(static_cast<int32_t>(opal::MemoryUsageType::GPUOnly) ==
              OPAL_MEMORY_USAGE_TYPE_GPU_ONLY);

static_assert(static_cast<int32_t>(opal::MemoryUsageType::CPUToGPU) ==
              OPAL_MEMORY_USAGE_TYPE_CPU_TO_GPU);

static_assert(static_cast<int32_t>(opal::MemoryUsageType::GPUToCPU) ==
              OPAL_MEMORY_USAGE_TYPE_GPU_TO_CPU);

static_assert(static_cast<int32_t>(opal::Attachment::Type::Color) ==
              OPAL_ATTACHMENT_TYPE_COLOR);

static_assert(static_cast<int32_t>(opal::Attachment::Type::Depth) ==
              OPAL_ATTACHMENT_TYPE_DEPTH);

static_assert(static_cast<int32_t>(opal::Attachment::Type::Stencil) ==
              OPAL_ATTACHMENT_TYPE_STENCIL);

static_assert(static_cast<int32_t>(opal::Attachment::Type::DepthStencil) ==
              OPAL_ATTACHMENT_TYPE_DEPTH_STENCIL);
