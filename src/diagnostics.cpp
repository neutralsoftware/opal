#include "diagnostics.h"
#include <atomic>
#include <iostream>

namespace {
std::atomic<opal::LogCallback> callback = nullptr;
std::atomic<opal::ResourceCallback> resourceCallback = nullptr;
std::atomic<opal::DrawCallback> drawCallback = nullptr;
}

void opal::setLogCallback(LogCallback value) { callback.store(value); }
void opal::setResourceCallback(ResourceCallback value) {
    resourceCallback.store(value);
}
void opal::setDrawCallback(DrawCallback value) { drawCallback.store(value); }

void opal::detail::log(LogLevel level, const std::string &message) {
    if (auto current = callback.load()) {
        current(level, message.c_str());
        return;
    }
    std::ostream &stream = level == LogLevel::Info ? std::cout : std::cerr;
    stream << "Opal: " << message << '\n';
}

void opal::detail::emit(const ResourceEvent &event) {
    if (auto current = resourceCallback.load()) {
        current(event);
    }
}

void opal::detail::emit(const DrawEvent &event) {
    if (auto current = drawCallback.load()) {
        current(event);
    }
}
