#pragma once

#include <cstdint>

namespace midnafx::camera_fov {
float scale_vertical(float native_degrees, float scale);
float advance(float current, float target, std::uint64_t tick_delta, float transition_seconds);
float movement_target(float speed, float max_speed);
struct MovementFraming {
    float amount = 0;
    std::uint64_t ticks = 0, frame = 0;
    bool sampled = false;
    void reset();
    float update(float speed, float max_speed, bool active, std::uint64_t next_ticks,
                 std::uint64_t next_frame, float transition_seconds);
};
} // namespace midnafx::camera_fov
