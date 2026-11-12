#include "render/airborne_motion.hpp"
#include <limits>
#define CHECK(condition) if (!(condition)) return __LINE__
int main() {
    using namespace midnafx::render::airborne;
    CHECK(night_strength(0) == 1);
    CHECK(night_strength(180) == 0);
    CHECK(night_strength(std::numeric_limits<float>::quiet_NaN()) == 0);
    const std::array<float, 4> wind{1, 0, 1, 0};
    for (unsigned seed = 0; seed < 128; ++seed) {
        const float lifetime = 9 + random(seed+1)*8;
        const float reset = (1-random(seed+2))*lifetime;
        CHECK(pollen(seed, reset-0.0001f, wind).opacity < 0.001f);
        CHECK(pollen(seed, reset+0.0001f, wind).opacity < 0.001f);
        for (float t = 0; t < 60; t += 0.17f) {
            for (const auto motion : {pollen(seed,t,wind), firefly(seed,t,wind)}) {
                CHECK(motion.opacity >= 0 && motion.opacity <= 1);
                for (const float value : motion.offset) CHECK(std::isfinite(value) && std::abs(value) < 330);
                CHECK(motion.offset[1] > 0);
            }
            const auto before = firefly(seed,t,wind), after = firefly(seed,t+0.001f,wind);
            for (unsigned axis = 0; axis < 3; ++axis)
                CHECK(std::abs(before.offset[axis]-after.offset[axis]) < 0.1f);
        }
    }
}
