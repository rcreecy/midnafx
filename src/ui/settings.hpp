#pragma once
#include "config/grade.hpp"
#include "game/twilight.hpp"

namespace midnafx::settings {
bool initialize();
bool enabled();
bool diagnostics_enabled();
bool geometry_diagnostics_enabled();
bool geometry_mutation_test_enabled();
bool passthrough_test();
std::int64_t split_percent();
grade::Prepared prepared_grade();
void update_twilight(twilight::State state, float elapsed_seconds);
void shutdown();
} // namespace midnafx::settings
