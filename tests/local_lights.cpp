#include "render/local_light_state.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <span>

#define CHECK(condition)                                                                            \
    do {                                                                                            \
        if (!(condition)) {                                                                         \
            std::fprintf(stderr, "Local light check failed at line %d: %s\n", __LINE__, #condition); \
            return __LINE__;                                                                        \
        }                                                                                           \
    } while (false)

int main() {
    using namespace midnafx::render::local_lights;
    std::array<float, 16> identity{};
    identity[0] = identity[5] = identity[10] = identity[15] = 1;
    const Source ordinary{{0, 0, -10}, {255, 128, 64}, 100, 1};
    const auto sample = [&](const Source& source, const std::array<float, 16>& view) {
        return prepare(std::array{source}, view);
    };
    const auto empty = [](const Prepared& result) {
        if (result.count != 0) return false;
        for (const auto& light : result.lights)
            if (light.position_radius != std::array<float, 4>{} || light.color != std::array<float, 4>{} ||
                light.distance != std::array<float, 4>{})
                return false;
        return true;
    };
    const auto close = [](float a, float b) { return std::abs(a - b) < 0.000001f; };
    static_assert(noexcept(prepare(std::span<const Source>{}, std::span<const float, 16>(identity))));
    CHECK(empty(prepare(std::span<const Source>{}, identity)));
    const auto first = sample(ordinary, identity);
    CHECK(first.count == 1);
    CHECK((first.lights[0].position_radius == std::array<float, 4>{0, 0, -10, 100}));
    CHECK(first.lights[0].color[0] == 1 && close(first.lights[0].color[1], 128.0f / 255));
    CHECK(close(first.lights[0].color[2], 64.0f / 255) && first.lights[0].color[3] == 0);
    CHECK((ordinary.distance == std::array<float, 4>{}));
    CHECK((first.lights[0].distance == std::array<float, 4>{}));

    auto view = identity;
    view[0] = 0; view[1] = 1; view[4] = -1; view[5] = 0;
    view[12] = 10; view[13] = 20; view[14] = 30;
    auto source = ordinary; source.position = {2, 3, -4}; source.radius = 50;
    const auto transformed = sample(source, view);
    CHECK(transformed.count == 1);
    CHECK((transformed.lights[0].position_radius == std::array<float, 4>{7, 22, 26, 50}));
    view = identity; view[0] = 0.6f; view[1] = 0.8f; view[4] = -0.8f; view[5] = 0.6f;
    CHECK(sample(ordinary, view).count == 1);
    for (unsigned component = 0; component < 16; ++component)
        for (const float invalid : {std::numeric_limits<float>::quiet_NaN(),
                                    std::numeric_limits<float>::infinity(),
                                    -std::numeric_limits<float>::infinity()}) {
            view = identity; view[component] = invalid;
            CHECK(empty(sample(ordinary, view)));
        }
    for (unsigned component : {3u, 7u, 11u}) {
        view = identity; view[component] = 0.0000001f;
        CHECK(empty(sample(ordinary, view)));
    }
    view = identity; view[15] = 0;
    CHECK(empty(sample(ordinary, view)));
    view = identity; view[0] = 2;
    CHECK(empty(sample(ordinary, view)));
    view = identity; view[4] = 0.25f;
    CHECK(empty(sample(ordinary, view)));
    view = identity; view[0] = -1;
    CHECK(empty(sample(ordinary, view)));
    CHECK(empty(sample(ordinary, std::array<float, 16>{})));

    for (const float radius : {0.0f, -1.0f, 0.01f, 6000.5f,
                               std::numeric_limits<float>::quiet_NaN(),
                               std::numeric_limits<float>::infinity(),
                               std::numeric_limits<float>::max()}) {
        source = ordinary; source.radius = radius;
        CHECK(empty(sample(source, identity)));
    }
    for (const float radius : {std::nextafter(0.01f, 1.0f), 6000.0f}) {
        source = ordinary; source.radius = radius;
        CHECK(sample(source, identity).count == 1);
    }
    for (const int index : {0, -1, 0x8000, 0x8001, std::numeric_limits<int>::min(),
                           std::numeric_limits<int>::max()}) {
        source = ordinary; source.index = index;
        CHECK(empty(sample(source, identity)));
    }
    for (const int index : {1, 50, 51, 0x10000}) {
        source = ordinary; source.index = index;
        CHECK(sample(source, identity).count == 1);
    }
    for (unsigned component = 0; component < 3; ++component) {
        for (const float invalid : {-0.01f, std::numeric_limits<float>::quiet_NaN(),
                                    std::numeric_limits<float>::infinity()}) {
            source = ordinary; source.color[component] = invalid;
            CHECK(empty(sample(source, identity)));
        }
        for (const float invalid : {10000001.0f, -10000001.0f,
                                    std::numeric_limits<float>::quiet_NaN(),
                                    std::numeric_limits<float>::infinity(),
                                    std::numeric_limits<float>::max()}) {
            source = ordinary; source.position[component] = invalid;
            CHECK(empty(sample(source, identity)));
        }
    }
    source = ordinary; source.color = {0, 0, 0};
    CHECK(empty(sample(source, identity)));
    source.color = {0.00001f, 0.00001f, 0.00001f};
    CHECK(empty(sample(source, identity)));
    source.color = {std::numeric_limits<float>::max(), 255, 127.5f};
    const auto clamped = sample(source, identity);
    CHECK(clamped.count == 1);
    CHECK((clamped.lights[0].color == std::array<float, 4>{1, 1, 0.5f, 0}));
    source = ordinary; source.position = {1e7f, -1e7f, 1e7f}; source.radius = 6000;
    CHECK(sample(source, identity).count == 1);
    view = identity; view[12] = 1e7f;
    CHECK(empty(sample(source, view))); // Transformed coordinates are bounded as well.
    view = identity; view[12] = std::numeric_limits<float>::max();
    CHECK(empty(sample(source, view))); // Safe rejection without overflowing float accumulation.

    std::array<Source, 7> ranked{};
    const std::array<float, 7> distances{40, 5, 20, 1, 30, 10, 2};
    for (unsigned i = 0; i < ranked.size(); ++i)
        ranked[i] = {{0, 0, -distances[i]}, {255, 255, 255}, 10, int(i + 1)};
    const auto nearest = prepare(ranked, identity);
    CHECK(nearest.count == 4);
    const std::array<float, 4> expected_distances{1, 2, 5, 10};
    for (unsigned i = 0; i < nearest.count; ++i)
        CHECK(nearest.lights[i].position_radius[2] == -expected_distances[i]);
    for (const auto& light : nearest.lights)
        CHECK((light.distance == std::array<float, 4>{}));
    std::array<Source, 5> ties{};
    for (unsigned i = 0; i < ties.size(); ++i) {
        const float distance = float((i + 1) * 10);
        ties[i] = {{0, 0, -distance}, {255, 255, 255}, distance, int(i + 1)};
    }
    const auto stable = prepare(ties, identity);
    CHECK(stable.count == 4);
    for (unsigned i = 0; i < stable.count; ++i)
        CHECK(stable.lights[i].position_radius[2] == -float((i + 1) * 10));
    const std::array<Source, 2> coverage{{
        {{0, 0, -1}, {255, 255, 255}, 1, 1},
        {{0, 0, -20}, {255, 255, 255}, 100, 2}}};
    CHECK(prepare(coverage, identity).lights[0].position_radius[2] == -20);
    const std::array<Source, 2> brightness{{
        {{0, 0, -1}, {25.5f, 25.5f, 25.5f}, 10, 1},
        {{0, 0, -20}, {255, 0, 0}, 10, 2}}};
    CHECK(prepare(brightness, identity).lights[0].position_radius[2] == -20);
    const std::array<Source, 2> maximum_channel{{
        {{0, 0, -20}, {128, 128, 128}, 20, 1},
        {{0, 0, -10}, {255, 0, 0}, 10, 2}}};
    CHECK(prepare(maximum_channel, identity).lights[0].position_radius[2] == -10);
    for (unsigned count = 0; count < 4; ++count) {
        const auto result = prepare(std::span<const Source>(ties).first(count), identity);
        CHECK(result.count == count);
        for (unsigned i = count; i < result.lights.size(); ++i) {
            CHECK((result.lights[i].position_radius == std::array<float, 4>{}));
            CHECK((result.lights[i].color == std::array<float, 4>{}));
            CHECK((result.lights[i].distance == std::array<float, 4>{}));
        }
    }
    CHECK(empty(prepare({}, identity))); // A later empty call carries no previous selection.

    const std::array<float, 3> lantern_position{2, 3, -4}, lantern_color{181, 112, 40};
    const auto lantern = lantern_source(lantern_position, lantern_color, 1, 1, 255, 0, 3);
    static_assert(noexcept(lantern_source({}, {}, 1, 1, 255, 0, 3)));
    CHECK(lantern.index == 101 && lantern.position == lantern_position && lantern.color == lantern_color);
    CHECK(lantern.radius > 3130 && lantern.radius < 3160 && lantern.radius != 1);
    CHECK(lantern.distance[0] == 1 && lantern.distance[1] == 0 && lantern.distance[3] == 1);
    const auto attenuation = [](const Source& value, double distance) {
        return 1.0 / (value.distance[0] + value.distance[1] * distance +
                      value.distance[2] * distance * distance);
    };
    CHECK(std::abs(attenuation(lantern, 1) - double(0.99999f)) < 0.0000001);
    CHECK(std::abs(attenuation(lantern, 316) - 0.5) < 0.002);
    CHECK(std::abs(attenuation(lantern, lantern.radius) - 0.01) < 0.000001);
    const auto lantern_prepared = sample(lantern, identity);
    CHECK(lantern_prepared.count == 1 && lantern_prepared.lights[0].distance == lantern.distance);
    CHECK(close(lantern_prepared.lights[0].color[0], 181.0f / 255));
    CHECK(lantern_prepared.lights[0].position_radius[3] == lantern.radius);
    view = identity; view[0] = 0; view[1] = 1; view[4] = -1; view[5] = 0;
    view[12] = 10; view[13] = 20; view[14] = 30;
    const auto lantern_rotated = sample(lantern, view);
    CHECK(lantern_rotated.count == 1 && lantern_rotated.lights[0].distance == lantern.distance);
    CHECK(lantern_rotated.lights[0].position_radius[0] == 7 &&
          lantern_rotated.lights[0].position_radius[1] == 22 &&
          lantern_rotated.lights[0].position_radius[2] == 26 &&
          lantern_rotated.lights[0].position_radius[3] == lantern.radius);
    for (const float reference : {0.5f, 0.25f}) {
        const auto faded = lantern_source(lantern_position, lantern_color, reference, 1, 255, 0, 3);
        CHECK(sample(faded, identity).count == 1);
        CHECK(std::abs(faded.radius / lantern.radius - reference) < 0.000001f);
        CHECK(std::abs(faded.distance[2] / lantern.distance[2] - 1 / (reference * reference)) < 0.00001f);
        CHECK(attenuation(faded, 316) < attenuation(lantern, 316));
    }
    const auto capped_lantern = lantern_source(lantern_position, lantern_color, 4, 1, 255, 0, 3);
    CHECK(capped_lantern.radius == 6000 && sample(capped_lantern, identity).count == 1);
    const auto tiny_lantern = lantern_source(lantern_position, lantern_color, 0.000001f, 1, 255, 0, 3);
    CHECK(tiny_lantern.radius <= 0.01f && empty(sample(tiny_lantern, identity)));
    const auto empty_source = [](const Source& value) {
        return value.position == std::array<float, 3>{} && value.color == std::array<float, 3>{} &&
               value.radius == 0 && value.index == 0 && value.distance == std::array<float, 4>{};
    };
    for (const float reference : {0.0f, -1.0f, std::numeric_limits<float>::quiet_NaN(),
                                  std::numeric_limits<float>::infinity(),
                                  std::numeric_limits<float>::max(),
                                  std::numeric_limits<float>::min()})
        CHECK(empty_source(lantern_source(lantern_position, lantern_color, reference, 1, 255, 0, 3)));
    for (unsigned active : {0u, 2u})
        CHECK(empty_source(lantern_source(lantern_position, lantern_color, 1, active, 255, 0, 3)));
    for (unsigned alpha : {0u, 253u, 254u, 256u})
        CHECK(empty_source(lantern_source(lantern_position, lantern_color, 1, 1, alpha, 0, 3)));
    for (unsigned spot : {1u, 2u, 3u, 4u, 5u, 6u})
        CHECK(empty_source(lantern_source(lantern_position, lantern_color, 1, 1, 255, spot, 3)));
    for (unsigned distance : {0u, 1u, 2u, 4u})
        CHECK(empty_source(lantern_source(lantern_position, lantern_color, 1, 1, 255, 0, distance)));
    CHECK(empty(sample(lantern_source({1e8f, 0, 0}, lantern_color, 1, 1, 255, 0, 3), identity)));
    CHECK(empty(sample(lantern_source(lantern_position, {-1, 112, 40}, 1, 1, 255, 0, 3), identity)));
    for (const auto invalid : {std::array<float, 4>{0, 0, 1, 1}, {1, 1, 1, 1},
                               {1, 0, 0, 1}, {1, 0, -1, 1}, {1, 0, 1, 0}, {1, 0, 1, 2}}) {
        source = ordinary; source.distance = invalid;
        CHECK(empty(sample(source, identity)));
    }
    for (unsigned component = 0; component < 4; ++component)
        for (const float invalid : {std::numeric_limits<float>::quiet_NaN(),
                                    std::numeric_limits<float>::infinity()}) {
            source = lantern; source.distance[component] = invalid;
            CHECK(empty(sample(source, identity)));
        }
    const std::array<Source, 2> mixed{lantern, ordinary};
    const auto selected = prepare(mixed, identity);
    CHECK(selected.count == 2 && selected.lights[1].distance == lantern.distance);
    CHECK((selected.lights[0].distance == std::array<float, 4>{}));
    CHECK(empty(prepare({}, identity))); // No stale lantern coefficients after source retirement.
    std::puts("Local lights: native color, rigid transforms, bounded selection, and lantern attenuation: PASS");
}
