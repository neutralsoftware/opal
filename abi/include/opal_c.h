#ifndef OPAL_C_H

#define OPAL_C_H

#include <stddef.h>

#include <stdint.h>

#if defined(_WIN32)
#if defined(OPAL_C_BUILD)
#define OPAL_C_API __declspec(dllexport)
#else
#define OPAL_C_API __declspec(dllimport)
#endif
#define OPAL_C_CALL __cdecl
#else
#define OPAL_C_API __attribute__((visibility("default")))
#define OPAL_C_CALL
#endif
#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpalContext OpalContext;

typedef struct OpalDevice OpalDevice;

typedef struct OpalCommandBuffer OpalCommandBuffer;

typedef struct OpalTexture OpalTexture;

typedef struct OpalShader OpalShader;

typedef struct OpalShaderProgram OpalShaderProgram;

typedef struct OpalPipeline OpalPipeline;

typedef struct OpalBuffer OpalBuffer;

typedef struct OpalDrawingState OpalDrawingState;

typedef struct OpalDepthStencilBuffer OpalDepthStencilBuffer;

typedef struct OpalFramebuffer OpalFramebuffer;

typedef struct OpalRenderPass OpalRenderPass;

typedef struct OpalResolveAction OpalResolveAction;

typedef struct OpalPrimitiveAccelerationStructure
    OpalPrimitiveAccelerationStructure;

typedef struct OpalInstanceAccelerationStructure
    OpalInstanceAccelerationStructure;

typedef int32_t OpalResult;

enum {
    OPAL_SUCCESS = 0,
    OPAL_ERROR_INVALID_ARGUMENT = 1,
    OPAL_ERROR_INITIALIZATION_FAILED = 2,
    OPAL_ERROR_OUT_OF_MEMORY = 3,
    OPAL_ERROR_UNKNOWN = 4,
    OPAL_ERROR_UNSUPPORTED = 5
};

typedef int32_t OpalLogLevel;

enum {
    OPAL_LOG_LEVEL_INFO = 0,
    OPAL_LOG_LEVEL_WARNING = 1,
    OPAL_LOG_LEVEL_ERROR = 2
};

typedef int32_t OpalResourceType;

enum {
    OPAL_RESOURCE_TYPE_TEXTURE = 0,
    OPAL_RESOURCE_TYPE_BUFFER = 1,
    OPAL_RESOURCE_TYPE_SHADER = 2,
    OPAL_RESOURCE_TYPE_MESH = 3
};

typedef int32_t OpalResourceOperation;

enum {
    OPAL_RESOURCE_OPERATION_CREATED = 0,
    OPAL_RESOURCE_OPERATION_LOADED = 1,
    OPAL_RESOURCE_OPERATION_UNLOADED = 2
};

typedef int32_t OpalDrawType;

enum {
    OPAL_DRAW_TYPE_DRAW = 0,
    OPAL_DRAW_TYPE_INDEXED = 1,
    OPAL_DRAW_TYPE_PATCH = 2
};

typedef int32_t OpalOpenGLProfile;

enum { OPAL_OPEN_GL_PROFILE_CORE = 0, OPAL_OPEN_GL_PROFILE_COMPATIBILITY = 1 };

typedef int32_t OpalNativeWindowType;

enum {
    OPAL_NATIVE_WINDOW_TYPE_NONE = 0,
    OPAL_NATIVE_WINDOW_TYPE_WIN32 = 1,
    OPAL_NATIVE_WINDOW_TYPE_X11 = 2,
    OPAL_NATIVE_WINDOW_TYPE_WAYLAND = 3
};

typedef int32_t OpalTextureType;

enum {
    OPAL_TEXTURE_TYPE_TEXTURE_2D = 0,
    OPAL_TEXTURE_TYPE_TEXTURE_CUBE_MAP = 1,
    OPAL_TEXTURE_TYPE_TEXTURE_3D = 2,
    OPAL_TEXTURE_TYPE_TEXTURE_2D_ARRAY = 3,
    OPAL_TEXTURE_TYPE_TEXTURE_2D_MULTISAMPLE = 4
};

typedef int32_t OpalTextureFormat;

enum {
    OPAL_TEXTURE_FORMAT_RGBA8 = 0,
    OPAL_TEXTURE_FORMAT_S_RGBA8 = 1,
    OPAL_TEXTURE_FORMAT_RGB8 = 2,
    OPAL_TEXTURE_FORMAT_S_RGB8 = 3,
    OPAL_TEXTURE_FORMAT_RGBA16F = 4,
    OPAL_TEXTURE_FORMAT_RGB16F = 5,
    OPAL_TEXTURE_FORMAT_DEPTH24_STENCIL8 = 6,
    OPAL_TEXTURE_FORMAT_DEPTH_COMPONENT24 = 7,
    OPAL_TEXTURE_FORMAT_DEPTH32F = 8,
    OPAL_TEXTURE_FORMAT_RED8 = 9,
    OPAL_TEXTURE_FORMAT_RED16F = 10
};

typedef int32_t OpalTextureWrapMode;

enum {
    OPAL_TEXTURE_WRAP_MODE_REPEAT = 0,
    OPAL_TEXTURE_WRAP_MODE_MIRRORED_REPEAT = 1,
    OPAL_TEXTURE_WRAP_MODE_CLAMP_TO_EDGE = 2,
    OPAL_TEXTURE_WRAP_MODE_CLAMP_TO_BORDER = 3
};

typedef int32_t OpalTextureFilterMode;

enum {
    OPAL_TEXTURE_FILTER_MODE_NEAREST = 0,
    OPAL_TEXTURE_FILTER_MODE_LINEAR = 1,
    OPAL_TEXTURE_FILTER_MODE_NEAREST_MIPMAP_NEAREST = 2,
    OPAL_TEXTURE_FILTER_MODE_LINEAR_MIPMAP_LINEAR = 3
};

typedef int32_t OpalTextureAxis;

enum {
    OPAL_TEXTURE_AXIS_S = 0,
    OPAL_TEXTURE_AXIS_T = 1,
    OPAL_TEXTURE_AXIS_R = 2
};

typedef int32_t OpalTextureDataFormat;

enum {
    OPAL_TEXTURE_DATA_FORMAT_RGBA = 0,
    OPAL_TEXTURE_DATA_FORMAT_RGB = 1,
    OPAL_TEXTURE_DATA_FORMAT_RED = 2,
    OPAL_TEXTURE_DATA_FORMAT_BGR = 3,
    OPAL_TEXTURE_DATA_FORMAT_BGRA = 4,
    OPAL_TEXTURE_DATA_FORMAT_DEPTH_COMPONENT = 5
};

