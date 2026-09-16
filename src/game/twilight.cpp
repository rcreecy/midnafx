#include "twilight.hpp"

#include <algorithm>

namespace midnafx::twilight {
State classify(bool scene_ready, std::uint8_t world_dark) {
    if (!scene_ready)
        return State::Unavailable;
    if (world_dark == 1)
        return State::Active;
    if (world_dark == 2)
        return State::Spot;
    return State::Normal;
}

float advance(float current, State state, float seconds, float transition_seconds) {
    const float goal = state == State::Active ? 1.0f : 0.0f;
    if (transition_seconds <= 0.0f)
        return goal;
    const float step = std::clamp(seconds, 0.0f, 0.1f) / transition_seconds;
    return goal > current ? std::min(goal, current + step) : std::max(goal, current - step);
}

grade::Prepared blend(const grade::Prepared& general, const grade::Prepared& target, float weight) {
    if (weight <= 0.0f)
        return general;
    if (weight >= 1.0f)
        return target;
    grade::Prepared result = general;
    const auto mix = [weight](float from, float to) { return from + (to - from) * weight; };
    result.uniforms.gain_r = mix(general.uniforms.gain_r, target.uniforms.gain_r);
    result.uniforms.gain_g = mix(general.uniforms.gain_g, target.uniforms.gain_g);
    result.uniforms.gain_b = mix(general.uniforms.gain_b, target.uniforms.gain_b);
    result.uniforms.black_point = mix(general.uniforms.black_point, target.uniforms.black_point);
    result.uniforms.contrast = mix(general.uniforms.contrast, target.uniforms.contrast);
    result.uniforms.saturation = mix(general.uniforms.saturation, target.uniforms.saturation);
    result.uniforms.gamma_inverse =
        mix(general.uniforms.gamma_inverse, target.uniforms.gamma_inverse);
    result.uniforms.rolloff = mix(general.uniforms.rolloff, target.uniforms.rolloff);
    result.uniforms.detail_strength =
        mix(general.uniforms.detail_strength, target.uniforms.detail_strength);
    result.neutral = general.neutral && target.neutral;
    return result;
}

const char* label(State state) {
    switch (state) {
    case State::Unavailable:
        return "Scene unavailable";
    case State::Normal:
        return "Normal";
    case State::Spot:
        return "Twilight spot (excluded)";
    case State::Active:
        return "Active Twilight";
    }
    return "Unknown";
}
} // namespace midnafx::twilight
