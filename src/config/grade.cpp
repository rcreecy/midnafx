#include "grade.hpp"

#include <algorithm>
#include <cmath>

namespace midnafx::grade {
Prepared prepare(const Controls& controls) {
    constexpr std::array<float, Count> neutral{0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f};
    constexpr std::array<float, Count> minimum{-2.0f, 0.0f, 0.5f, 0.7f, 0.0f, 0.0f, -1.0f, -1.0f};
    constexpr std::array<float, Count> maximum{2.0f, 0.2f, 1.5f, 1.5f, 2.0f, 1.0f, 1.0f, 1.0f};
    std::array<float, Count> value{};
    bool all_neutral = true;
    for (unsigned i = 0; i < Count; ++i) {
        const auto candidate = controls.value[i];
        value[i] = controls.active[i] && std::isfinite(candidate)
                       ? std::clamp(candidate, minimum[i], maximum[i])
                       : neutral[i];
        all_neutral &= value[i] == neutral[i];
    }
    const float exposure = std::exp2(value[Exposure]);
    const float temperature = value[Temperature];
    const float tint = value[Tint];
    return {
        {exposure * (1.0f + 0.10f * temperature + 0.03f * tint), exposure * (1.0f - 0.06f * tint),
         exposure * (1.0f - 0.10f * temperature + 0.03f * tint), value[BlackPoint], value[Contrast],
         value[Saturation], 1.0f / value[Gamma], value[HighlightRolloff], 0.0f, 4.0f, 0u, 0u},
        all_neutral};
}
} // namespace midnafx::grade