typedef int32_t OpalShaderType;

enum {
    OPAL_SHADER_TYPE_VERTEX = 0,
    OPAL_SHADER_TYPE_FRAGMENT = 1,
    OPAL_SHADER_TYPE_GEOMETRY = 2,
    OPAL_SHADER_TYPE_TESSELLATION_CONTROL = 3,
    OPAL_SHADER_TYPE_TESSELLATION_EVALUATION = 4,
    OPAL_SHADER_TYPE_COMPUTE = 5,
    OPAL_SHADER_TYPE_RAY_GENERATION = 6,
    OPAL_SHADER_TYPE_MISS = 7,
    OPAL_SHADER_TYPE_CLOSEST_HIT = 8,
    OPAL_SHADER_TYPE_ANY_HIT = 9,
    OPAL_SHADER_TYPE_INTERSECTION = 10,
    OPAL_SHADER_TYPE_CALLABLE = 11
};

typedef int32_t OpalVertexAttributeType;

enum {
    OPAL_VERTEX_ATTRIBUTE_TYPE_FLOAT = 0,
    OPAL_VERTEX_ATTRIBUTE_TYPE_DOUBLE = 1,
    OPAL_VERTEX_ATTRIBUTE_TYPE_INT = 2,
    OPAL_VERTEX_ATTRIBUTE_TYPE_UNSIGNED_INT = 3,
    OPAL_VERTEX_ATTRIBUTE_TYPE_SHORT = 4,
    OPAL_VERTEX_ATTRIBUTE_TYPE_UNSIGNED_SHORT = 5,
    OPAL_VERTEX_ATTRIBUTE_TYPE_BYTE = 6,
    OPAL_VERTEX_ATTRIBUTE_TYPE_UNSIGNED_BYTE = 7
};

typedef int32_t OpalPrimitiveStyle;

enum {
    OPAL_PRIMITIVE_STYLE_POINTS = 0,
    OPAL_PRIMITIVE_STYLE_LINES = 1,
    OPAL_PRIMITIVE_STYLE_LINE_STRIP = 2,
    OPAL_PRIMITIVE_STYLE_TRIANGLES = 3,
    OPAL_PRIMITIVE_STYLE_TRIANGLE_STRIP = 4,
    OPAL_PRIMITIVE_STYLE_TRIANGLE_FAN = 5,
    OPAL_PRIMITIVE_STYLE_PATCHES = 6
};

typedef int32_t OpalRasterizerMode;

enum {
    OPAL_RASTERIZER_MODE_FILL = 0,
    OPAL_RASTERIZER_MODE_LINE = 1,
    OPAL_RASTERIZER_MODE_POINT = 2
};

typedef int32_t OpalVertexBindingInputRate;

enum {
    OPAL_VERTEX_BINDING_INPUT_RATE_VERTEX = 0,
    OPAL_VERTEX_BINDING_INPUT_RATE_INSTANCE = 1
};

typedef int32_t OpalCullMode;

enum {
    OPAL_CULL_MODE_NONE = 0,
    OPAL_CULL_MODE_FRONT = 1,
    OPAL_CULL_MODE_BACK = 2,
    OPAL_CULL_MODE_FRONT_AND_BACK = 3
};

typedef int32_t OpalFrontFace;

enum { OPAL_FRONT_FACE_CLOCKWISE = 0, OPAL_FRONT_FACE_COUNTER_CLOCKWISE = 1 };

typedef int32_t OpalCompareOp;

enum {
    OPAL_COMPARE_OP_NEVER = 0,
    OPAL_COMPARE_OP_LESS = 1,
    OPAL_COMPARE_OP_EQUAL = 2,
    OPAL_COMPARE_OP_LESS_EQUAL = 3,
    OPAL_COMPARE_OP_GREATER = 4,
    OPAL_COMPARE_OP_NOT_EQUAL = 5,
    OPAL_COMPARE_OP_GREATER_EQUAL = 6,
    OPAL_COMPARE_OP_ALWAYS = 7
};

typedef int32_t OpalBlendFunc;

enum {
    OPAL_BLEND_FUNC_ZERO = 0,
    OPAL_BLEND_FUNC_ONE = 1,
    OPAL_BLEND_FUNC_SRC_COLOR = 2,
    OPAL_BLEND_FUNC_ONE_MINUS_SRC_COLOR = 3,
    OPAL_BLEND_FUNC_DST_COLOR = 4,
    OPAL_BLEND_FUNC_ONE_MINUS_DST_COLOR = 5,
    OPAL_BLEND_FUNC_SRC_ALPHA = 6,
    OPAL_BLEND_FUNC_ONE_MINUS_SRC_ALPHA = 7,
    OPAL_BLEND_FUNC_DST_ALPHA = 8,
    OPAL_BLEND_FUNC_ONE_MINUS_DST_ALPHA = 9
};

typedef int32_t OpalBlendEquation;

enum {
    OPAL_BLEND_EQUATION_ADD = 0,
    OPAL_BLEND_EQUATION_SUBTRACT = 1,
    OPAL_BLEND_EQUATION_REVERSE_SUBTRACT = 2,
    OPAL_BLEND_EQUATION_MIN = 3,
    OPAL_BLEND_EQUATION_MAX = 4
};

typedef int32_t OpalLogicOp;

enum {
    OPAL_LOGIC_OP_CLEAR = 0,
    OPAL_LOGIC_OP_AND = 1,
    OPAL_LOGIC_OP_AND_REVERSE = 2,
    OPAL_LOGIC_OP_COPY = 3,
    OPAL_LOGIC_OP_AND_INVERTED = 4,
    OPAL_LOGIC_OP_NO_OP = 5,
    OPAL_LOGIC_OP_XOR = 6,
    OPAL_LOGIC_OP_OR = 7,
    OPAL_LOGIC_OP_NOR = 8,
    OPAL_LOGIC_OP_EQUIVALENT = 9,
    OPAL_LOGIC_OP_INVERT = 10,
    OPAL_LOGIC_OP_OR_REVERSE = 11,
    OPAL_LOGIC_OP_COPY_INVERTED = 12,
    OPAL_LOGIC_OP_OR_INVERTED = 13,
    OPAL_LOGIC_OP_NAND = 14,
    OPAL_LOGIC_OP_SET = 15
};

