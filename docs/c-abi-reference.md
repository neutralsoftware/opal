# Opal C ABI function reference

Exact signatures are declared in [`opal_c.h`](../abi/include/opal_c.h). Read the [usage manual](c-abi.md) first for ownership, thread restrictions, error handling, default values, backend support, and recording order. All functions below are exported by every backend; backend-conditional operations return unsupported where unavailable.

The result-returning functions use the per-thread error state. Constructors return owned handles through `out`. Numeric ID and native-pointer queries return borrowed backend values; string queries copy into caller storage. Integer enum values and structure layouts are defined in the C header.

## General

Version/backend/error queries are infallible. Callback registrations accept nullable callbacks and a borrowed user-data pointer.

```c
const char *OPAL_C_CALL opal_get_last_error(void);
uint32_t OPAL_C_CALL opal_get_abi_version(void);
OpalBackend OPAL_C_CALL opal_get_backend(void);
OpalResult OPAL_C_CALL opal_set_log_callback(OpalLogCallback callback, void *user_data);
OpalResult OPAL_C_CALL opal_set_resource_callback(OpalResourceCallback callback, void *user_data);
OpalResult OPAL_C_CALL opal_set_draw_callback(OpalDrawCallback callback, void *user_data);
```

## Context

```c
OpalResult OPAL_C_CALL opal_context_configuration_default(OpalContextConfiguration *out);
OpalResult OPAL_C_CALL opal_context_create(OpalContext **out);
OpalResult OPAL_C_CALL opal_context_create_with_configuration( const OpalContextConfiguration *configuration, OpalContext **out);
OpalResult OPAL_C_CALL opal_context_destroy(OpalContext *handle);
OpalResult OPAL_C_CALL opal_context_set_decorated(OpalContext *handle, int32_t enabled);
OpalResult OPAL_C_CALL opal_context_set_resizable(OpalContext *handle, int32_t enabled);
OpalResult OPAL_C_CALL opal_context_set_transparent(OpalContext *handle, int32_t enabled);
OpalResult OPAL_C_CALL opal_context_set_always_on_top(OpalContext *handle, int32_t enabled);
OpalResult OPAL_C_CALL opal_context_set_high_pixel_density(OpalContext *handle, int32_t enabled);
OpalResult OPAL_C_CALL opal_context_set_hidden(OpalContext *handle, int32_t enabled);
OpalResult OPAL_C_CALL opal_context_set_samples(OpalContext *handle, int32_t samples);
OpalResult OPAL_C_CALL opal_context_make_window(OpalContext *context, int width, int height, const char *title);
OpalResult OPAL_C_CALL opal_context_make_window_on_display( OpalContext *handle, int32_t width, int32_t height, const char *title, uint32_t display_id);
OpalResult OPAL_C_CALL opal_context_make_window_from_native( OpalContext *handle, void *window, OpalNativeWindowType type, int32_t width, int32_t height, const char *title);
OpalResult OPAL_C_CALL opal_context_adopt_window( OpalContext *handle, void *window, int32_t take_ownership);
OpalResult OPAL_C_CALL opal_context_set_metal_target_view(OpalContext *handle, void *view);
OpalResult OPAL_C_CALL opal_context_get_window(OpalContext *handle, void **out);
OpalResult OPAL_C_CALL opal_context_get_metal_target_view(OpalContext *handle, void **out);
OpalResult OPAL_C_CALL opal_context_get_gl_context(OpalContext *handle, void **out);
OpalResult OPAL_C_CALL opal_context_make_current(OpalContext *handle);
OpalResult OPAL_C_CALL opal_context_swap_window(OpalContext *handle);
```

## Device

`get_info` supports five `OPAL_DEVICE_INFO_*` string fields. Pass a null buffer and capacity zero to query `required`, including the terminator; positive-capacity buffers are terminated and may be truncated. A truncated copy still succeeds, with `required` reporting the full size. `submit_command_buffer` mirrors the C++ no-op; submit using command-buffer commit.

