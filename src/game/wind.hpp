#pragma once

#include <d/d_kankyo.h>
#include <SSystem/SComponent/c_counter.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace midnafx::wind {

// XZ direction, amplitude, and the shared simulation clock.
inline std::array<float, 4> sample() {
    const auto& native = g_env_light.global_wind_influence;
    const float magnitude = std::hypot(native.vec.x, native.vec.z);
    const bool direction_valid = std::isfinite(magnitude) && magnitude > 0.001f;
    const float power = std::isfinite(native.pow) ? std::clamp(native.pow, 0.0f, 1.0f) : 0.0f;
    const bool outdoor = dKy_Outdoor_check();
    return {direction_valid ? native.vec.x / magnitude : 1.0f,
            direction_valid ? native.vec.z / magnitude : 0.0f,
            outdoor ? 0.24f + 0.76f * power : 0.32f * power,
            static_cast<float>(g_Counter.mCounter0) / 30.0f};
}

} // namespace midnafx::wind
