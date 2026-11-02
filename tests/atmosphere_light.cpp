#include "render/atmosphere_light_state.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <limits>

#define CHECK(condition)                                                                            \
    do {                                                                                            \
        if (!(condition)) {                                                                         \
            std::fprintf(stderr, "Atmosphere light check failed at line %d: %s\n", __LINE__, #condition); \
            return __LINE__;                                                                        \
        }                                                                                           \
    } while (false)

int main() {
    using namespace midnafx::render::atmosphere_light;
    std::array<float, 16> view{}, projection{};
    view[0] = view[5] = view[10] = view[15] = 1;
    projection[0] = projection[5] = 1;
    projection[10] = projection[14] = -100.0f / 99;
    projection[11] = -1;
    const auto identity = view, perspective = projection;
    const std::array<float, 3> world{0, 0, -8000}, color{1, 0.5f, 0.25f};
    const auto empty = [](const Source& source) {
        return source.direction == std::array<float, 4>{} && source.color == std::array<float, 4>{};
    };
    const auto close = [](float a, float b) { return std::abs(a - b) < 0.000001f; };
    const auto sample = [&](const auto& v, const auto& p, const auto& position,
                            const auto& rgb, float alpha = 1, bool active = true) {
        return prepare(v.data(), p.data(), position.data(), rgb.data(), alpha, active);
    };
    static_assert(noexcept(prepare(nullptr, nullptr, nullptr, nullptr, 0)));
    const auto source = sample(view, projection, world, color);
    CHECK((source.direction == std::array<float, 4>{0, 0, -1, 0.5f}));
    CHECK((source.color == std::array<float, 4>{1, 0.5f, 0.25f, 0}));
    // All six principal directions are valid: no screen bounds or forward-Z test.
    for (unsigned axis = 0; axis < 3; ++axis)
        for (float sign : {-1.0f, 1.0f}) {
            std::array<float, 3> position{}; position[axis] = sign * 8000;
            const auto result = sample(view, projection, position, color);
            CHECK(result.direction[axis] == sign && result.direction[3] == 0.5f);
            for (unsigned other = 0; other < 3; ++other)
                if (other != axis) CHECK(result.direction[other] == 0);
        }
    const std::array<float, 3> offscreen{8000, 3000, -1000};
    const auto offscreen_source = sample(view, projection, offscreen, color);
    const float offscreen_length = std::sqrt(74000000.0f);
    CHECK(close(offscreen_source.direction[0], 8000 / offscreen_length));
    CHECK(close(offscreen_source.direction[1], 3000 / offscreen_length));
    CHECK(close(offscreen_source.direction[2], -1000 / offscreen_length));
    for (float scale : {0.01f, 100.0f, 100000.0f}) {
        auto position = offscreen;
        for (auto& component : position) component *= scale;
        const auto result = sample(view, projection, position, color);
        for (unsigned i = 0; i < 4; ++i) CHECK(close(result.direction[i], offscreen_source.direction[i]));
    }
    view = identity; view[0] = 0; view[1] = 1; view[4] = -1; view[5] = 0;
    view[12] = 10; view[13] = 20; view[14] = 30;
    const auto transformed = sample(view, projection, std::array<float, 3>{2,3,-4}, color);
    const float transformed_length = std::sqrt(1209.0f); // View point is (7,22,26).
    CHECK(close(transformed.direction[0], 7 / transformed_length));
    CHECK(close(transformed.direction[1], 22 / transformed_length));
    CHECK(close(transformed.direction[2], 26 / transformed_length));
    view = identity; view[12] = -100; view[13] = -200; view[14] = -300;
    const auto moved_camera = sample(view, projection, std::array<float, 3>{100,200,-7700}, color);
    CHECK(moved_camera.direction == source.direction);
    CHECK(empty(sample(view, projection, std::array<float, 3>{100,200,300}, color)));
    view = identity;
    CHECK(empty(sample(view, projection, std::array<float, 3>{0,0,0}, color)));
    CHECK(empty(sample(view, projection, std::array<float, 3>{0,0,-0.00001f}, color)));
    CHECK(!empty(sample(view, projection, std::array<float, 3>{0,0,-0.0002f}, color)));
    const auto faded = sample(view, projection, world, color, 0.25f);
    CHECK(faded.direction[3] == 0.125f && faded.color == source.color);
    for (unsigned i = 0; i < 3; ++i) CHECK(faded.direction[i] == source.direction[i]);
    CHECK(sample(view, projection, world, color, 100).direction[3] == 0.5f);
    CHECK(empty(sample(view, projection, world, color, 1, false)));
    for (float alpha : {0.0f, -1.0f, std::numeric_limits<float>::quiet_NaN(),
                        std::numeric_limits<float>::infinity()})
        CHECK(empty(sample(view, projection, world, color, alpha)));
    CHECK(empty(prepare(nullptr, projection.data(), world.data(), color.data(), 1)));
    CHECK(empty(prepare(view.data(), nullptr, world.data(), color.data(), 1)));
    CHECK(empty(prepare(view.data(), projection.data(), nullptr, color.data(), 1)));
    CHECK(empty(prepare(view.data(), projection.data(), world.data(), nullptr, 1)));
    CHECK(empty(sample(view, projection, world, std::array<float, 3>{-1,0,-2})));
    CHECK(empty(sample(view, projection, world, std::array<float, 3>{0,0,0})));
    const auto clamped = sample(view, projection, world, std::array<float, 3>{-2,2,0.25f});
    CHECK((clamped.color == std::array<float, 4>{0,1,0.25f,0}));
    for (unsigned component = 0; component < 3; ++component)
        for (float bad : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
            auto position = world; position[component] = bad;
            CHECK(empty(sample(view, projection, position, color)));
            auto rgb = color; rgb[component] = bad;
            CHECK(empty(sample(view, projection, world, rgb)));
        }
    for (unsigned component = 0; component < 16; ++component)
        for (float bad : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
            view = identity; view[component] = bad;
            CHECK(empty(sample(view, perspective, world, color)));
            projection = perspective; projection[component] = bad;
            CHECK(empty(sample(identity, projection, world, color)));
        }
    view = identity; projection = perspective;
    for (unsigned component : {3u, 7u, 11u}) {
        view = identity; view[component] = 0.000001f;
        CHECK(empty(sample(view, projection, world, color)));
    }
    view = identity; view[15] = 0;
    CHECK(empty(sample(view, projection, world, color)));
    view = identity; view[0] = 2;
    CHECK(empty(sample(view, projection, world, color)));
    view = identity; view[4] = 0.25f;
    CHECK(empty(sample(view, projection, world, color)));
    view = identity; view[0] = -1;
    CHECK(empty(sample(view, projection, world, color)));
    view = identity; view[0] = 0.6f; view[1] = 0.8f; view[4] = -0.8f; view[5] = 0.6f;
    CHECK(!empty(sample(view, projection, world, color)));
    for (unsigned component : {0u, 5u})
        for (float bad : {0.0f, -1.0f}) {
            projection = perspective; projection[component] = bad;
            CHECK(empty(sample(identity, projection, world, color)));
        }
    for (unsigned component : {3u, 7u, 15u}) {
        projection = perspective; projection[component] = 0.001f;
        CHECK(empty(sample(identity, projection, world, color)));
    }
    projection = perspective; projection[11] = 0;
    CHECK(empty(sample(identity, projection, world, color)));
    projection = perspective; projection[10] = 1.0f / 99; projection[14] = 100.0f / 99;
    CHECK(sample(identity, projection, world, color).direction == source.direction);
    projection = perspective; projection[8] = 2; projection[9] = -2;
    CHECK(sample(identity, projection, world, color).direction == source.direction);
    const float maximum = std::numeric_limits<float>::max();
    const auto huge = sample(identity, perspective, std::array<float, 3>{maximum,maximum,maximum}, color);
    CHECK(!empty(huge));
    for (unsigned i = 0; i < 3; ++i) CHECK(close(huge.direction[i], 1 / std::sqrt(3.0f)));
    std::puts("Atmosphere light: full-sphere direction, rigid view, projection guards, fade and color: PASS");
}