```c
OpalResult OPAL_C_CALL opal_device_destroy(OpalDevice *handle);
OpalResult OPAL_C_CALL opal_device_acquire(OpalContext *context, OpalDevice **out);
OpalResult OPAL_C_CALL opal_device_submit_command_buffer( OpalDevice *handle, OpalCommandBuffer *commands);
OpalResult OPAL_C_CALL opal_device_get_info(OpalDevice *handle, OpalDeviceInfoField field, char *buffer, size_t capacity, size_t *required);
OpalResult OPAL_C_CALL opal_device_supports_ray_tracing(OpalDevice *handle, int32_t *out);
OpalResult OPAL_C_CALL opal_device_get_frame_count(OpalDevice *handle, int64_t *out);
OpalResult OPAL_C_CALL opal_device_get_metal_device(OpalDevice *handle, void **out);
```

## Command buffer

Start before recording, begin a framebuffer pass before drawing, bind a pipeline, end the pass, then commit. Wait before destroying referenced resources. OpenGL presentation uses context swap. Compute dispatch is Metal/Vulkan; ray dispatch is Vulkan. Direct BLAS binding is Metal-only.

```c
OpalResult OPAL_C_CALL opal_command_buffer_destroy(OpalCommandBuffer *handle);
OpalResult OPAL_C_CALL opal_command_buffer_acquire(OpalDevice *device, OpalCommandBuffer **out);
OpalResult OPAL_C_CALL opal_command_buffer_start(OpalCommandBuffer *handle);
OpalResult OPAL_C_CALL opal_command_buffer_end_pass(OpalCommandBuffer *handle);
OpalResult OPAL_C_CALL opal_command_buffer_commit(OpalCommandBuffer *handle);
OpalResult OPAL_C_CALL opal_command_buffer_wait_for_submitted_work(OpalCommandBuffer *handle);
OpalResult OPAL_C_CALL opal_command_buffer_unbind_pipeline(OpalCommandBuffer *handle);
OpalResult OPAL_C_CALL opal_command_buffer_reset_scissor(OpalCommandBuffer *handle);
OpalResult OPAL_C_CALL opal_command_buffer_unbind_drawing_state(OpalCommandBuffer *handle);
OpalResult OPAL_C_CALL opal_command_buffer_compute_barrier(OpalCommandBuffer *handle);
OpalResult OPAL_C_CALL opal_command_buffer_begin_pass(OpalCommandBuffer *handle, OpalRenderPass *pass);
OpalResult OPAL_C_CALL opal_command_buffer_bind_pipeline( OpalCommandBuffer *handle, OpalPipeline *pipeline);
OpalResult OPAL_C_CALL opal_command_buffer_bind_drawing_state( OpalCommandBuffer *handle, OpalDrawingState *state);
OpalResult OPAL_C_CALL opal_command_buffer_generate_mipmaps( OpalCommandBuffer *handle, OpalTexture *texture);
OpalResult OPAL_C_CALL opal_command_buffer_perform_resolve( OpalCommandBuffer *handle, OpalResolveAction *action);
OpalResult OPAL_C_CALL opal_command_buffer_begin_sampled( OpalCommandBuffer *handle, OpalFramebuffer *read_framebuffer, OpalFramebuffer *write_framebuffer);
OpalResult OPAL_C_CALL opal_command_buffer_set_scissor(OpalCommandBuffer *handle, int32_t x, int32_t y, int32_t width, int32_t height);
OpalResult OPAL_C_CALL opal_command_buffer_draw( OpalCommandBuffer *handle, uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance, int32_t object_id);
OpalResult OPAL_C_CALL opal_command_buffer_draw_indexed( OpalCommandBuffer *handle, uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance, int32_t object_id);
OpalResult OPAL_C_CALL opal_command_buffer_draw_patches( OpalCommandBuffer *handle, uint32_t vertex_count, uint32_t first_vertex, int32_t object_id);
OpalResult OPAL_C_CALL opal_command_buffer_dispatch( OpalCommandBuffer *handle, uint32_t x, uint32_t y, uint32_t z);
OpalResult OPAL_C_CALL opal_command_buffer_dispatch_rays( OpalCommandBuffer *handle, uint32_t width, uint32_t height, uint32_t depth);
OpalResult OPAL_C_CALL opal_command_buffer_clear_color( OpalCommandBuffer *handle, float r, float g, float b, float a);
OpalResult OPAL_C_CALL opal_command_buffer_clear_depth(OpalCommandBuffer *handle, float depth);
OpalResult OPAL_C_CALL opal_command_buffer_clear( OpalCommandBuffer *handle, float r, float g, float b, float a, float depth);
OpalResult OPAL_C_CALL opal_command_buffer_perform_spatial_upscale( OpalCommandBuffer *handle, OpalTexture *source, int32_t *out);
OpalResult OPAL_C_CALL opal_command_buffer_get_and_reset_draw_call_count(OpalCommandBuffer *handle, int32_t *out);
OpalResult OPAL_C_CALL opal_command_buffer_build_primitive_acceleration_structure( OpalCommandBuffer *handle, OpalPrimitiveAccelerationStructure *structure);
OpalResult OPAL_C_CALL opal_command_buffer_bind_primitive_acceleration_structure( OpalCommandBuffer *handle, OpalPrimitiveAccelerationStructure *structure, uint32_t binding);
OpalResult OPAL_C_CALL opal_command_buffer_build_instance_acceleration_structure( OpalCommandBuffer *handle, OpalInstanceAccelerationStructure *structure);
OpalResult OPAL_C_CALL opal_command_buffer_bind_instance_acceleration_structure( OpalCommandBuffer *handle, OpalInstanceAccelerationStructure *structure, uint32_t binding);
```

