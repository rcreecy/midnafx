#pragma once

namespace midnafx::render::dof_quality {
float advance_focus(float current, float target, float elapsed_seconds,
                    float transition_seconds);
} // namespace midnafx::render::dof_quality
