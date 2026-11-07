#include <exception>
#include <iostream>
#include <opal/opal.h>

void run() {
    opal::ContextConfiguration configuration;
    configuration.applicationName = "Opal clear example";
    auto context = opal::Context::create(configuration);
    context->makeWindow(960, 540, "Opal clear example");
    auto device = opal::Device::acquire(context);
    auto commands = device->acquireCommandBuffer();
    auto pass = opal::RenderPass::create();
    pass->setFramebuffer(device->getDefaultFramebuffer());
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT ||
                event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                running = false;
            }
        }
        if (!running) {
            break;
        }
        if (SDL_GetWindowFlags(context->getWindow()) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(16);
            continue;
        }
        commands->start();
#ifdef METAL
        commands->clearColor(0.08f, 0.18f, 0.35f, 1.0f);
#endif
        commands->beginPass(pass);
#ifndef METAL
        commands->clearColor(0.08f, 0.18f, 0.35f, 1.0f);
#endif
        commands->endPass();
        commands->commit();
#ifdef OPENGL
        SDL_GL_SwapWindow(context->getWindow());
#endif
        commands->waitForSubmittedWork();
    }
}

int main() {
    int result = 0;
    try {
        run();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    SDL_Quit();
    return result;
}
