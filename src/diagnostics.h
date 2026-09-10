#ifndef OPAL_DIAGNOSTICS_H
#define OPAL_DIAGNOSTICS_H

#include <opal/opal.h>
#include <string>

namespace opal::detail {
void log(LogLevel level, const std::string &message);
}

#endif
