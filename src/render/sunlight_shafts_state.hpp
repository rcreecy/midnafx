#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>

namespace midnafx::render::sunlight_shafts {
struct Source {
    std::array<float, 4> screen{}; // UV, effective strength, viewport aspect.
    std::array<float, 4> color{};
};

inline Source prepare(const float* view, const float* projection, const float* world,
                      const float* color, float strength, float alpha, float visibility,
                      unsigned width, unsigned height) {
    Source result;
    if (!view || !projection || !world || !color || !width || !height ||
        !std::isfinite(strength) || !std::isfinite(alpha) || !std::isfinite(visibility) ||
        strength <= 0 || alpha <= 0 || visibility <= 0) return result;
    for (unsigned i = 0; i < 16; ++i)
        if (!std::isfinite(view[i]) || !std::isfinite(projection[i])) return result;
    for (unsigned i = 0; i < 3; ++i)
        if (!std::isfinite(world[i]) || !std::isfinite(color[i])) return result;
    // The native camera uses a right-handed perspective with w = -view.z.
    if (std::abs(projection[11] + 1) > 0.0001f || std::abs(projection[15]) > 0.0001f ||
        std::abs(projection[3]) > 0.0001f || std::abs(projection[7]) > 0.0001f) return result;
    float point[4]{}, clip[4]{};
    for (unsigned row = 0; row < 4; ++row)
        point[row] = view[row] * world[0] + view[4 + row] * world[1] +
                     view[8 + row] * world[2] + view[12 + row];
    for (unsigned row = 0; row < 4; ++row)
        for (unsigned column = 0; column < 4; ++column)
            clip[row] += projection[column * 4 + row] * point[column];
    if (!std::all_of(std::begin(clip), std::end(clip), [](float v) { return std::isfinite(v); }) ||
        clip[3] <= 0.0001f) return result;
    const float u = 0.5f + 0.5f * clip[0] / clip[3];
    const float v = 0.5f - 0.5f * clip[1] / clip[3];
    // Sun sprites can sit beyond the geometry far plane. Only their direction
    // and positive perspective w are used; opaque depth supplies occlusion.
    if (u <= 0 || u >= 1 || v <= 0 || v >= 1) return result;
    result.screen = {u, v, std::clamp(strength, 0.0f, 0.25f) *
        std::clamp(alpha, 0.0f, 1.0f) * std::clamp(visibility, 0.0f, 1.0f), float(width) / height};
    for (unsigned i = 0; i < 3; ++i) result.color[i] = std::clamp(color[i], 0.0f, 1.0f);
    if (std::all_of(result.color.begin(), result.color.begin() + 3, [](float v) { return v == 0; })) return {};
    return result;
}
} // namespace midnafx::render::sunlight_shafts
