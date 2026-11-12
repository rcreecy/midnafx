#pragma once
#include "terrain_trails.hpp"

#include <d/d_com_inf_game.h>
#include <array>
#include <cmath>

namespace midnafx::terrain_interaction {
using Sources = terrain_trails::Sources;

inline Sources sample() {
    Sources sources{};
    for (unsigned i = 0; i < sources.size(); ++i) {
        const auto* actor = dComIfGp_getPlayer(i);
        if (!actor) continue;
        const auto& p = actor->current.pos;
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) ||
            std::abs(p.x) > 1e7f || std::abs(p.y) > 1e7f || std::abs(p.z) > 1e7f) continue;
        sources[i] = {p.x, p.y, p.z, i == 0 ? 68.0f : 145.0f};
    }
    return sources;
}
}
