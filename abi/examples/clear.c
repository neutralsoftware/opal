#include <SDL3/SDL.h>
#include <opal_c.h>
#include <stdio.h>

#define CHECK(call)                                                            \
    do {                                                                       \
        if ((call) != OPAL_SUCCESS) {                                          \
            fprintf(stderr, "%s\n", opal_get_last_error());                    \
            result = 1;                                                        \
            goto cleanup;                                                      \
        }                                                                      \
    } while (0)

int main(void) {
    int result = 0;
    int running = 1;
    void *window = NULL;
    OpalContext *context = NULL;
    OpalDevice *device = NULL;
    OpalCommandBuffer *commands = NULL;
    OpalFramebuffer *framebuffer = NULL;
    OpalRenderPass *pass = NULL;
    OpalContextConfiguration config;
    CHECK(opal_context_configuration_default(&config));
    config.application_name = "Opal C example";
    CHECK(opal_context_create_with_configuration(&config, &context));
    CHECK(opal_context_make_window(context, 960, 540, "Opal C example"));
    CHECK(opal_context_get_window(context, &window));
    CHECK(opal_device_acquire(context, &device));
    CHECK(opal_command_buffer_acquire(device, &commands));
    CHECK(opal_framebuffer_get_default(device, &framebuffer));
    CHECK(opal_render_pass_create(&pass));
    CHECK(opal_render_pass_set_framebuffer(pass, framebuffer));
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT ||
                event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                running = 0;
            }
        }
        if (!running) {
            break;
        }
        if (SDL_GetWindowFlags((SDL_Window *)window) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(16);
            continue;
        }
        CHECK(opal_command_buffer_start(commands));
        if (opal_get_backend() == OPAL_BACKEND_METAL) {
            CHECK(opal_command_buffer_clear_color(commands, 0.08f, 0.18f, 0.35f,
                                                  1.0f));
        }
        CHECK(opal_command_buffer_begin_pass(commands, pass));
        if (opal_get_backend() != OPAL_BACKEND_METAL) {
            CHECK(opal_command_buffer_clear_color(commands, 0.08f, 0.18f, 0.35f,
                                                  1.0f));
        }
        CHECK(opal_command_buffer_end_pass(commands));
        CHECK(opal_command_buffer_commit(commands));
        if (opal_get_backend() == OPAL_BACKEND_OPENGL) {
            CHECK(opal_context_swap_window(context));
        }
        CHECK(opal_command_buffer_wait_for_submitted_work(commands));
    }
cleanup:
    opal_command_buffer_destroy(commands);
    opal_render_pass_destroy(pass);
    opal_framebuffer_destroy(framebuffer);
    opal_device_destroy(device);
    opal_context_destroy(context);
    SDL_Quit();
    return result;
}