typedef int32_t OpalBufferUsage;

enum {
    OPAL_BUFFER_USAGE_VERTEX_BUFFER = 0,
    OPAL_BUFFER_USAGE_INDEX_ARRAY = 1,
    OPAL_BUFFER_USAGE_GENERAL_PURPOSE = 2,
    OPAL_BUFFER_USAGE_UNIFORM_BUFFER = 3,
    OPAL_BUFFER_USAGE_SHADER_READ = 4,
    OPAL_BUFFER_USAGE_SHADER_READ_WRITE = 5,
    OPAL_BUFFER_USAGE_RAY_TRACING = 6
};

typedef int32_t OpalMemoryUsageType;

enum {
    OPAL_MEMORY_USAGE_TYPE_GPU_ONLY = 0,
    OPAL_MEMORY_USAGE_TYPE_CPU_TO_GPU = 1,
    OPAL_MEMORY_USAGE_TYPE_GPU_TO_CPU = 2
};

typedef int32_t OpalAttachmentType;

enum {
    OPAL_ATTACHMENT_TYPE_COLOR = 0,
    OPAL_ATTACHMENT_TYPE_DEPTH = 1,
    OPAL_ATTACHMENT_TYPE_STENCIL = 2,
    OPAL_ATTACHMENT_TYPE_DEPTH_STENCIL = 3
};

typedef struct OpalContextConfiguration {
    int32_t use_opengl;
    int32_t major_version;
    int32_t minor_version;
    OpalOpenGLProfile profile;
    const char *application_name;
    const char *application_version;
    int32_t create_validation_layers;
} OpalContextConfiguration;
typedef struct OpalVertexAttribute {
    const char *name;
    OpalVertexAttributeType type;
    uint32_t offset, location;
    int32_t normalized;
    uint32_t size, stride;
    OpalVertexBindingInputRate input_rate;
    uint32_t divisor;
} OpalVertexAttribute;
typedef struct OpalVertexBinding {
    uint32_t stride;
    OpalVertexBindingInputRate input_rate;
} OpalVertexBinding;
typedef struct OpalVertexAttributeBinding {
    OpalVertexAttribute attribute;
    OpalBuffer *source_buffer;
} OpalVertexAttributeBinding;
typedef struct OpalPrimitiveVertex {
    float position[3], normal[3], tangent[3], bitangent[3], uv[2];
} OpalPrimitiveVertex;
typedef struct OpalGeometry {
    const float *positions;
    size_t position_count;
    const uint32_t *indices;
    size_t index_count;
} OpalGeometry;
typedef struct OpalAccelerationStructureInstance {
    OpalPrimitiveAccelerationStructure *blas;
    float transform[16];
    uint32_t instance_id, mask;
    int32_t cull_disable;
} OpalAccelerationStructureInstance;
typedef struct OpalResourceEvent {
    const char *caller_object;
    OpalResourceType type;
    OpalResourceOperation operation;
    uint32_t frame_number;
    float size_mb;
} OpalResourceEvent;
typedef struct OpalDrawEvent {
    const char *caller_object;
    OpalDrawType type;
    uint32_t frame_number;
} OpalDrawEvent;
typedef void(OPAL_C_CALL *OpalLogCallback)(OpalLogLevel level,
                                           const char *message,
                                           void *user_data);
typedef void(OPAL_C_CALL *OpalResourceCallback)(const OpalResourceEvent *event,
                                                void *user_data);
typedef void(OPAL_C_CALL *OpalDrawCallback)(const OpalDrawEvent *event,
                                            void *user_data);
typedef int32_t OpalDeviceInfoField;
enum {
    OPAL_DEVICE_INFO_DEVICE_NAME,
    OPAL_DEVICE_INFO_VENDOR_NAME,
    OPAL_DEVICE_INFO_DRIVER_VERSION,
    OPAL_DEVICE_INFO_RENDERING_VERSION,
    OPAL_DEVICE_INFO_OPAL_VERSION
};
typedef int32_t OpalBackend;
enum { OPAL_BACKEND_METAL, OPAL_BACKEND_VULKAN, OPAL_BACKEND_OPENGL };

OPAL_C_API const char *OPAL_C_CALL opal_get_last_error(void);

OPAL_C_API uint32_t OPAL_C_CALL opal_get_abi_version(void);

OPAL_C_API OpalBackend OPAL_C_CALL opal_get_backend(void);

OPAL_C_API OpalResult OPAL_C_CALL
opal_context_configuration_default(OpalContextConfiguration *out);

OPAL_C_API OpalResult OPAL_C_CALL opal_context_create(OpalContext **out);

OPAL_C_API OpalResult OPAL_C_CALL opal_context_create_with_configuration(
    const OpalContextConfiguration *configuration, OpalContext **out);

OPAL_C_API OpalResult OPAL_C_CALL opal_context_destroy(OpalContext *handle);

OPAL_C_API OpalResult OPAL_C_CALL opal_device_destroy(OpalDevice *handle);

OPAL_C_API OpalResult OPAL_C_CALL
opal_command_buffer_destroy(OpalCommandBuffer *handle);

OPAL_C_API OpalResult OPAL_C_CALL opal_texture_destroy(OpalTexture *handle);

OPAL_C_API OpalResult OPAL_C_CALL opal_shader_destroy(OpalShader *handle);

OPAL_C_API OpalResult OPAL_C_CALL
opal_shader_program_destroy(OpalShaderProgram *handle);

OPAL_C_API OpalResult OPAL_C_CALL opal_pipeline_destroy(OpalPipeline *handle);

OPAL_C_API OpalResult OPAL_C_CALL opal_buffer_destroy(OpalBuffer *handle);

OPAL_C_API OpalResult OPAL_C_CALL
opal_drawing_state_destroy(OpalDrawingState *handle);

OPAL_C_API OpalResult OPAL_C_CALL
opal_depth_stencil_buffer_destroy(OpalDepthStencilBuffer *handle);

OPAL_C_API OpalResult OPAL_C_CALL
opal_framebuffer_destroy(OpalFramebuffer *handle);

OPAL_C_API OpalResult OPAL_C_CALL
opal_render_pass_destroy(OpalRenderPass *handle);

OPAL_C_API OpalResult OPAL_C_CALL
opal_resolve_action_destroy(OpalResolveAction *handle);

