#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <vector>

namespace midnafx::render::grass_reach {
using Point = std::array<float, 2>;
inline std::array<std::uint32_t, 2> bits(const Point& point) {
    return {std::bit_cast<std::uint32_t>(point[0]), std::bit_cast<std::uint32_t>(point[1])};
}
class Points {
    std::vector<Point> points;
public:
    template<class GetPoint> void rebuild(std::size_t count, GetPoint get) {
        points.resize(count);
        for (std::size_t i = 0; i < count; ++i) points[i] = get(i);
        // Integer ordering is well-defined even for NaNs and signed zero. Remove
        // only bit-identical pairs: the original float subtraction/hypot stays intact.
        std::sort(points.begin(), points.end(), [](const auto& a, const auto& b) { return bits(a) < bits(b); });
        points.erase(std::unique(points.begin(), points.end(), [](const auto& a, const auto& b) {
            return bits(a) == bits(b);
        }), points.end());
    }
    float furthest(float x, float z) const {
        float result = 2200;
        for (const auto& p : points) result = std::max(result, std::hypot(p[0] - x, p[1] - z) + 512);
        return result;
    }
    std::size_t size() const { return points.size(); }
    std::size_t bytes() const { return points.capacity() * sizeof(Point); }
};
class Cache {
    std::array<std::uint32_t, 2> eye{};
    std::uint32_t revision = 0;
    float result = 2200;
    bool valid = false;
public:
    void reset() { valid = false; }
    template<class Compute> float get(float x, float z, std::uint32_t next_revision, Compute compute) {
        const auto next_eye = bits({x, z});
        if (!valid || next_eye != eye || next_revision != revision) {
            result = compute(); eye = next_eye; revision = next_revision; valid = true;
        }
        return result;
    }
};
} // namespace midnafx::render::grass_reach