## Texture

Creation data is optional. Update/readback memory must contain sufficient bytes for the native transfer format. The depth-stencil texture accessor returns an owned handle. `get_info` requires all output pointers. Supported sample-count queries are Vulkan-only, Metal texture pointers are Metal-only.

```c
OpalResult OPAL_C_CALL opal_texture_destroy(OpalTexture *handle);
OpalResult OPAL_C_CALL opal_texture_create( OpalTextureType type, OpalTextureFormat format, int32_t width, int32_t height, OpalTextureDataFormat data_format, const void *data, uint32_t mip_levels, OpalTexture **out);
OpalResult OPAL_C_CALL opal_texture_create_multisampled( OpalTextureFormat format, int32_t width, int32_t height, int32_t samples, OpalTexture **out);
OpalResult OPAL_C_CALL opal_texture_create_depth_cubemap( OpalTextureFormat format, int32_t resolution, OpalTexture **out);
OpalResult OPAL_C_CALL opal_texture_create_3d( OpalTextureFormat format, int32_t width, int32_t height, int32_t depth, OpalTextureDataFormat data_format, const void *data, OpalTexture **out);
OpalResult OPAL_C_CALL opal_texture_get_supported_sample_count( const OpalTextureFormat *formats, size_t count, int32_t requested, int32_t *out);
OpalResult OPAL_C_CALL opal_texture_update_data(OpalTexture *handle, const void *data, int32_t width, int32_t height, OpalTextureDataFormat format);
OpalResult OPAL_C_CALL opal_texture_update_data_3d( OpalTexture *handle, const void *data, int32_t width, int32_t height, int32_t depth, OpalTextureDataFormat format);
OpalResult OPAL_C_CALL opal_texture_update_face( OpalTexture *handle, const void *data, int32_t face, int32_t width, int32_t height, OpalTextureDataFormat format);
OpalResult OPAL_C_CALL opal_texture_change_format(OpalTexture *handle, OpalTextureFormat format);
OpalResult OPAL_C_CALL opal_texture_change_border_color(OpalTexture *handle, const float color[4]);
OpalResult OPAL_C_CALL opal_texture_read_data( OpalTexture *handle, void *buffer, OpalTextureDataFormat format);
OpalResult OPAL_C_CALL opal_texture_generate_mipmaps(OpalTexture *handle, uint32_t levels);
OpalResult OPAL_C_CALL opal_texture_automatically_generate_mipmaps(OpalTexture *handle);
OpalResult OPAL_C_CALL opal_texture_set_wrap_mode( OpalTexture *handle, OpalTextureAxis axis, OpalTextureWrapMode mode);
OpalResult OPAL_C_CALL opal_texture_set_filter_mode( OpalTexture *handle, OpalTextureFilterMode min_filter, OpalTextureFilterMode mag_filter);
OpalResult OPAL_C_CALL opal_texture_set_parameters( OpalTexture *handle, OpalTextureWrapMode s, OpalTextureWrapMode t, OpalTextureFilterMode min_filter, OpalTextureFilterMode mag_filter);
OpalResult OPAL_C_CALL opal_texture_set_parameters_3d( OpalTexture *handle, OpalTextureWrapMode s, OpalTextureWrapMode t, OpalTextureWrapMode r, OpalTextureFilterMode min_filter, OpalTextureFilterMode mag_filter);
OpalResult OPAL_C_CALL opal_texture_get_info( OpalTexture *handle, OpalTextureType *type, OpalTextureFormat *format, int32_t *width, int32_t *height, uint32_t *mip_levels, int32_t *samples);
OpalResult OPAL_C_CALL opal_texture_get_metal_texture(OpalTexture *handle, void **out);
OpalResult OPAL_C_CALL opal_texture_get_depth_stencil_texture( OpalDepthStencilBuffer *buffer, OpalTexture **out);
OpalResult OPAL_C_CALL opal_texture_get_id(OpalTexture *handle, uint32_t *out);
```

