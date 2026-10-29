#include "render/sunlight_shafts_state.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <limits>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "Sunlight source check failed at line %d: %s\n", __LINE__, #condition); \
            return __LINE__;                                                                       \
        }                                                                                          \
    } while (false)

int main() {
    using namespace midnafx::render::sunlight_shafts;
    std::array<float,16> view{}, projection{};
    view[0] = view[5] = view[10] = view[15] = 1;
    projection[0] = projection[5] = 1;
    projection[10] = projection[14] = -100.0f / 99;
    projection[11] = -1;
    const std::array<float,3> world{2,1,-10}, color{1,0.5f,0.25f};
    const auto empty = [](const Source& source) {
        return source.screen == std::array<float,4>{} && source.color == std::array<float,4>{};
    };
    const auto close = [](float a, float b) { return std::abs(a - b) < 0.000001f; };
    const auto sample = [&](const std::array<float,16>& v, const std::array<float,16>& p,
                            const std::array<float,3>& w, const std::array<float,3>& c,
                            float strength = 0.1f, float alpha = 1, float visibility = 1,
                            unsigned width = 800, unsigned height = 600) {
        return prepare(v.data(), p.data(), w.data(), c.data(), strength, alpha, visibility, width, height);
    };
    auto source = sample(view, projection, world, color);
    CHECK(close(source.screen[0], 0.6f) && close(source.screen[1], 0.45f));
    CHECK(close(source.screen[2], 0.1f) && close(source.screen[3], 4.0f / 3));
    CHECK(source.color[0] == 1 && source.color[1] == 0.5f && source.color[2] == 0.25f);
    CHECK(source.color[3] == 0);
    const auto far = sample(view, projection, std::array<float,3>{200,100,-1000}, color);
    CHECK(far.screen == source.screen && far.color == source.color);
    auto shifted_projection = projection;
    shifted_projection[8] = 0.2f; shifted_projection[9] = -0.1f;
    source = sample(view, shifted_projection, std::array<float,3>{0,0,-10}, color);
    CHECK(close(source.screen[0], 0.4f) && close(source.screen[1], 0.45f));
    auto translated = view;
    translated[12] = -10; translated[13] = -1;
    source = sample(translated, projection, std::array<float,3>{12,1,-10}, color);
    CHECK(close(source.screen[0], 0.6f) && close(source.screen[1], 0.5f));
    auto rotated = view;
    rotated[0] = rotated[10] = 0; rotated[2] = -1; rotated[8] = 1;
    source = sample(rotated, projection, std::array<float,3>{10,0,0}, color);
    CHECK(close(source.screen[0], 0.5f) && close(source.screen[1], 0.5f));
    auto reversed = projection;
    reversed[10] = 1.0f / 99; reversed[14] = 100.0f / 99;
    CHECK(sample(view, reversed, world, color).screen == sample(view, projection, world, color).screen);

    for (const auto& position : {std::array<float,3>{0,0,10}, {0,0,0}, {0,0,-0.00005f},
                                 {-10,0,-10}, {10,0,-10}, {0,-10,-10}, {0,10,-10}, {20,0,-10}})
        CHECK(empty(sample(view, projection, position, color)));
    CHECK(!empty(sample(view, projection, std::array<float,3>{0,0,-0.001f}, color)));
    for (unsigned index : {3u, 7u, 11u, 15u}) {
        auto unsupported = projection;
        unsupported[index] = index == 11 ? 0 : 1;
        CHECK(empty(sample(view, unsupported, world, color)));
    }
    CHECK(empty(sample(view, projection, world, color, 0)));
    CHECK(empty(sample(view, projection, world, color, -0.1f)));
    CHECK(empty(sample(view, projection, world, color, 0.1f, 0)));
    CHECK(empty(sample(view, projection, world, color, 0.1f, 1, 0)));
    CHECK(empty(sample(view, projection, world, color, 0.1f, -1, 1)));
    CHECK(empty(sample(view, projection, world, color, 0.1f, 1, -1)));
    CHECK(close(sample(view, projection, world, color, 1, 2, 2).screen[2], 0.25f));
    CHECK(close(sample(view, projection, world, color, 0.2f, 0.5f, 0.25f).screen[2], 0.025f));
    CHECK(empty(sample(view, projection, world, color, 0.1f, 1, 1, 0, 600)));
    CHECK(empty(sample(view, projection, world, color, 0.1f, 1, 1, 800, 0)));
    CHECK(sample(view, projection, world, color, 0.1f, 1, 1, 1, 1).screen[3] == 1);
    source = sample(view, projection, world, std::array<float,3>{-1,0.5f,2});
    CHECK(source.color[0] == 0 && source.color[1] == 0.5f && source.color[2] == 1);
    CHECK(empty(sample(view, projection, world, std::array<float,3>{0,0,0})));
    CHECK(empty(sample(view, projection, world, std::array<float,3>{-1,0,-2})));

    for (float bad : {std::numeric_limits<float>::quiet_NaN(),
                      std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()}) {
        CHECK(empty(sample(view, projection, world, color, bad)));
        CHECK(empty(sample(view, projection, world, color, 0.1f, bad)));
        CHECK(empty(sample(view, projection, world, color, 0.1f, 1, bad)));
        for (unsigned index = 0; index < 16; ++index) {
            auto broken_view = view, broken_projection = projection;
            broken_view[index] = bad; broken_projection[index] = bad;
            CHECK(empty(sample(broken_view, projection, world, color)));
            CHECK(empty(sample(view, broken_projection, world, color)));
        }
        for (unsigned index = 0; index < 3; ++index) {
            auto broken_world = world, broken_color = color;
            broken_world[index] = bad; broken_color[index] = bad;
            CHECK(empty(sample(view, projection, broken_world, color)));
            CHECK(empty(sample(view, projection, world, broken_color)));
        }
    }
    CHECK(empty(prepare(nullptr, projection.data(), world.data(), color.data(), 0.1f, 1, 1, 800, 600)));
    CHECK(empty(prepare(view.data(), nullptr, world.data(), color.data(), 0.1f, 1, 1, 800, 600)));
    CHECK(empty(prepare(view.data(), projection.data(), nullptr, color.data(), 0.1f, 1, 1, 800, 600)));
    CHECK(empty(prepare(view.data(), projection.data(), world.data(), nullptr, 0.1f, 1, 1, 800, 600)));
    std::puts("Sunlight source projection, transforms, visibility, bounds, and invalid inputs: PASS");
}
