#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <span>

namespace midnafx::render::local_lights {
struct Source {
    std::array<float, 3> position{};
    // Raw signed native color channels, e.g. {188,102,66}, not normalized RGB.
    std::array<float, 3> color{};
    float radius = 0;
    int index = 0;
    // Zero retains ordinary radius falloff; {1,0,K2,1} selects native steep attenuation.
    std::array<float, 4> distance{};
};
struct Light {
    std::array<float, 4> position_radius{};
    std::array<float, 4> color{};
    std::array<float, 4> distance{};
};
struct Prepared {
    std::array<Light, 4> lights{};
    unsigned count = 0;
};

inline Source lantern_source(std::array<float, 3> position, std::array<float, 3> raw_color,
                             float reference_distance, unsigned active, unsigned alpha,
                             unsigned spot_mode, unsigned distance_mode) noexcept {
    if (active != 1 || alpha != 255 || spot_mode != 0 || distance_mode != 3 ||
        !std::isfinite(reference_distance) || reference_distance <= 0) return {};
    // Match native GX steep attenuation at its actual reference brightness.
    constexpr float brightness = 0.99999f;
    const float coefficient = (1 - brightness) /
                              (reference_distance * (brightness * reference_distance));
    if (!std::isfinite(coefficient) || coefficient <= 0) return {};
    // This is a separate tracing envelope at one-percent native response,
    // capped for bounded work. It is not the native reference distance.
    const float radius = static_cast<float>(std::min(6000.0, std::sqrt(99.0 / coefficient)));
    return {position, raw_color, radius, 101, {1, 0, coefficient, 1}};
}

inline Prepared prepare(std::span<const Source> sources,
                        std::span<const float, 16> view_from_world) noexcept {
    Prepared result;
    const auto& view = view_from_world;
    for (const auto value : view)
        if (!std::isfinite(value)) return result;
    if (view[3] != 0 || view[7] != 0 || view[11] != 0 || view[15] != 1) return result;
    // Camera transforms preserve radii: reject scale, shear, and reflections.
    constexpr double tolerance = 0.001;
    for (unsigned a = 0; a < 3; ++a)
        for (unsigned b = a; b < 3; ++b) {
            double product = 0;
            for (unsigned row = 0; row < 3; ++row)
                product += double(view[a * 4 + row]) * view[b * 4 + row];
            if (std::abs(product - (a == b ? 1.0 : 0.0)) > tolerance) return result;
        }
    const double determinant =
        double(view[0]) * (double(view[5]) * view[10] - double(view[9]) * view[6]) -
        double(view[4]) * (double(view[1]) * view[10] - double(view[9]) * view[2]) +
        double(view[8]) * (double(view[1]) * view[6] - double(view[5]) * view[2]);
    if (std::abs(determinant - 1.0) > tolerance) return result;

    std::array<double, 4> scores{};
    for (const auto& source : sources) {
        if (source.index <= 0 || (source.index & 0x8000) != 0 ||
            !std::isfinite(source.radius) || source.radius <= 0.01f || source.radius > 6000)
            continue;
        if (source.distance != std::array<float, 4>{} &&
            (source.distance[0] != 1 || source.distance[1] != 0 ||
             !std::isfinite(source.distance[2]) || source.distance[2] <= 0 ||
             source.distance[3] != 1)) continue;
        bool valid = true;
        Light light;
        light.distance = source.distance;
        double brightest = 0;
        for (unsigned component = 0; component < 3; ++component) {
            if (!std::isfinite(source.position[component]) ||
                std::abs(double(source.position[component])) > 1e7 ||
                !std::isfinite(source.color[component]) || source.color[component] < 0) {
                valid = false;
                break;
            }
            light.color[component] = std::min(source.color[component] / 255.0f, 1.0f);
            brightest = std::max(brightest, double(light.color[component]));
        }
        if (!valid || brightest <= 0.000001) continue;
        double distance_squared = 0;
        for (unsigned row = 0; row < 3; ++row) {
            double position = view[12 + row];
            for (unsigned column = 0; column < 3; ++column)
                position += double(view[column * 4 + row]) * source.position[column];
            if (!std::isfinite(position) || std::abs(position) > 1e7) {
                valid = false;
                break;
            }
            light.position_radius[row] = static_cast<float>(position);
            distance_squared += position * position;
        }
        if (!valid) continue;
        light.position_radius[3] = source.radius;
        const double radius_squared = double(source.radius) * source.radius;
        const double score = brightest * radius_squared / (radius_squared + distance_squared);
        unsigned insertion = 0;
        // Equal scores remain in source order, including at the fourth slot.
        while (insertion < result.count && score <= scores[insertion]) ++insertion;
        if (insertion >= result.lights.size()) continue;
        const unsigned count = std::min(result.count + 1, unsigned(result.lights.size()));
        for (unsigned slot = count - 1; slot > insertion; --slot) {
            result.lights[slot] = result.lights[slot - 1];
            scores[slot] = scores[slot - 1];
        }
        result.lights[insertion] = light;
        scores[insertion] = score;
        result.count = count;
    }
    return result;
}
} // namespace midnafx::render::local_lights
