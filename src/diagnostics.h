#ifndef OPAL_DIAGNOSTICS_H
#define OPAL_DIAGNOSTICS_H

#include <opal/opal.h>
#include <stdexcept>
#include <string>

namespace opal::detail {
void log(LogLevel level, const std::string &message);
inline void require(bool condition, const char *message) {
    if (!condition) {
        log(LogLevel::Error, message);
        throw std::invalid_argument(message);
    }
}
void emit(const ResourceEvent &event);
void emit(const DrawEvent &event);
}

#endif
