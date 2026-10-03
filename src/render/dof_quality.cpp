#include "dof_quality.hpp"

#include <algorithm>
#include <cmath>

namespace midnafx::render::dof_quality {
float advance_focus(float current, float target, float elapsed_seconds,
                    float transition_seconds) {
    if (!std::isfinite(target) || target <= 0.0f)
        return current;
    if (!std::isfinite(current) || current <= 0.0f || !std::isfinite(transition_seconds) ||
        transition_seconds <= 0.0f)
        return target;
    const float elapsed = std::clamp(elapsed_seconds, 0.0f, 0.25f);
    if (elapsed <= 0.0f)
        return current;
    const float weight = 1.0f - std::exp(-elapsed / transition_seconds);
    return current + (target - current) * weight;
}
} // namespace midnafx::render::dof_quality
