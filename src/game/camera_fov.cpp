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
} // namespace midnafx::camera_fov
