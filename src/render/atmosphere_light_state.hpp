#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace midnafx::render::atmosphere_light {
struct Source {
    std::array<float, 4> direction{}, color{};
};

inline Source prepare(const float* view, const float* projection, const float* world,
                      const float* color, float alpha, bool active = true) noexcept {
    Source result;
    if (!active || !view || !projection || !world || !color ||
        !std::isfinite(alpha) || alpha <= 0) return result;
    for (unsigned i = 0; i < 16; ++i)
        if (!std::isfinite(view[i]) || !std::isfinite(projection[i])) return result;
    for (unsigned i = 0; i < 3; ++i)
        if (!std::isfinite(world[i]) || !std::isfinite(color[i])) return result;
    if (view[3] != 0 || view[7] != 0 || view[11] != 0 || view[15] != 1) return result;
    constexpr double rigid_tolerance = 0.001;
    for (unsigned a = 0; a < 3; ++a)
        for (unsigned b = a; b < 3; ++b) {
            double product = 0;
            for (unsigned row = 0; row < 3; ++row)
                product += double(view[a * 4 + row]) * view[b * 4 + row];
            if (std::abs(product - (a == b ? 1.0 : 0.0)) > rigid_tolerance) return result;
        }
    const double determinant =
        double(view[0]) * (double(view[5]) * view[10] - double(view[9]) * view[6]) -
        double(view[4]) * (double(view[1]) * view[10] - double(view[9]) * view[2]) +
        double(view[8]) * (double(view[1]) * view[6] - double(view[5]) * view[2]);
    if (std::abs(determinant - 1.0) > rigid_tolerance) return result;
    if (projection[0] <= 0 || projection[5] <= 0 ||
        std::abs(projection[11] + 1) > 0.0001f || std::abs(projection[15]) > 0.0001f ||
        std::abs(projection[3]) > 0.0001f || std::abs(projection[7]) > 0.0001f) return result;

    std::array<double, 3> point{};
    double distance_squared = 0;
    for (unsigned row = 0; row < 3; ++row) {
        point[row] = view[12 + row];
        for (unsigned column = 0; column < 3; ++column)
            point[row] += double(view[column * 4 + row]) * world[column];
        if (!std::isfinite(point[row])) return result;
        distance_squared += point[row] * point[row];
    }
    const double distance = std::sqrt(distance_squared);
    if (!std::isfinite(distance) || distance <= 0.0001) return result;
    for (unsigned i = 0; i < 3; ++i) {
        result.direction[i] = static_cast<float>(point[i] / distance);
        result.color[i] = std::clamp(color[i], 0.0f, 1.0f);
    }
    if (result.color[0] == 0 && result.color[1] == 0 && result.color[2] == 0) return {};
    // A direction remains meaningful outside the viewport and behind the eye.
    // No projected W, depth range, or screen-coordinate gate belongs here.
    result.direction[3] = 0.5f * std::clamp(alpha, 0.0f, 1.0f);
    return result;
}
} // namespace midnafx::render::atmosphere_light
