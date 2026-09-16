#pragma once

#include "config/grade.hpp"

#include <cstdint>

namespace midnafx::twilight {
enum class State : std::uint8_t { Unavailable, Normal, Spot, Active };

State classify(bool scene_ready, std::uint8_t world_dark);
float advance(float current, State state, float seconds, float transition_seconds);
grade::Prepared blend(const grade::Prepared& general, const grade::Prepared& target, float weight);
const char* label(State state);
} // namespace midnafx::twilight
