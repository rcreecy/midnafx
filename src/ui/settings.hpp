#pragma once
#include "config/grade.hpp"
#include "game/twilight.hpp"

namespace midnafx::settings {
bool initialize();
bool enabled();
bool diagnostics_enabled();
bool geometry_diagnostics_enabled();
bool topology_diagnostics_enabled();
bool geometry_mutation_test_enabled();
bool geometry_smoothing_enabled();
bool geometry_skinned_smoothing_enabled();
float geometry_smoothing_angle();
bool camera_enabled();
float camera_fov_scale();
float camera_transition_seconds();
bool camera_lower_angle_enabled();
float camera_angle_reduction();
bool passthrough_test();
std::int64_t split_percent();
grade::Prepared prepared_grade();
void update_twilight(twilight::State state, float elapsed_seconds);
void shutdown();
} // namespace midnafx::settings
