#include "camera_fov.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace midnafx::camera_fov {
float scale_vertical(float native_degrees, float scale) {
    if (!std::isfinite(native_degrees) || !std::isfinite(scale) || native_degrees <= 0.0f ||
        native_degrees >= 180.0f || scale <= 0.0f)
        return native_degrees;
    const float radians = native_degrees * std::numbers::pi_v<float> / 180.0f;
    return 2.0f * std::atan(std::tan(radians * 0.5f) * scale) * 180.0f /
           std::numbers::pi_v<float>;
}

float advance(float current, float target, std::uint64_t tick_delta, float transition_seconds) {
    current = std::clamp(current, 0.0f, 1.0f);
    target = std::clamp(target, 0.0f, 1.0f);
    if (transition_seconds <= 0.0f)
        return target;
    const float step = static_cast<float>(std::min<std::uint64_t>(tick_delta, 4u)) /
                       (30.0f * transition_seconds);
    return target > current ? std::min(target, current + step)
                            : std::max(target, current - step);
}
float movement_target(float speed, float max_speed) {
    if (!std::isfinite(speed) || !std::isfinite(max_speed) || max_speed <= 0)
        return 0;
    const float fraction = std::clamp(std::abs(speed) / max_speed, 0.0f, 1.0f);
    const float t = std::clamp((fraction - 0.35f) / 0.55f, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
void MovementFraming::reset() { *this = {}; }
float MovementFraming::update(float speed, float max_speed, bool active,
                              std::uint64_t next_ticks, std::uint64_t next_frame,
                              float transition_seconds) {
    if (!active || !std::isfinite(speed) || !std::isfinite(max_speed) || max_speed <= 0 ||
        !std::isfinite(transition_seconds) || transition_seconds < 0) {
        reset();
        return 0;
    }
    if (sampled && (next_ticks < ticks || next_frame < frame || next_ticks - ticks > 4))
        reset();
    if (sampled && (next_ticks == ticks || next_frame == frame))
        return amount;
    const auto delta = sampled ? next_ticks - ticks : 1u;
    ticks = next_ticks; frame = next_frame; sampled = true;
    const float target = movement_target(speed, max_speed);
    // Settle more slowly than expansion to avoid pumping on short input gaps.
    const float duration = transition_seconds * (target < amount ? 2.0f : 1.0f);
    amount = advance(amount, target, delta, duration);
    return amount;
}
} // namespace midnafx::camera_fov