## Shader

Check native status after compile/link even when the call succeeds. Log output requires a valid positive-capacity buffer; truncation is permitted and the result is terminated.

```c
OpalResult OPAL_C_CALL opal_shader_destroy(OpalShader *handle);
OpalResult OPAL_C_CALL opal_shader_create_from_source(const char *source, OpalShaderType type, const char *entry, OpalShader **out);
OpalResult OPAL_C_CALL opal_shader_for_function(OpalShader *shader, const char *name, OpalShaderType type, OpalShader **out);
OpalResult OPAL_C_CALL opal_shader_compile(OpalShader *handle);
OpalResult OPAL_C_CALL opal_shader_get_status(OpalShader *handle, int32_t *out);
OpalResult OPAL_C_CALL opal_shader_get_log(OpalShader *handle, char *buffer, size_t capacity);
OpalResult OPAL_C_CALL opal_shader_get_id(OpalShader *handle, uint32_t *out);
```

## Shader program

Check native status after compile/link even when the call succeeds. Log output requires a valid positive-capacity buffer; truncation is permitted and the result is terminated.

```c
OpalResult OPAL_C_CALL opal_shader_program_destroy(OpalShaderProgram *handle);
OpalResult OPAL_C_CALL opal_shader_program_link(OpalShaderProgram *handle);
OpalResult OPAL_C_CALL opal_shader_program_get_status(OpalShaderProgram *handle, int32_t *out);
OpalResult OPAL_C_CALL opal_shader_program_get_log( OpalShaderProgram *handle, char *buffer, size_t capacity);
OpalResult OPAL_C_CALL opal_shader_program_create(OpalShaderProgram **out);
OpalResult OPAL_C_CALL opal_shader_program_attach_shader( OpalShaderProgram *handle, OpalShader *shader, int32_t caller_id);
OpalResult OPAL_C_CALL opal_shader_program_use(OpalShaderProgram *handle);
OpalResult OPAL_C_CALL opal_shader_program_is_compute_program(OpalShaderProgram *handle, int32_t *out);
OpalResult OPAL_C_CALL opal_shader_program_is_ray_tracing_program( OpalShaderProgram *handle, int32_t *out);
OpalResult OPAL_C_CALL opal_shader_program_get_id(OpalShaderProgram *handle, uint32_t *out);
```

