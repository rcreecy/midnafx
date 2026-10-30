#pragma once

#include <array>

namespace midnafx::render::bloom {
inline constexpr unsigned MaxLevels = 5;
struct Extent {
    unsigned width = 0, height = 0;
};
struct Plan {
    std::array<Extent, MaxLevels> levels{};
    unsigned count = 0;
};

constexpr Plan make_plan(unsigned full_width, unsigned full_height) {
    Plan result;
    if (full_width == 0 || full_height == 0) return result;
    result.levels[0] = {full_width / 4 + (full_width % 4 != 0),
                        full_height / 4 + (full_height % 4 != 0)};
    result.count = 1;
    while (result.count < MaxLevels) {
        const auto previous = result.levels[result.count - 1];
        if (previous.height <= 34 || (previous.width == 1 && previous.height == 1)) break;
        const Extent next{previous.width / 2 + previous.width % 2,
                          previous.height / 2 + previous.height % 2};
        if (next.width == previous.width && next.height == previous.height) break;
        result.levels[result.count++] = next;
    }
    return result;
}
} // namespace midnafx::render::bloom
