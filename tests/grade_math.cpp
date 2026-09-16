#include "config/grade.hpp"

#include <cmath>
#include <limits>

#define CHECK(condition)                                                                           \
    if (!(condition))                                                                              \
    return __LINE__

int main() {
    midnafx::grade::Controls values;
    auto neutral = midnafx::grade::prepare(values);
    CHECK(neutral.neutral);
    CHECK(neutral.uniforms.gain_r == 1.0f);
    CHECK(neutral.uniforms.gain_g == 1.0f);
    CHECK(neutral.uniforms.gain_b == 1.0f);
    CHECK(neutral.uniforms.gamma_inverse == 1.0f);
    values.value[midnafx::grade::Exposure] = 1.0f;
    auto brighter = midnafx::grade::prepare(values);
    CHECK(!brighter.neutral);
    CHECK(brighter.uniforms.gain_r == 2.0f);
    values.active[midnafx::grade::Exposure] = false;
    CHECK(midnafx::grade::prepare(values).neutral);
    values.value[midnafx::grade::Gamma] = 0.0f;
    auto safe = midnafx::grade::prepare(values);
    CHECK(std::isfinite(safe.uniforms.gamma_inverse));
    CHECK(safe.uniforms.gamma_inverse > 0.0f);
    values.value[midnafx::grade::Gamma] = std::numeric_limits<float>::quiet_NaN();
    CHECK(midnafx::grade::prepare(values).neutral);
}
