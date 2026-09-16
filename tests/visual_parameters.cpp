#include "config/visual.hpp"

#define CHECK(condition)                                                                           \
    if (!(condition))                                                                              \
    return __LINE__

int main() {
    using namespace midnafx::visual;
    CHECK(detail_strength(false, 50) == 0.0f);
    CHECK(detail_strength(true, -9) == 0.0f);
    CHECK(detail_strength(true, 999) == 0.5f);
    CHECK(detail_strength(true, 20) == 0.2f);
    for (std::int64_t raw = 0; raw <= 6; ++raw)
        CHECK(static_cast<std::int64_t>(debug_mode(raw)) == raw);
    CHECK(debug_mode(-1) == DebugMode::Final);
    CHECK(debug_mode(7) == DebugMode::Final);
    CHECK(split_boundary(1920, 50) == 960);
    CHECK(original_side(959, 1920, 50));
    CHECK(!original_side(960, 1920, 50));
    CHECK(split_boundary(3840, 90) == 3456);
    CHECK(split_boundary(1920, -10) == 192);
    CHECK(split_boundary(0, 50) == 0);
}