## Pipeline

Set state before build. Uniform matrix arrays are column-major. Vector uploads use `bind_buffer_data` with a byte count; other buffer bindings take handles. Use explicit caller IDs, usually `-1`. Compute threadgroup sizes must match shader local sizes on Vulkan.

```c
OpalResult OPAL_C_CALL opal_pipeline_destroy(OpalPipeline *handle);
OpalResult OPAL_C_CALL opal_pipeline_create(OpalPipeline **out);
OpalResult OPAL_C_CALL opal_pipeline_set_shader_program( OpalPipeline *handle, OpalShaderProgram *program);
OpalResult OPAL_C_CALL opal_pipeline_set_vertex_attributes( OpalPipeline *handle, const OpalVertexAttribute *attributes, size_t count, const OpalVertexBinding *binding);
OpalResult OPAL_C_CALL opal_pipeline_set_primitive_style( OpalPipeline *handle, OpalPrimitiveStyle value);
OpalResult OPAL_C_CALL opal_pipeline_set_rasterizer_mode( OpalPipeline *handle, OpalRasterizerMode value);
OpalResult OPAL_C_CALL opal_pipeline_set_cull_mode(OpalPipeline *handle, OpalCullMode value);
OpalResult OPAL_C_CALL opal_pipeline_set_front_face(OpalPipeline *handle, OpalFrontFace value);
OpalResult OPAL_C_CALL opal_pipeline_set_depth_compare_op(OpalPipeline *handle, OpalCompareOp value);
OpalResult OPAL_C_CALL opal_pipeline_set_blend_equation(OpalPipeline *handle, OpalBlendEquation value);
OpalResult OPAL_C_CALL opal_pipeline_set_logic_op(OpalPipeline *handle, OpalLogicOp value);
OpalResult OPAL_C_CALL opal_pipeline_enable_depth_test(OpalPipeline *handle, int32_t enabled);
OpalResult OPAL_C_CALL opal_pipeline_enable_depth_write(OpalPipeline *handle, int32_t enabled);
OpalResult OPAL_C_CALL opal_pipeline_enable_blending(OpalPipeline *handle, int32_t enabled);
OpalResult OPAL_C_CALL opal_pipeline_enable_logic_op(OpalPipeline *handle, int32_t enabled);
OpalResult OPAL_C_CALL opal_pipeline_enable_multisampling(OpalPipeline *handle, int32_t enabled);
OpalResult OPAL_C_CALL opal_pipeline_enable_polygon_offset(OpalPipeline *handle, int32_t enabled);
OpalResult OPAL_C_CALL opal_pipeline_set_patch_vertices(OpalPipeline *handle, int32_t count);
OpalResult OPAL_C_CALL opal_pipeline_set_viewport( OpalPipeline *handle, int32_t x, int32_t y, int32_t width, int32_t height);
OpalResult OPAL_C_CALL opal_pipeline_set_line_width(OpalPipeline *handle, float width);
OpalResult OPAL_C_CALL opal_pipeline_set_compute_threadgroup_size( OpalPipeline *handle, uint32_t x, uint32_t y, uint32_t z);
OpalResult OPAL_C_CALL opal_pipeline_set_blend_func( OpalPipeline *handle, OpalBlendFunc src, OpalBlendFunc dst);
OpalResult OPAL_C_CALL opal_pipeline_set_color_write_mask(OpalPipeline *handle, int32_t red, int32_t green, int32_t blue, int32_t alpha);
OpalResult OPAL_C_CALL opal_pipeline_set_polygon_offset( OpalPipeline *handle, float factor, float units);
OpalResult OPAL_C_CALL opal_pipeline_enable_clip_distance( OpalPipeline *handle, int32_t index, int32_t enabled);
OpalResult OPAL_C_CALL opal_pipeline_get_patch_vertices(OpalPipeline *handle, int32_t *out);
OpalResult OPAL_C_CALL opal_pipeline_get_compute_threadgroup_size_x( OpalPipeline *handle, uint32_t *out);
OpalResult OPAL_C_CALL opal_pipeline_get_compute_threadgroup_size_y( OpalPipeline *handle, uint32_t *out);
OpalResult OPAL_C_CALL opal_pipeline_get_compute_threadgroup_size_z( OpalPipeline *handle, uint32_t *out);
OpalResult OPAL_C_CALL opal_pipeline_build(OpalPipeline *handle);
OpalResult OPAL_C_CALL opal_pipeline_bind(OpalPipeline *handle);
OpalResult OPAL_C_CALL opal_pipeline_set_uniform1i( OpalPipeline *handle, const char *name, int32_t value);
OpalResult OPAL_C_CALL opal_pipeline_set_uniform1f( OpalPipeline *handle, const char *name, float value);
OpalResult OPAL_C_CALL opal_pipeline_set_uniform_bool( OpalPipeline *handle, const char *name, int32_t value);
OpalResult OPAL_C_CALL opal_pipeline_set_uniform2f( OpalPipeline *handle, const char *name, float x, float y);
OpalResult OPAL_C_CALL opal_pipeline_set_uniform3f( OpalPipeline *handle, const char *name, float x, float y, float z);
OpalResult OPAL_C_CALL opal_pipeline_set_uniform4f( OpalPipeline *handle, const char *name, float x, float y, float z, float w);
OpalResult OPAL_C_CALL opal_pipeline_set_uniform_mat4f( OpalPipeline *handle, const char *name, const float values[16]);
OpalResult OPAL_C_CALL opal_pipeline_bind_buffer_data( OpalPipeline *handle, const char *name, const void *data, size_t size);
OpalResult OPAL_C_CALL opal_pipeline_bind_buffer(OpalPipeline *handle, const char *name, OpalBuffer *buffer, int32_t caller_id);
OpalResult OPAL_C_CALL opal_pipeline_bind_shader_read_write_buffer( OpalPipeline *handle, const char *name, OpalBuffer *buffer, int32_t caller_id);
OpalResult OPAL_C_CALL opal_pipeline_bind_texture( OpalPipeline *handle, const char *name, OpalTexture *texture, int32_t unit, int32_t caller_id);
OpalResult OPAL_C_CALL opal_pipeline_bind_texture_2d( OpalPipeline *handle, const char *name, uint32_t texture_id, int32_t unit, int32_t caller_id);
OpalResult OPAL_C_CALL opal_pipeline_bind_texture_3d( OpalPipeline *handle, const char *name, uint32_t texture_id, int32_t unit, int32_t caller_id);
OpalResult OPAL_C_CALL opal_pipeline_bind_texture_cubemap( OpalPipeline *handle, const char *name, uint32_t texture_id, int32_t unit, int32_t caller_id);
OpalResult OPAL_C_CALL opal_pipeline_bind_texture_array( OpalPipeline *handle, OpalTexture *const *textures, size_t count, uint32_t buffer_index);
```

