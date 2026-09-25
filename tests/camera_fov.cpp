#include "game/camera_fov.hpp"

#include <cmath>
#include <limits>

#define CHECK(condition)                                                                           \
    if (!(condition))                                                                              \
    return __LINE__

int main() {
    using namespace midnafx::camera_fov;
    CHECK(std::abs(scale_vertical(60.0f, 1.0f) - 60.0f) < 0.0001f);
    CHECK(scale_vertical(60.0f, 1.15f) > 60.0f);
    CHECK(scale_vertical(60.0f, 0.85f) < 60.0f);
    CHECK(scale_vertical(60.0f, 0.0f) == 60.0f);
    CHECK(std::isnan(scale_vertical(std::numeric_limits<float>::quiet_NaN(), 1.1f)));
    CHECK(std::abs(advance(0.0f, 1.0f, 3, 1.0f) - 0.1f) < 0.0001f);
    CHECK(advance(0.95f, 1.0f, 4, 1.0f) == 1.0f);
    CHECK(std::abs(advance(0.0f, 1.0f, std::numeric_limits<std::uint64_t>::max(), 1.0f) -
                   (4.0f / 30.0f)) < 0.0001f);
    CHECK(advance(0.2f, 0.0f, 4, 0.0f) == 0.0f);
}
