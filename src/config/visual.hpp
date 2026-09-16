#pragma once

#include <cstdint>

namespace midnafx::visual {
enum class DebugMode : std::uint32_t {
    Final = 0,
    Passthrough = 1,
    Split = 2,
    Luminance = 3,
    HighlightClipping = 4,
    ShadowClipping = 5,
    Difference = 6,
};

DebugMode debug_mode(std::int64_t raw);
std::int64_t detail_value(std::int64_t raw);
float detail_strength(bool enabled, std::int64_t raw);
std::int64_t split_percent(std::int64_t raw);
std::uint32_t split_boundary(std::uint32_t width, std::int64_t percent);
bool original_side(std::uint32_t x, std::uint32_t width, std::int64_t percent);
} // namespace midnafx::visual
