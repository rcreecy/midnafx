#pragma once

#include <cstdint>

namespace midnafx::camera_fov {
float scale_vertical(float native_degrees, float scale);
float advance(float current, float target, std::uint64_t tick_delta, float transition_seconds);
} // namespace midnafx::camera_fov