OPAL_C_API OpalResult OPAL_C_CALL opal_primitive_acceleration_structure_destroy(
    OpalPrimitiveAccelerationStructure *handle);

OPAL_C_API OpalResult OPAL_C_CALL opal_instance_acceleration_structure_destroy(
    OpalInstanceAccelerationStructure *handle);

OPAL_C_API OpalResult OPAL_C_CALL
opal_context_set_decorated(OpalContext *handle, int32_t enabled);

OPAL_C_API OpalResult OPAL_C_CALL
opal_context_set_resizable(OpalContext *handle, int32_t enabled);

OPAL_C_API OpalResult OPAL_C_CALL
opal_context_set_transparent(OpalContext *handle, int32_t enabled);

OPAL_C_API OpalResult OPAL_C_CALL
opal_context_set_always_on_top(OpalContext *handle, int32_t enabled);

OPAL_C_API OpalResult OPAL_C_CALL
opal_context_set_high_pixel_density(OpalContext *handle, int32_t enabled);

OPAL_C_API OpalResult OPAL_C_CALL opal_context_set_hidden(OpalContext *handle,
                                                          int32_t enabled);

OPAL_C_API OpalResult OPAL_C_CALL opal_context_set_samples(OpalContext *handle,
                                                           int32_t samples);

OPAL_C_API OpalResult OPAL_C_CALL opal_context_make_window(OpalContext *context,
                                                           int width,
                                                           int height,
                                                           const char *title);

OPAL_C_API OpalResult OPAL_C_CALL opal_context_make_window_on_display(
    OpalContext *handle, int32_t width, int32_t height, const char *title,
    uint32_t display_id);

OPAL_C_API OpalResult OPAL_C_CALL opal_context_make_window_from_native(
    OpalContext *handle, void *window, OpalNativeWindowType type, int32_t width,
    int32_t height, const char *title);

OPAL_C_API OpalResult OPAL_C_CALL opal_context_adopt_window(
    OpalContext *handle, void *window, int32_t take_ownership);

OPAL_C_API OpalResult OPAL_C_CALL
opal_context_set_metal_target_view(OpalContext *handle, void *view);

OPAL_C_API OpalResult OPAL_C_CALL opal_context_get_window(OpalContext *handle,
                                                          void **out);

OPAL_C_API OpalResult OPAL_C_CALL
opal_context_get_metal_target_view(OpalContext *handle, void **out);

OPAL_C_API OpalResult OPAL_C_CALL
opal_context_get_gl_context(OpalContext *handle, void **out);

OPAL_C_API OpalResult OPAL_C_CALL
opal_context_make_current(OpalContext *handle);

OPAL_C_API OpalResult OPAL_C_CALL opal_context_swap_window(OpalContext *handle);

OPAL_C_API OpalResult OPAL_C_CALL opal_device_acquire(OpalContext *context,
                                                      OpalDevice **out);

OPAL_C_API OpalResult OPAL_C_CALL
opal_command_buffer_acquire(OpalDevice *device, OpalCommandBuffer **out);

OPAL_C_API OpalResult OPAL_C_CALL
opal_framebuffer_get_default(OpalDevice *device, OpalFramebuffer **out);

OPAL_C_API OpalResult OPAL_C_CALL opal_device_submit_command_buffer(
    OpalDevice *handle, OpalCommandBuffer *commands);

OPAL_C_API OpalResult OPAL_C_CALL
opal_device_get_info(OpalDevice *handle, OpalDeviceInfoField field,
                     char *buffer, size_t capacity, size_t *required);

OPAL_C_API OpalResult OPAL_C_CALL
opal_device_supports_ray_tracing(OpalDevice *handle, int32_t *out);

OPAL_C_API OpalResult OPAL_C_CALL
opal_device_get_frame_count(OpalDevice *handle, int64_t *out);

OPAL_C_API OpalResult OPAL_C_CALL
opal_device_get_metal_device(OpalDevice *handle, void **out);

OPAL_C_API OpalResult OPAL_C_CALL opal_texture_create(
    OpalTextureType type, OpalTextureFormat format, int32_t width,
    int32_t height, OpalTextureDataFormat data_format, const void *data,
    uint32_t mip_levels, OpalTexture **out);

OPAL_C_API OpalResult OPAL_C_CALL opal_texture_create_multisampled(
    OpalTextureFormat format, int32_t width, int32_t height, int32_t samples,
    OpalTexture **out);

OPAL_C_API OpalResult OPAL_C_CALL opal_texture_create_depth_cubemap(
    OpalTextureFormat format, int32_t resolution, OpalTexture **out);

OPAL_C_API OpalResult OPAL_C_CALL opal_texture_create_3d(
    OpalTextureFormat format, int32_t width, int32_t height, int32_t depth,
    OpalTextureDataFormat data_format, const void *data, OpalTexture **out);

OPAL_C_API OpalResult OPAL_C_CALL opal_texture_get_supported_sample_count(
    const OpalTextureFormat *formats, size_t count, int32_t requested,
    int32_t *out);

OPAL_C_API OpalResult OPAL_C_CALL
opal_texture_update_data(OpalTexture *handle, const void *data, int32_t width,
                         int32_t height, OpalTextureDataFormat format);

OPAL_C_API OpalResult OPAL_C_CALL opal_texture_update_data_3d(
    OpalTexture *handle, const void *data, int32_t width, int32_t height,
    int32_t depth, OpalTextureDataFormat format);

OPAL_C_API OpalResult OPAL_C_CALL opal_texture_update_face(
    OpalTexture *handle, const void *data, int32_t face, int32_t width,
    int32_t height, OpalTextureDataFormat format);

OPAL_C_API OpalResult OPAL_C_CALL
opal_texture_change_format(OpalTexture *handle, OpalTextureFormat format);

OPAL_C_API OpalResult OPAL_C_CALL
opal_texture_change_border_color(OpalTexture *handle, const float color[4]);

OPAL_C_API OpalResult OPAL_C_CALL opal_texture_read_data(
    OpalTexture *handle, void *buffer, OpalTextureDataFormat format);

OPAL_C_API OpalResult OPAL_C_CALL
opal_texture_generate_mipmaps(OpalTexture *handle, uint32_t levels);

OPAL_C_API OpalResult OPAL_C_CALL
opal_texture_automatically_generate_mipmaps(OpalTexture *handle);

OPAL_C_API OpalResult OPAL_C_CALL opal_texture_set_wrap_mode(
    OpalTexture *handle, OpalTextureAxis axis, OpalTextureWrapMode mode);

