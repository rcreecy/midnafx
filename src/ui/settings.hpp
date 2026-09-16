#pragma once
#include "config/grade.hpp"

namespace midnafx::settings {
bool initialize();
bool enabled();
bool diagnostics_enabled();
bool passthrough_test();
grade::Prepared prepared_grade();
void shutdown();
} // namespace midnafx::settings
