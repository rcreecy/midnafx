#include "visual.hpp"

#include <algorithm>

namespace midnafx::visual {
DebugMode debug_mode(std::int64_t raw) {
    if (raw < 0 || raw > static_cast<std::int64_t>(DebugMode::Difference))
        return DebugMode::Final;
    return static_cast<DebugMode>(raw);
}
std::int64_t detail_value(std::int64_t raw) { return std::clamp<std::int64_t>(raw, 0, 50); }
float detail_strength(bool enabled, std::int64_t raw) {
    return enabled ? static_cast<float>(detail_value(raw)) / 100.0f : 0.0f;
}
std::int64_t split_percent(std::int64_t raw) { return std::clamp<std::int64_t>(raw, 10, 90); }
std::uint32_t split_boundary(std::uint32_t width, std::int64_t percent) {
    return static_cast<std::uint32_t>((static_cast<std::uint64_t>(width) * split_percent(percent)) /
                                      100u);
}
bool original_side(std::uint32_t x, std::uint32_t width, std::int64_t percent) {
    return x < split_boundary(width, percent);
}
} // namespace midnafx::visual