OPAL_C_API OpalResult OPAL_C_CALL opal_texture_set_filter_mode(
    OpalTexture *handle, OpalTextureFilterMode min_filter,
    OpalTextureFilterMode mag_filter);

OPAL_C_API OpalResult OPAL_C_CALL opal_texture_set_parameters(
    OpalTexture *handle, OpalTextureWrapMode s, OpalTextureWrapMode t,
    OpalTextureFilterMode min_filter, OpalTextureFilterMode mag_filter);

OPAL_C_API OpalResult OPAL_C_CALL opal_texture_set_parameters_3d(
    OpalTexture *handle, OpalTextureWrapMode s, OpalTextureWrapMode t,
    OpalTextureWrapMode r, OpalTextureFilterMode min_filter,
    OpalTextureFilterMode mag_filter);

OPAL_C_API OpalResult OPAL_C_CALL opal_texture_get_info(
    OpalTexture *handle, OpalTextureType *type, OpalTextureFormat *format,
    int32_t *width, int32_t *height, uint32_t *mip_levels, int32_t *samples);

OPAL_C_API OpalResult OPAL_C_CALL
opal_texture_get_metal_texture(OpalTexture *handle, void **out);

OPAL_C_API OpalResult OPAL_C_CALL
opal_shader_create_from_source(const char *source, OpalShaderType type,
                               const char *entry, OpalShader **out);

OPAL_C_API OpalResult OPAL_C_CALL opal_shader_for_function(OpalShader *shader,
                                                           const char *name,
                                                           OpalShaderType type,
                                                           OpalShader **out);

OPAL_C_API OpalResult OPAL_C_CALL opal_shader_compile(OpalShader *handle);

OPAL_C_API OpalResult OPAL_C_CALL opal_shader_get_status(OpalShader *handle,
                                                         int32_t *out);

OPAL_C_API OpalResult OPAL_C_CALL opal_shader_get_log(OpalShader *handle,
                                                      char *buffer,
                                                      size_t capacity);

OPAL_C_API OpalResult OPAL_C_CALL
opal_shader_program_link(OpalShaderProgram *handle);

OPAL_C_API OpalResult OPAL_C_CALL
opal_shader_program_get_status(OpalShaderProgram *handle, int32_t *out);

OPAL_C_API OpalResult OPAL_C_CALL opal_shader_program_get_log(
    OpalShaderProgram *handle, char *buffer, size_t capacity);

OPAL_C_API OpalResult OPAL_C_CALL
opal_shader_program_create(OpalShaderProgram **out);

OPAL_C_API OpalResult OPAL_C_CALL opal_shader_program_attach_shader(
    OpalShaderProgram *handle, OpalShader *shader, int32_t caller_id);

OPAL_C_API OpalResult OPAL_C_CALL
opal_shader_program_use(OpalShaderProgram *handle);

OPAL_C_API OpalResult OPAL_C_CALL
opal_shader_program_is_compute_program(OpalShaderProgram *handle, int32_t *out);

OPAL_C_API OpalResult OPAL_C_CALL opal_shader_program_is_ray_tracing_program(
    OpalShaderProgram *handle, int32_t *out);

OPAL_C_API OpalResult OPAL_C_CALL opal_pipeline_create(OpalPipeline **out);

OPAL_C_API OpalResult OPAL_C_CALL opal_pipeline_set_shader_program(
    OpalPipeline *handle, OpalShaderProgram *program);

OPAL_C_API OpalResult OPAL_C_CALL opal_pipeline_set_vertex_attributes(
    OpalPipeline *handle, const OpalVertexAttribute *attributes, size_t count,
    const OpalVertexBinding *binding);

OPAL_C_API OpalResult OPAL_C_CALL opal_pipeline_set_primitive_style(
    OpalPipeline *handle, OpalPrimitiveStyle value);

OPAL_C_API OpalResult OPAL_C_CALL opal_pipeline_set_rasterizer_mode(
    OpalPipeline *handle, OpalRasterizerMode value);

OPAL_C_API OpalResult OPAL_C_CALL
opal_pipeline_set_cull_mode(OpalPipeline *handle, OpalCullMode value);

OPAL_C_API OpalResult OPAL_C_CALL
opal_pipeline_set_front_face(OpalPipeline *handle, OpalFrontFace value);

OPAL_C_API OpalResult OPAL_C_CALL
opal_pipeline_set_depth_compare_op(OpalPipeline *handle, OpalCompareOp value);

OPAL_C_API OpalResult OPAL_C_CALL
opal_pipeline_set_blend_equation(OpalPipeline *handle, OpalBlendEquation value);

OPAL_C_API OpalResult OPAL_C_CALL
opal_pipeline_set_logic_op(OpalPipeline *handle, OpalLogicOp value);

OPAL_C_API OpalResult OPAL_C_CALL
opal_pipeline_enable_depth_test(OpalPipeline *handle, int32_t enabled);

OPAL_C_API OpalResult OPAL_C_CALL
opal_pipeline_enable_depth_write(OpalPipeline *handle, int32_t enabled);

OPAL_C_API OpalResult OPAL_C_CALL
opal_pipeline_enable_blending(OpalPipeline *handle, int32_t enabled);

OPAL_C_API OpalResult OPAL_C_CALL
opal_pipeline_enable_logic_op(OpalPipeline *handle, int32_t enabled);

OPAL_C_API OpalResult OPAL_C_CALL
opal_pipeline_enable_multisampling(OpalPipeline *handle, int32_t enabled);

OPAL_C_API OpalResult OPAL_C_CALL
opal_pipeline_enable_polygon_offset(OpalPipeline *handle, int32_t enabled);

OPAL_C_API OpalResult OPAL_C_CALL
opal_pipeline_set_patch_vertices(OpalPipeline *handle, int32_t count);

OPAL_C_API OpalResult OPAL_C_CALL opal_pipeline_set_viewport(
    OpalPipeline *handle, int32_t x, int32_t y, int32_t width, int32_t height);

OPAL_C_API OpalResult OPAL_C_CALL
opal_pipeline_set_line_width(OpalPipeline *handle, float width);

OPAL_C_API OpalResult OPAL_C_CALL opal_pipeline_set_compute_threadgroup_size(
    OpalPipeline *handle, uint32_t x, uint32_t y, uint32_t z);