## Buffer

```c
OpalResult OPAL_C_CALL opal_buffer_destroy(OpalBuffer *handle);
OpalResult OPAL_C_CALL opal_buffer_create( OpalBufferUsage usage, size_t size, const void *data, OpalMemoryUsageType memory_usage, int32_t caller_id, OpalBuffer **out);
OpalResult OPAL_C_CALL opal_buffer_update_data(OpalBuffer *handle, size_t offset, size_t size, const void *data);
OpalResult OPAL_C_CALL opal_buffer_bind(OpalBuffer *handle, int32_t caller_id);
OpalResult OPAL_C_CALL opal_buffer_unbind(OpalBuffer *handle, int32_t caller_id);
OpalResult OPAL_C_CALL opal_buffer_get_id(OpalBuffer *handle, uint32_t *out);
```

## Drawing state

```c
OpalResult OPAL_C_CALL opal_drawing_state_destroy(OpalDrawingState *handle);
OpalResult OPAL_C_CALL opal_drawing_state_create(OpalBuffer *vertex_buffer, OpalBuffer *index_buffer, OpalDrawingState **out);
OpalResult OPAL_C_CALL opal_drawing_state_set_buffers( OpalDrawingState *handle, OpalBuffer *vertex_buffer, OpalBuffer *index_buffer);
OpalResult OPAL_C_CALL opal_drawing_state_set_instance_buffer( OpalDrawingState *handle, OpalBuffer *buffer);
OpalResult OPAL_C_CALL opal_drawing_state_bind(OpalDrawingState *handle);
OpalResult OPAL_C_CALL opal_drawing_state_unbind(OpalDrawingState *handle);
OpalResult OPAL_C_CALL opal_drawing_state_configure_attributes( OpalDrawingState *handle, const OpalVertexAttributeBinding *bindings, size_t count);
OpalResult OPAL_C_CALL opal_drawing_state_get_id(OpalDrawingState *handle, uint32_t *out);
```

