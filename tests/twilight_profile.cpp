#include "config/grade.hpp"
#include "game/twilight.hpp"

#include <cmath>

#define CHECK(condition)                                                                           \
    if (!(condition))                                                                              \
    return __LINE__

int main() {
    using namespace midnafx;
    using twilight::State;
    CHECK(twilight::classify(false, 1) == State::Unavailable);
    CHECK(twilight::classify(true, 0) == State::Normal);
    CHECK(twilight::classify(true, 1) == State::Active);
    CHECK(twilight::classify(true, 2) == State::Spot);
    CHECK(twilight::classify(true, 3) == State::Normal);
    CHECK(twilight::advance(0.0f, State::Spot, 0.1f, 1.0f) == 0.0f);
    CHECK(twilight::advance(0.0f, State::Active, 0.1f, 1.0f) == 0.1f);
    CHECK(twilight::advance(1.0f, State::Active, 0.1f, 1.0f) == 1.0f);
    CHECK(twilight::advance(0.95f, State::Active, 0.1f, 1.0f) == 1.0f);
    CHECK(twilight::advance(0.1f, State::Normal, 0.1f, 1.0f) == 0.0f);
    CHECK(twilight::advance(0.0f, State::Active, 0.1f, 0.0f) == 1.0f);

    grade::Controls controls;
    const auto general = grade::prepare(controls);
    controls.value[grade::Exposure] = 1.0f;
    auto target = grade::prepare(controls);
    target.uniforms.detail_strength = 0.3f;
    CHECK(twilight::blend(general, target, 0.0f).neutral);
    CHECK(!twilight::blend(general, target, 0.5f).neutral);
    CHECK(twilight::blend(general, target, 1.0f).uniforms.gain_r == 2.0f);
    const auto middle = twilight::blend(general, target, 0.5f);
    CHECK(std::abs(middle.uniforms.gain_r - 1.5f) < 0.0001f);
    CHECK(std::abs(middle.uniforms.detail_strength - 0.15f) < 0.0001f);
}