OPAL_C_API OpalResult OPAL_C_CALL opal_pipeline_set_blend_func(
    OpalPipeline *handle, OpalBlendFunc src, OpalBlendFunc dst);

OPAL_C_API OpalResult OPAL_C_CALL
opal_pipeline_set_color_write_mask(OpalPipeline *handle, int32_t red,
                                   int32_t green, int32_t blue, int32_t alpha);

OPAL_C_API OpalResult OPAL_C_CALL opal_pipeline_set_polygon_offset(
    OpalPipeline *handle, float factor, float units);

OPAL_C_API OpalResult OPAL_C_CALL opal_pipeline_enable_clip_distance(
    OpalPipeline *handle, int32_t index, int32_t enabled);

OPAL_C_API OpalResult OPAL_C_CALL
opal_pipeline_get_patch_vertices(OpalPipeline *handle, int32_t *out);

OPAL_C_API OpalResult OPAL_C_CALL opal_pipeline_get_compute_threadgroup_size_x(
    OpalPipeline *handle, uint32_t *out);

OPAL_C_API OpalResult OPAL_C_CALL opal_pipeline_get_compute_threadgroup_size_y(
    OpalPipeline *handle, uint32_t *out);

OPAL_C_API OpalResult OPAL_C_CALL opal_pipeline_get_compute_threadgroup_size_z(
    OpalPipeline *handle, uint32_t *out);

OPAL_C_API OpalResult OPAL_C_CALL opal_pipeline_build(OpalPipeline *handle);

OPAL_C_API OpalResult OPAL_C_CALL opal_pipeline_bind(OpalPipeline *handle);

OPAL_C_API OpalResult OPAL_C_CALL opal_pipeline_set_uniform1i(
    OpalPipeline *handle, const char *name, int32_t value);

OPAL_C_API OpalResult OPAL_C_CALL opal_pipeline_set_uniform1f(
    OpalPipeline *handle, const char *name, float value);

OPAL_C_API OpalResult OPAL_C_CALL opal_pipeline_set_uniform_bool(
    OpalPipeline *handle, const char *name, int32_t value);

OPAL_C_API OpalResult OPAL_C_CALL opal_pipeline_set_uniform2f(
    OpalPipeline *handle, const char *name, float x, float y);

OPAL_C_API OpalResult OPAL_C_CALL opal_pipeline_set_uniform3f(
    OpalPipeline *handle, const char *name, float x, float y, float z);

OPAL_C_API OpalResult OPAL_C_CALL opal_pipeline_set_uniform4f(
    OpalPipeline *handle, const char *name, float x, float y, float z, float w);

OPAL_C_API OpalResult OPAL_C_CALL opal_pipeline_set_uniform_mat4f(
    OpalPipeline *handle, const char *name, const float values[16]);

OPAL_C_API OpalResult OPAL_C_CALL opal_pipeline_bind_buffer_data(
    OpalPipeline *handle, const char *name, const void *data, size_t size);

OPAL_C_API OpalResult OPAL_C_CALL
opal_pipeline_bind_buffer(OpalPipeline *handle, const char *name,
                          OpalBuffer *buffer, int32_t caller_id);

OPAL_C_API OpalResult OPAL_C_CALL opal_pipeline_bind_shader_read_write_buffer(
    OpalPipeline *handle, const char *name, OpalBuffer *buffer,
    int32_t caller_id);

OPAL_C_API OpalResult OPAL_C_CALL opal_pipeline_bind_texture(
    OpalPipeline *handle, const char *name, OpalTexture *texture, int32_t unit,
    int32_t caller_id);

OPAL_C_API OpalResult OPAL_C_CALL opal_pipeline_bind_texture_2d(
    OpalPipeline *handle, const char *name, uint32_t texture_id, int32_t unit,
    int32_t caller_id);

OPAL_C_API OpalResult OPAL_C_CALL opal_pipeline_bind_texture_3d(
    OpalPipeline *handle, const char *name, uint32_t texture_id, int32_t unit,
    int32_t caller_id);

OPAL_C_API OpalResult OPAL_C_CALL opal_pipeline_bind_texture_cubemap(
    OpalPipeline *handle, const char *name, uint32_t texture_id, int32_t unit,
    int32_t caller_id);

OPAL_C_API OpalResult OPAL_C_CALL opal_pipeline_bind_texture_array(
    OpalPipeline *handle, OpalTexture *const *textures, size_t count,
    uint32_t buffer_index);

OPAL_C_API OpalResult OPAL_C_CALL opal_buffer_create(
    OpalBufferUsage usage, size_t size, const void *data,
    OpalMemoryUsageType memory_usage, int32_t caller_id, OpalBuffer **out);

OPAL_C_API OpalResult OPAL_C_CALL opal_buffer_update_data(OpalBuffer *handle,
                                                          size_t offset,
                                                          size_t size,
                                                          const void *data);

OPAL_C_API OpalResult OPAL_C_CALL opal_buffer_bind(OpalBuffer *handle,
                                                   int32_t caller_id);

OPAL_C_API OpalResult OPAL_C_CALL opal_buffer_unbind(OpalBuffer *handle,
                                                     int32_t caller_id);

OPAL_C_API OpalResult OPAL_C_CALL
opal_drawing_state_create(OpalBuffer *vertex_buffer, OpalBuffer *index_buffer,
                          OpalDrawingState **out);

OPAL_C_API OpalResult OPAL_C_CALL opal_drawing_state_set_buffers(
    OpalDrawingState *handle, OpalBuffer *vertex_buffer,
    OpalBuffer *index_buffer);

OPAL_C_API OpalResult OPAL_C_CALL opal_drawing_state_set_instance_buffer(
    OpalDrawingState *handle, OpalBuffer *buffer);

OPAL_C_API OpalResult OPAL_C_CALL
opal_drawing_state_bind(OpalDrawingState *handle);

OPAL_C_API OpalResult OPAL_C_CALL
opal_drawing_state_unbind(OpalDrawingState *handle);

OPAL_C_API OpalResult OPAL_C_CALL opal_drawing_state_configure_attributes(
    OpalDrawingState *handle, const OpalVertexAttributeBinding *bindings,
    size_t count);

OPAL_C_API OpalResult OPAL_C_CALL opal_depth_stencil_buffer_create(
    int32_t width, int32_t height, OpalTextureFormat format,
    OpalDepthStencilBuffer **out);

OPAL_C_API OpalResult OPAL_C_CALL opal_texture_get_depth_stencil_texture(
    OpalDepthStencilBuffer *buffer, OpalTexture **out);

