#pragma once
#include <algorithm>
#include <cstdint>

namespace midnafx::motion_clarity {
struct GameplayContext {
    bool enabled = false, player_present = false, paused = false;
    bool event_running = false, demo_camera = false, fading = false;
};
constexpr bool eligible(const GameplayContext& context) {
    return context.enabled && context.player_present && !context.paused &&
           !context.event_running && !context.demo_camera && !context.fading;
}
constexpr std::uint8_t retained_rate(std::uint8_t rate, unsigned percent) {
    return static_cast<std::uint8_t>((unsigned(rate) * std::min(percent, 100u) + 50u) / 100u);
}

// A frame-local override owns only its last written value. A different value
// belongs to the game or another writer and must not be replaced on restoration.
struct RateOverride {
    bool active = false;
    std::uint8_t original = 0, applied = 0;
    bool restore(std::uint8_t& value) {
        const bool owned = active && value == applied;
        if (owned) value = original;
        active = false;
        return owned;
    }
    bool begin(std::uint8_t& value, unsigned percent) {
        restore(value);
        const auto reduced = retained_rate(value, percent);
        if (reduced == value) return false;
        original = value;
        applied = reduced;
        active = true;
        value = applied;
        return true;
    }
};
} // namespace midnafx::motion_clarity
