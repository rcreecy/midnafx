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
bool water_classification_diagnostic_enabled();
bool water_scene_capture_diagnostic_enabled();
bool water_surface_capture_diagnostic_enabled();
bool water_thickness_diagnostic_enabled();
bool enhanced_water_enabled();
float water_absorption_strength();
float water_max_optical_depth();
float water_wave_strength();
bool camera_enabled();
float camera_fov_scale();
float camera_transition_seconds();
bool camera_lower_angle_enabled();
float camera_angle_reduction();
bool atmosphere_depth_probe_enabled();
bool atmosphere_depth_view_enabled();
float atmosphere_depth_distance();
bool dof_coc_view_enabled();
bool dof_blur_enabled();
bool dof_autofocus_enabled();
float dof_focus_distance();
float dof_focus_range();
float dof_blur_radius();
float dof_focus_transition_seconds();
bool passthrough_test();
std::int64_t split_percent();
grade::Prepared prepared_grade();
void update_diagnostics();
void update_twilight(twilight::State state, float elapsed_seconds);
void shutdown();
} // namespace midnafx::settings