OPAL_C_API OpalResult OPAL_C_CALL
opal_framebuffer_create(int32_t width, int32_t height, OpalFramebuffer **out);

OPAL_C_API OpalResult OPAL_C_CALL
opal_framebuffer_create_empty(OpalFramebuffer **out);

OPAL_C_API OpalResult OPAL_C_CALL opal_framebuffer_add_attachment(
    OpalFramebuffer *handle, OpalAttachmentType type, OpalTexture *texture);

OPAL_C_API OpalResult OPAL_C_CALL opal_framebuffer_attach_depth_stencil_buffer(
    OpalFramebuffer *handle, OpalDepthStencilBuffer *buffer);

OPAL_C_API OpalResult OPAL_C_CALL opal_framebuffer_attach_texture(
    OpalFramebuffer *handle, OpalTexture *texture, int32_t index);

OPAL_C_API OpalResult OPAL_C_CALL opal_framebuffer_attach_cubemap(
    OpalFramebuffer *handle, OpalTexture *texture, OpalAttachmentType type);

OPAL_C_API OpalResult OPAL_C_CALL opal_framebuffer_attach_cubemap_face(
    OpalFramebuffer *handle, OpalTexture *texture, int32_t face,
    OpalAttachmentType type);

OPAL_C_API OpalResult OPAL_C_CALL
opal_framebuffer_disable_color_buffer(OpalFramebuffer *handle);

OPAL_C_API OpalResult OPAL_C_CALL
opal_framebuffer_set_viewport(OpalFramebuffer *handle);

OPAL_C_API OpalResult OPAL_C_CALL
opal_framebuffer_bind(OpalFramebuffer *handle);

OPAL_C_API OpalResult OPAL_C_CALL
opal_framebuffer_unbind(OpalFramebuffer *handle);

OPAL_C_API OpalResult OPAL_C_CALL
opal_framebuffer_bind_for_read(OpalFramebuffer *handle);

OPAL_C_API OpalResult OPAL_C_CALL
opal_framebuffer_bind_for_draw(OpalFramebuffer *handle);

OPAL_C_API OpalResult OPAL_C_CALL
opal_framebuffer_set_custom_viewport(OpalFramebuffer *handle, int32_t x,
                                     int32_t y, int32_t width, int32_t height);

OPAL_C_API OpalResult OPAL_C_CALL
opal_framebuffer_set_draw_buffers(OpalFramebuffer *handle, int32_t count);

OPAL_C_API OpalResult OPAL_C_CALL
opal_framebuffer_get_draw_buffer_count(OpalFramebuffer *handle, int32_t *out);

OPAL_C_API OpalResult OPAL_C_CALL
opal_framebuffer_get_status(OpalFramebuffer *handle, int32_t *out);

OPAL_C_API OpalResult OPAL_C_CALL opal_framebuffer_get_size(
    OpalFramebuffer *handle, int32_t *width, int32_t *height);

OPAL_C_API OpalResult OPAL_C_CALL opal_render_pass_create(OpalRenderPass **out);

OPAL_C_API OpalResult OPAL_C_CALL opal_render_pass_set_framebuffer(
    OpalRenderPass *handle, OpalFramebuffer *framebuffer);

OPAL_C_API OpalResult OPAL_C_CALL opal_resolve_action_create(
    OpalFramebuffer *source, OpalFramebuffer *destination,
    OpalResolveAction **out);

OPAL_C_API OpalResult OPAL_C_CALL
opal_resolve_action_create_for_color_attachment(OpalFramebuffer *source,
                                                OpalFramebuffer *destination,
                                                int32_t index,
                                                OpalResolveAction **out);

OPAL_C_API OpalResult OPAL_C_CALL opal_resolve_action_create_for_depth(
    OpalFramebuffer *source, OpalFramebuffer *destination,
    OpalResolveAction **out);

OPAL_C_API OpalResult OPAL_C_CALL
opal_resolve_action_set_options(OpalResolveAction *handle, int32_t color_index,
                                int32_t resolve_depth, int32_t resolve_color);

OPAL_C_API OpalResult OPAL_C_CALL
opal_command_buffer_start(OpalCommandBuffer *handle);

OPAL_C_API OpalResult OPAL_C_CALL
opal_command_buffer_end_pass(OpalCommandBuffer *handle);

OPAL_C_API OpalResult OPAL_C_CALL
opal_command_buffer_commit(OpalCommandBuffer *handle);

OPAL_C_API OpalResult OPAL_C_CALL
opal_command_buffer_wait_for_submitted_work(OpalCommandBuffer *handle);

OPAL_C_API OpalResult OPAL_C_CALL
opal_command_buffer_unbind_pipeline(OpalCommandBuffer *handle);

OPAL_C_API OpalResult OPAL_C_CALL
opal_command_buffer_reset_scissor(OpalCommandBuffer *handle);

OPAL_C_API OpalResult OPAL_C_CALL
opal_command_buffer_unbind_drawing_state(OpalCommandBuffer *handle);

OPAL_C_API OpalResult OPAL_C_CALL
opal_command_buffer_compute_barrier(OpalCommandBuffer *handle);

OPAL_C_API OpalResult OPAL_C_CALL
opal_command_buffer_begin_pass(OpalCommandBuffer *handle, OpalRenderPass *pass);

OPAL_C_API OpalResult OPAL_C_CALL opal_command_buffer_bind_pipeline(
    OpalCommandBuffer *handle, OpalPipeline *pipeline);

OPAL_C_API OpalResult OPAL_C_CALL opal_command_buffer_bind_drawing_state(
    OpalCommandBuffer *handle, OpalDrawingState *state);

OPAL_C_API OpalResult OPAL_C_CALL opal_command_buffer_generate_mipmaps(
    OpalCommandBuffer *handle, OpalTexture *texture);

OPAL_C_API OpalResult OPAL_C_CALL opal_command_buffer_perform_resolve(
    OpalCommandBuffer *handle, OpalResolveAction *action);

OPAL_C_API OpalResult OPAL_C_CALL opal_command_buffer_begin_sampled(
    OpalCommandBuffer *handle, OpalFramebuffer *read_framebuffer,
    OpalFramebuffer *write_framebuffer);

OPAL_C_API OpalResult OPAL_C_CALL
opal_command_buffer_set_scissor(OpalCommandBuffer *handle, int32_t x, int32_t y,
                                int32_t width, int32_t height);

