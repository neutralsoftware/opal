#ifndef OPAL_WINDOWING_H
#define OPAL_WINDOWING_H

#include <SDL3/SDL.h>
#include <algorithm>

namespace opal::detail {
inline void getWindowSizeInPixels(SDL_Window *window, int *width, int *height) {
    int pixelWidth = 0;
    int pixelHeight = 0;
    if (window != nullptr && !SDL_GetWindowSizeInPixels(window, &pixelWidth, &pixelHeight)) {
        int windowWidth = 0;
        int windowHeight = 0;
        SDL_GetWindowSize(window, &windowWidth, &windowHeight);
        float scale = SDL_GetWindowDisplayScale(window);
        if (scale <= 0.0f) {
            scale = 1.0f;
        }
        pixelWidth = std::max(1, static_cast<int>(static_cast<float>(windowWidth) * scale));
        pixelHeight = std::max(1, static_cast<int>(static_cast<float>(windowHeight) * scale));
    }
    if (window != nullptr) {
        pixelWidth = std::max(1, pixelWidth);
        pixelHeight = std::max(1, pixelHeight);
    }
    if (width != nullptr) {
        *width = pixelWidth;
    }
    if (height != nullptr) {
        *height = pixelHeight;
    }
}
}

#endif
