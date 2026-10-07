#pragma once

#include <string_view>

namespace midnafx::game::water {

struct StageMaterialDecision {
    bool selected = false;
    bool thickness = false;
};

StageMaterialDecision classify_stage_material(std::string_view stage, int room,
                                               std::string_view role,
                                               std::string_view material);
bool has_stage_water(std::string_view stage);

} // namespace midnafx::game::water