OPAL_C_API OpalResult OPAL_C_CALL opal_command_buffer_draw(
    OpalCommandBuffer *handle, uint32_t vertex_count, uint32_t instance_count,
    uint32_t first_vertex, uint32_t first_instance, int32_t object_id);

OPAL_C_API OpalResult OPAL_C_CALL opal_command_buffer_draw_indexed(
    OpalCommandBuffer *handle, uint32_t index_count, uint32_t instance_count,
    uint32_t first_index, int32_t vertex_offset, uint32_t first_instance,
    int32_t object_id);

OPAL_C_API OpalResult OPAL_C_CALL opal_command_buffer_draw_patches(
    OpalCommandBuffer *handle, uint32_t vertex_count, uint32_t first_vertex,
    int32_t object_id);

OPAL_C_API OpalResult OPAL_C_CALL opal_command_buffer_dispatch(
    OpalCommandBuffer *handle, uint32_t x, uint32_t y, uint32_t z);

OPAL_C_API OpalResult OPAL_C_CALL opal_command_buffer_dispatch_rays(
    OpalCommandBuffer *handle, uint32_t width, uint32_t height, uint32_t depth);

OPAL_C_API OpalResult OPAL_C_CALL opal_command_buffer_clear_color(
    OpalCommandBuffer *handle, float r, float g, float b, float a);

OPAL_C_API OpalResult OPAL_C_CALL
opal_command_buffer_clear_depth(OpalCommandBuffer *handle, float depth);

OPAL_C_API OpalResult OPAL_C_CALL opal_command_buffer_clear(
    OpalCommandBuffer *handle, float r, float g, float b, float a, float depth);

OPAL_C_API OpalResult OPAL_C_CALL opal_command_buffer_perform_spatial_upscale(
    OpalCommandBuffer *handle, OpalTexture *source, int32_t *out);

OPAL_C_API OpalResult OPAL_C_CALL
opal_command_buffer_get_and_reset_draw_call_count(OpalCommandBuffer *handle,
                                                  int32_t *out);

OPAL_C_API OpalResult OPAL_C_CALL opal_primitive_acceleration_structure_create(
    const OpalPrimitiveVertex *vertices, size_t vertex_count,
    const uint32_t *indices, size_t index_count,
    OpalPrimitiveAccelerationStructure **out);

OPAL_C_API OpalResult OPAL_C_CALL
opal_primitive_acceleration_structure_create_positions(
    const float *positions, size_t position_count, const uint32_t *indices,
    size_t index_count, OpalPrimitiveAccelerationStructure **out);

OPAL_C_API OpalResult OPAL_C_CALL
opal_primitive_acceleration_structure_create_geometries(
    const OpalGeometry *geometries, size_t count,
    OpalPrimitiveAccelerationStructure **out);

OPAL_C_API OpalResult OPAL_C_CALL opal_instance_acceleration_structure_create(
    const OpalAccelerationStructureInstance *values, size_t count,
    OpalInstanceAccelerationStructure **out);

OPAL_C_API OpalResult OPAL_C_CALL
opal_primitive_acceleration_structure_is_built(
    OpalPrimitiveAccelerationStructure *handle, int32_t *out);

OPAL_C_API OpalResult OPAL_C_CALL
opal_primitive_acceleration_structure_native_handle(
    OpalPrimitiveAccelerationStructure *handle, uint64_t *out);

OPAL_C_API OpalResult OPAL_C_CALL
opal_command_buffer_build_primitive_acceleration_structure(
    OpalCommandBuffer *handle, OpalPrimitiveAccelerationStructure *structure);

OPAL_C_API OpalResult OPAL_C_CALL
opal_command_buffer_bind_primitive_acceleration_structure(
    OpalCommandBuffer *handle, OpalPrimitiveAccelerationStructure *structure,
    uint32_t binding);

OPAL_C_API OpalResult OPAL_C_CALL opal_instance_acceleration_structure_is_built(
    OpalInstanceAccelerationStructure *handle, int32_t *out);

OPAL_C_API OpalResult OPAL_C_CALL
opal_instance_acceleration_structure_native_handle(
    OpalInstanceAccelerationStructure *handle, uint64_t *out);

OPAL_C_API OpalResult OPAL_C_CALL
opal_command_buffer_build_instance_acceleration_structure(
    OpalCommandBuffer *handle, OpalInstanceAccelerationStructure *structure);

OPAL_C_API OpalResult OPAL_C_CALL
opal_command_buffer_bind_instance_acceleration_structure(
    OpalCommandBuffer *handle, OpalInstanceAccelerationStructure *structure,
    uint32_t binding);

OPAL_C_API OpalResult OPAL_C_CALL opal_instance_acceleration_structure_build(
    OpalCommandBuffer *commands,
    OpalPrimitiveAccelerationStructure *const *blases, size_t blas_count,
    const OpalAccelerationStructureInstance *values, size_t count,
    OpalInstanceAccelerationStructure **out);

OPAL_C_API OpalResult OPAL_C_CALL opal_texture_get_id(OpalTexture *handle,
                                                      uint32_t *out);

OPAL_C_API OpalResult OPAL_C_CALL opal_buffer_get_id(OpalBuffer *handle,
                                                     uint32_t *out);

OPAL_C_API OpalResult OPAL_C_CALL opal_shader_get_id(OpalShader *handle,
                                                     uint32_t *out);

OPAL_C_API OpalResult OPAL_C_CALL
opal_shader_program_get_id(OpalShaderProgram *handle, uint32_t *out);

OPAL_C_API OpalResult OPAL_C_CALL
opal_framebuffer_get_id(OpalFramebuffer *handle, uint32_t *out);

OPAL_C_API OpalResult OPAL_C_CALL
opal_depth_stencil_buffer_get_id(OpalDepthStencilBuffer *handle, uint32_t *out);

OPAL_C_API OpalResult OPAL_C_CALL
opal_drawing_state_get_id(OpalDrawingState *handle, uint32_t *out);

OPAL_C_API OpalResult OPAL_C_CALL
opal_set_log_callback(OpalLogCallback callback, void *user_data);

OPAL_C_API OpalResult OPAL_C_CALL
opal_set_resource_callback(OpalResourceCallback callback, void *user_data);

OPAL_C_API OpalResult OPAL_C_CALL
opal_set_draw_callback(OpalDrawCallback callback, void *user_data);

#ifdef __cplusplus
}

#endif

#endif
