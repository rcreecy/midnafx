#include "render/grass_reach.hpp"
#include "render/grass_view.hpp"
#include <limits>
#include <random>
#include <cstdio>

using namespace midnafx::render::grass_reach;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "line %d\n", __LINE__); return 1; } } while (false)
static float reference(const std::vector<Point>& points, float x, float z) {
    float value = 2200;
    for (const auto& p : points) value = std::max(value, std::hypot(p[0] - x, p[1] - z) + 512);
    return value;
}
static bool equal(float a, float b) { return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b); }
int main() {
    std::mt19937 rng(6411);
    std::vector<Point> source;
    for (unsigned i = 0; i < 4000; ++i) {
        const Point point{float(int(rng()%20000000)-10000000), float(int(rng()%20000000)-10000000)};
        for (unsigned copy = 0; copy < 4; ++copy) source.push_back(point);
    }
    Points points;
    points.rebuild(source.size(), [&](auto i) { return source[i]; });
    CHECK(points.size() == source.size()/4);
    for (unsigned i = 0; i < 2000; ++i) {
        const float x = float(int(rng()%20000000)-10000000), z = float(int(rng()%20000000)-10000000);
        const float expected = reference(source, x, z), actual = points.furthest(x, z);
        CHECK(equal(expected, actual));
        for (float fov : {30.0f, 60.0f, 120.0f})
            CHECK(midnafx::render::grass_view::tile_size(midnafx::render::grass_view::reach(2000000, expected, fov, 1.8f)) ==
                  midnafx::render::grass_view::tile_size(midnafx::render::grass_view::reach(2000000, actual, fov, 1.8f)));
    }
    // Arbitrary IEEE payloads and edge distances retain the original reduction.
    source.clear();
    for (unsigned i = 0; i < 2000; ++i)
        source.push_back({std::bit_cast<float>(std::uint32_t(rng())), std::bit_cast<float>(std::uint32_t(rng()))});
    source.push_back({0.0f, -0.0f}); source.push_back({-0.0f, 0.0f});
    source.push_back({std::numeric_limits<float>::infinity(), 0});
    source.push_back({std::numeric_limits<float>::quiet_NaN(), 0});
    points.rebuild(source.size(), [&](auto i) { return source[i]; });
    for (float x : {0.0f, -0.0f, 2200.0f, -10000000.0f, std::numeric_limits<float>::max()})
        CHECK(equal(reference(source, x, 1), points.furthest(x, 1)));
    Cache cache;
    unsigned calls = 0;
    auto compute = [&] { ++calls; return 9000.0f; };
    CHECK(cache.get(1, 2, 1, compute) == 9000);
    CHECK(cache.get(1, 2, 1, compute) == 9000 && calls == 1);
    cache.get(1, 2, 2, compute); CHECK(calls == 2);
    cache.get(std::nextafter(1.0f, 2.0f), 2, 2, compute); CHECK(calls == 3);
    cache.reset(); cache.get(std::nextafter(1.0f, 2.0f), 2, 2, compute); CHECK(calls == 4);
    points.rebuild(0, [](auto) { return Point{}; });
    CHECK(points.size() == 0 && points.furthest(1, 2) == 2200);
    points.rebuild(1, [](auto) { return Point{10000, 20000}; });
    CHECK(equal(points.furthest(1, 2), reference({{10000, 20000}}, 1, 2)));
}
