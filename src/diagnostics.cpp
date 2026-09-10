#include "diagnostics.h"
#include <atomic>
#include <iostream>

namespace {
std::atomic<opal::LogCallback> callback = nullptr;
}

void opal::setLogCallback(LogCallback value) { callback.store(value); }

void opal::detail::log(LogLevel level, const std::string &message) {
    if (auto current = callback.load()) {
        current(level, message.c_str());
        return;
    }
    std::ostream &stream = level == LogLevel::Info ? std::cout : std::cerr;
    stream << "Opal: " << message << '\n';
}
