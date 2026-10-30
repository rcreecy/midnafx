#include "render/bloom_plan.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <limits>

#define CHECK(condition)                                                                            \
    do {                                                                                            \
        if (!(condition)) {                                                                         \
            std::fprintf(stderr, "Bloom plan check failed at line %d: %s\n", __LINE__, #condition); \
            return __LINE__;                                                                        \
        }                                                                                           \
    } while (false)

int main() {
    using namespace midnafx::render::bloom;
    static_assert(MaxLevels == 5);
    static_assert(make_plan(0, 100).count == 0);
    static_assert(make_plan(100, 0).count == 0);
    static_assert(make_plan(1, 1).levels[0].width == 1);
    static_assert(make_plan(1, 1).count == 1);
    static_assert(make_plan(1920, 1080).count == 4);
    static_assert(make_plan(3840, 2160).count == 5);
    const auto empty = [](const Plan& plan) {
        if (plan.count != 0) return false;
        for (const auto extent : plan.levels)
            if (extent.width != 0 || extent.height != 0) return false;
        return true;
    };
    CHECK(empty(make_plan(0, 0)));
    CHECK(empty(make_plan(0, std::numeric_limits<unsigned>::max())));
    CHECK(empty(make_plan(std::numeric_limits<unsigned>::max(), 0)));
    const auto matches = [](const Plan& plan, std::initializer_list<Extent> expected) {
        if (plan.count != expected.size()) return false;
        unsigned index = 0;
        for (const auto extent : expected) {
            const auto actual = plan.levels[index++];
            if (actual.width != extent.width || actual.height != extent.height) return false;
        }
        for (; index < MaxLevels; ++index)
            if (plan.levels[index].width || plan.levels[index].height) return false;
        return true;
    };
    CHECK(matches(make_plan(1920, 1080), {{480,270}, {240,135}, {120,68}, {60,34}}));
    CHECK(matches(make_plan(3840, 2160), {{960,540}, {480,270}, {240,135}, {120,68}, {60,34}}));
    CHECK(matches(make_plan(1921, 1081), {{481,271}, {241,136}, {121,68}, {61,34}}));
    CHECK(matches(make_plan(7, 5), {{2,2}}));
    CHECK(matches(make_plan(1, 1), {{1,1}}));
    CHECK(matches(make_plan(9, 136), {{3,34}}));
    CHECK(matches(make_plan(9, 137), {{3,35}, {2,18}}));
    CHECK(matches(make_plan(1, 137), {{1,35}, {1,18}}));
    CHECK(matches(make_plan(137, 1), {{35,1}})); // Height controls the stop, not the long axis.
    CHECK(matches(make_plan(4, 272), {{1,68}, {1,34}}));
    CHECK(matches(make_plan(4, 273), {{1,69}, {1,35}, {1,18}}));
    CHECK(matches(make_plan(7680, 4320), {{1920,1080}, {960,540}, {480,270}, {240,135}, {120,68}}));

    const auto area = [](Extent extent) { return std::uint64_t(extent.width) * extent.height; };
    const auto total_area = [&](const Plan& plan) {
        std::uint64_t total = 0;
        for (unsigned i = 0; i < plan.count; ++i) total += area(plan.levels[i]);
        return total;
    };
    for (const auto full : {Extent{1920,1080}, {3840,2160}, {1921,1081}, {3841,2161}}) {
        const auto plan = make_plan(full.width, full.height);
        CHECK(total_area(plan) >= area(plan.levels[0]));
        CHECK(double(total_area(plan)) / double(area(plan.levels[0])) <= 1.34);
    }
    // Odd, very narrow levels cannot promise the regular two-dimensional 4/3
    // area ratio, because one axis saturates at one pixel. Check their exact
    // dimensions above and a separate conservative bound instead.
    for (const auto full : {Extent{1,137}, {9,137}, {1,1081}, {7,2161}}) {
        const auto plan = make_plan(full.width, full.height);
        CHECK(total_area(plan) <= area(plan.levels[0]) * 2);
    }
    constexpr unsigned maximum = std::numeric_limits<unsigned>::max();
    const auto huge = make_plan(maximum, maximum);
    CHECK(huge.count == MaxLevels);
    CHECK(huge.levels[0].width == maximum / 4 + (maximum % 4 != 0));
    CHECK(huge.levels[0].height == huge.levels[0].width);
    for (const auto full : {Extent{1,1}, {1,maximum}, {maximum,1}, {maximum,maximum},
                            {136,136}, {137,137}, {1919,1079}, {3841,2161}}) {
        const auto plan = make_plan(full.width, full.height);
        CHECK(plan.count > 0 && plan.count <= MaxLevels);
        CHECK(plan.levels[0].width == full.width / 4 + (full.width % 4 != 0));
        CHECK(plan.levels[0].height == full.height / 4 + (full.height % 4 != 0));
        for (unsigned i = 0; i < plan.count; ++i) {
            const auto current = plan.levels[i];
            CHECK(current.width > 0 && current.height > 0);
            CHECK(current.width <= full.width && current.height <= full.height);
            if (i == 0) continue;
            const auto previous = plan.levels[i - 1];
            CHECK(previous.height > 34);
            CHECK(current.width == previous.width / 2 + previous.width % 2);
            CHECK(current.height == previous.height / 2 + previous.height % 2);
            CHECK(current.width != previous.width || current.height != previous.height);
        }
        CHECK(plan.count == MaxLevels || plan.levels[plan.count - 1].height <= 34);
        for (unsigned i = plan.count; i < MaxLevels; ++i)
            CHECK(plan.levels[i].width == 0 && plan.levels[i].height == 0);
    }
    std::puts("Bloom plan: bounded pyramid, stopping boundaries, odd dimensions, and area overhead: PASS");
}
