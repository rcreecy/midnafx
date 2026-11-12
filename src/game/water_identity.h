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
bool is_water_surface_family(std::string_view material);

} // namespace midnafx::game::water