## Depth stencil buffer

```c
OpalResult OPAL_C_CALL opal_depth_stencil_buffer_destroy(OpalDepthStencilBuffer *handle);
OpalResult OPAL_C_CALL opal_depth_stencil_buffer_create( int32_t width, int32_t height, OpalTextureFormat format, OpalDepthStencilBuffer **out);
OpalResult OPAL_C_CALL opal_depth_stencil_buffer_get_id(OpalDepthStencilBuffer *handle, uint32_t *out);
```

## Framebuffer

```c
OpalResult OPAL_C_CALL opal_framebuffer_destroy(OpalFramebuffer *handle);
OpalResult OPAL_C_CALL opal_framebuffer_get_default(OpalDevice *device, OpalFramebuffer **out);
OpalResult OPAL_C_CALL opal_framebuffer_create(int32_t width, int32_t height, OpalFramebuffer **out);
OpalResult OPAL_C_CALL opal_framebuffer_create_empty(OpalFramebuffer **out);
OpalResult OPAL_C_CALL opal_framebuffer_add_attachment( OpalFramebuffer *handle, OpalAttachmentType type, OpalTexture *texture);
OpalResult OPAL_C_CALL opal_framebuffer_attach_depth_stencil_buffer( OpalFramebuffer *handle, OpalDepthStencilBuffer *buffer);
OpalResult OPAL_C_CALL opal_framebuffer_attach_texture( OpalFramebuffer *handle, OpalTexture *texture, int32_t index);
OpalResult OPAL_C_CALL opal_framebuffer_attach_cubemap( OpalFramebuffer *handle, OpalTexture *texture, OpalAttachmentType type);
OpalResult OPAL_C_CALL opal_framebuffer_attach_cubemap_face( OpalFramebuffer *handle, OpalTexture *texture, int32_t face, OpalAttachmentType type);
OpalResult OPAL_C_CALL opal_framebuffer_disable_color_buffer(OpalFramebuffer *handle);
OpalResult OPAL_C_CALL opal_framebuffer_set_viewport(OpalFramebuffer *handle);
OpalResult OPAL_C_CALL opal_framebuffer_bind(OpalFramebuffer *handle);
OpalResult OPAL_C_CALL opal_framebuffer_unbind(OpalFramebuffer *handle);
OpalResult OPAL_C_CALL opal_framebuffer_bind_for_read(OpalFramebuffer *handle);
OpalResult OPAL_C_CALL opal_framebuffer_bind_for_draw(OpalFramebuffer *handle);
OpalResult OPAL_C_CALL opal_framebuffer_set_custom_viewport(OpalFramebuffer *handle, int32_t x, int32_t y, int32_t width, int32_t height);
OpalResult OPAL_C_CALL opal_framebuffer_set_draw_buffers(OpalFramebuffer *handle, int32_t count);
OpalResult OPAL_C_CALL opal_framebuffer_get_draw_buffer_count(OpalFramebuffer *handle, int32_t *out);
OpalResult OPAL_C_CALL opal_framebuffer_get_status(OpalFramebuffer *handle, int32_t *out);
OpalResult OPAL_C_CALL opal_framebuffer_get_size( OpalFramebuffer *handle, int32_t *width, int32_t *height);
OpalResult OPAL_C_CALL opal_framebuffer_get_id(OpalFramebuffer *handle, uint32_t *out);
```

## Render pass

```c
OpalResult OPAL_C_CALL opal_render_pass_destroy(OpalRenderPass *handle);
OpalResult OPAL_C_CALL opal_render_pass_create(OpalRenderPass **out);
OpalResult OPAL_C_CALL opal_render_pass_set_framebuffer( OpalRenderPass *handle, OpalFramebuffer *framebuffer);
```

## Resolve action

```c
OpalResult OPAL_C_CALL opal_resolve_action_destroy(OpalResolveAction *handle);
OpalResult OPAL_C_CALL opal_resolve_action_create( OpalFramebuffer *source, OpalFramebuffer *destination, OpalResolveAction **out);
OpalResult OPAL_C_CALL opal_resolve_action_create_for_color_attachment(OpalFramebuffer *source, OpalFramebuffer *destination, int32_t index, OpalResolveAction **out);
OpalResult OPAL_C_CALL opal_resolve_action_create_for_depth( OpalFramebuffer *source, OpalFramebuffer *destination, OpalResolveAction **out);
OpalResult OPAL_C_CALL opal_resolve_action_set_options(OpalResolveAction *handle, int32_t color_index, int32_t resolve_depth, int32_t resolve_color);
```

## Primitive acceleration structure

Metal/Vulkan acceleration structures require hardware support. Vulkan-only `native_handle` returns a borrowed uint64 handle. Flat position counts are numbers of floats; indices are uint32 triangle indices. Input handles must remain alive through GPU completion.

```c
OpalResult OPAL_C_CALL opal_primitive_acceleration_structure_destroy( OpalPrimitiveAccelerationStructure *handle);
OpalResult OPAL_C_CALL opal_primitive_acceleration_structure_create( const OpalPrimitiveVertex *vertices, size_t vertex_count, const uint32_t *indices, size_t index_count, OpalPrimitiveAccelerationStructure **out);
OpalResult OPAL_C_CALL opal_primitive_acceleration_structure_create_positions( const float *positions, size_t position_count, const uint32_t *indices, size_t index_count, OpalPrimitiveAccelerationStructure **out);
OpalResult OPAL_C_CALL opal_primitive_acceleration_structure_create_geometries( const OpalGeometry *geometries, size_t count, OpalPrimitiveAccelerationStructure **out);
OpalResult OPAL_C_CALL opal_primitive_acceleration_structure_is_built( OpalPrimitiveAccelerationStructure *handle, int32_t *out);
OpalResult OPAL_C_CALL opal_primitive_acceleration_structure_native_handle( OpalPrimitiveAccelerationStructure *handle, uint64_t *out);
```

## Instance acceleration structure

Metal/Vulkan acceleration structures require hardware support. Vulkan-only `native_handle` returns a borrowed uint64 handle. Flat position counts are numbers of floats; indices are uint32 triangle indices. Input handles must remain alive through GPU completion.

```c
OpalResult OPAL_C_CALL opal_instance_acceleration_structure_destroy( OpalInstanceAccelerationStructure *handle);
OpalResult OPAL_C_CALL opal_instance_acceleration_structure_create( const OpalAccelerationStructureInstance *values, size_t count, OpalInstanceAccelerationStructure **out);
OpalResult OPAL_C_CALL opal_instance_acceleration_structure_is_built( OpalInstanceAccelerationStructure *handle, int32_t *out);
OpalResult OPAL_C_CALL opal_instance_acceleration_structure_native_handle( OpalInstanceAccelerationStructure *handle, uint64_t *out);
OpalResult OPAL_C_CALL opal_instance_acceleration_structure_build( OpalCommandBuffer *commands, OpalPrimitiveAccelerationStructure *const *blases, size_t blas_count, const OpalAccelerationStructureInstance *values, size_t count, OpalInstanceAccelerationStructure **out);
```
