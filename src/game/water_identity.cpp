#include "game/water_identity.h"

#include <array>

namespace midnafx::game::water {
namespace {
struct StageMaterial {
    std::string_view stage;
    int room;
    std::string_view role;
    std::string_view material;
    bool thickness;
};

constexpr std::array kStageMaterials = {
    StageMaterial{"D_MN01", 0, "part-0", "cc_MA06_mizugiwa_v_x", true},
    StageMaterial{"D_MN01", 0, "part-0", "cc_MA06_nami_v_x", true},
    StageMaterial{"D_MN01", 0, "part-0", "cc_MA09_mera_v", false},
    StageMaterial{"D_MN01", 0, "part-1", "cc_MA02_water_v", true},
    StageMaterial{"D_MN01", 1, "part-0", "cc_MA06_Suimen_v_x", true},
    StageMaterial{"D_MN01", 1, "part-1", "cc_MA02_mizu_v", true},
    StageMaterial{"D_MN01", 2, "part-0", "cc_MA06_water_v", true},
    StageMaterial{"D_MN01", 2, "part-1", "cc_MA02_suimen_v", true},
    StageMaterial{"D_MN01", 5, "part-0", "cc_MA06_mizugiwa_v_x", true},
    StageMaterial{"D_MN01", 5, "part-0", "cc_MA06_nami_v_x", true},
    StageMaterial{"D_MN01", 5, "part-0", "cc_MA09_mera_v_x", false},
    StageMaterial{"D_MN01", 5, "part-1", "cc_MA02_mizu_v", true},
    StageMaterial{"D_MN01", 6, "part-0", "cc_MA06_mizugiwa_v_x", true},
    StageMaterial{"D_MN01", 6, "part-0", "cc_MA06_nami_v_x", true},
    StageMaterial{"D_MN01", 6, "part-0", "cc_MA09_mera_v", false},
    StageMaterial{"D_MN01", 6, "part-1", "cc_MA02_mizu_v", true},
    StageMaterial{"D_MN01", 9, "part-0", "cc_MA06_mizugiwa_v_x", true},
    StageMaterial{"D_MN01", 9, "part-0", "cc_MA06_nami_v_x", true},
    StageMaterial{"D_MN01", 9, "part-0", "cc_MA09_mera_v", false},
    StageMaterial{"D_MN01", 9, "part-1", "cc_MA02_mizu_v", true},
    StageMaterial{"D_MN01B", 51, "part-0", "cc_MA06_mizugiwa_v_x", true},
    StageMaterial{"D_MN01B", 51, "part-0", "cc_MA06_nami_v_x", true},
    StageMaterial{"D_MN01B", 51, "part-0", "cc_MA09_mera_v", false},
    StageMaterial{"D_MN01B", 51, "part-1", "cc_MA02_mizu_v", true},
    StageMaterial{"D_MN04", 5, "part-0", "cc_MA06_mizugiwa_v_x", true},
    StageMaterial{"D_MN04", 5, "part-0", "cc_MA09_water_v", false},
    StageMaterial{"D_MN04", 5, "part-1", "cc_MA02_water_v", true},
    StageMaterial{"D_MN04", 6, "part-0", "cc_MA06_mizugiwa_v_x", true},
    StageMaterial{"D_MN04", 6, "part-0", "cc_MA09_water_v", false},
    StageMaterial{"D_MN04", 6, "part-1", "cc_MA02_water_v", true},
    StageMaterial{"D_MN04", 7, "part-0", "cc_MA06_mizugiwa_v_x", true},
    StageMaterial{"D_MN04", 7, "part-0", "cc_MA09_water_v", false},
    StageMaterial{"D_MN04", 7, "part-1", "cc_MA02_water_v", true},
    StageMaterial{"D_MN04", 9, "part-0", "cc_MA06_mizugiwa_v_x", true},
    StageMaterial{"D_MN04", 9, "part-0", "cc_MA09_water_v", false},
    StageMaterial{"D_MN04", 9, "part-1", "cc_MA02_water_v", true},
    StageMaterial{"D_MN05", 1, "part-0", "cc_MA06_mizugiwa_v_x", true},
    StageMaterial{"D_MN05", 1, "part-0", "cc_MA06_nami_v_x", true},
    StageMaterial{"D_MN05", 1, "part-0", "cc_MA09_mera_v_x", false},
    StageMaterial{"D_MN05", 1, "part-1", "cc_MA02_water_v_x", true},
    StageMaterial{"D_MN05", 5, "part-0", "cc_MA06_nami_v_x", true},
    StageMaterial{"D_MN05", 5, "part-0", "cc_MA09_mera_v_x", false},
    StageMaterial{"D_MN05", 5, "part-1", "cc_MA02_water_v_x", true},
    StageMaterial{"D_MN05", 10, "part-0", "cc_MA06_nami_v_x", true},
    StageMaterial{"D_MN05", 10, "part-0", "cc_MA09_mera_v_x", false},
    StageMaterial{"D_MN05", 10, "part-1", "cc_MA02_water_v_x", true},
    StageMaterial{"D_MN05", 22, "part-0", "cc_MA09_water_v", false},
    StageMaterial{"D_MN05", 22, "part-1", "cc_MA02_water_v", true},
    StageMaterial{"D_MN05A", 50, "part-0", "cc_MA06_NigoriWater_v_x", true},
    StageMaterial{"D_MN05A", 50, "part-0", "cc_MA09_meraWater_v", false},
    StageMaterial{"D_MN05A", 50, "part-1", "cc_MA02_IndirectWater1_v", true},
    StageMaterial{"D_MN07", 0, "part-0", "cc_MA06_nigoriWater_v_x", true},
    StageMaterial{"D_MN07", 0, "part-0", "cc_MA09_SuiroWater_v", false},
    StageMaterial{"D_MN07", 0, "part-1", "cc_MA02_water_v", true},
    StageMaterial{"D_MN08D", 53, "part-0", "cc_MA06_NigoriWater1_v_x", true},
    StageMaterial{"D_MN08D", 53, "part-1", "cc_MA09_MeraWater_v", false},
    StageMaterial{"D_MN09B", 0, "part-0", "cd_MA09_MeraWater_v", false},
    StageMaterial{"D_MN09B", 0, "part-1", "cc_MA02_water_v", true},
    StageMaterial{"D_MN09C", 0, "part-0", "cd_MA09_MeraWater_v", false},
    StageMaterial{"D_MN09C", 0, "part-1", "cc_MA02_water_v", true},
    StageMaterial{"D_SB09", 4, "part-0", "cc_MA06_NigoriWater_v_x", true},
    StageMaterial{"D_SB09", 4, "part-0", "cd_MA09_MeraWater_v", false},
    StageMaterial{"D_SB09", 4, "part-1", "cc_MA02_water_v", true},
    StageMaterial{"F_SP102", 0, "part-0", "cd_MA09_MeraWater_v", false},
    StageMaterial{"F_SP102", 0, "part-1", "cc_MA02_water_v", true},
    StageMaterial{"F_SP103", 0, "part-0", "cc_MA09_Water_v", false},
    StageMaterial{"F_SP103", 0, "part-1", "cc_MA02_IndirectWater_v", true},
    StageMaterial{"F_SP104", 1, "part-1", "cc_MA02_water_v", true},
    StageMaterial{"F_SP108", 1, "part-0", "cc_MA06_water_v_x", true},
    StageMaterial{"F_SP108", 1, "part-0", "cg_MA09_water_v", false},
    StageMaterial{"F_SP108", 1, "part-1", "cc_MA02_IndirectWater_v", true},
    StageMaterial{"F_SP108", 4, "part-1", "cc_MA02_Water_v", true},
    StageMaterial{"F_SP109", 0, "part-1", "cc_MA02_water_v", true},
    StageMaterial{"F_SP115", 0, "part-0", "cc_MA06_NigoriWater_v_x", true},
    StageMaterial{"F_SP115", 0, "part-0", "cd_MA09_MeraWater_v_x", false},
    StageMaterial{"F_SP115", 0, "bgobj-0", "cc_MA06_NigoriWater_v_x", true},
    StageMaterial{"F_SP115", 0, "bgobj-0", "cd_MA09_MeraWater_v", false},
    StageMaterial{"F_SP115", 0, "bgobj-1", "cc_MA02_IndirectWater_v", false},
    StageMaterial{"F_SP115", 1, "part-0", "cc_MA06_NigoriWater1_v_x", true},
    StageMaterial{"F_SP115", 1, "part-0", "cd_MA09_MeraWater_v_x", false},
    StageMaterial{"F_SP115", 1, "part-1", "cc_MA02_water_v_x", true},
    StageMaterial{"F_SP116", 1, "part-0", "cc_MA09water_v", false},
    StageMaterial{"F_SP116", 3, "part-0", "cc_MA09_water_v", false},
    StageMaterial{"F_SP116", 3, "part-1", "cc_MA02_water_v", true},
    StageMaterial{"F_SP117", 1, "part-1", "cc_MA02_IndirectWater_v", true},
    StageMaterial{"F_SP117", 3, "part-0", "cc_MA06_Water_v_x", true},
    StageMaterial{"F_SP117", 3, "part-0", "cc_MA09_Water_v", false},
    StageMaterial{"F_SP117", 3, "part-1", "cc_MA02_IndirectWater_v", true},
    StageMaterial{"F_SP118", 2, "part-0", "Desert_Lake_051214a_cc_MA02_water_v_x", true},
    StageMaterial{"F_SP121", 0, "part-0", "cd_MA09_MeraWater_v", false},
    StageMaterial{"F_SP121", 0, "part-1", "cc_MA02_water_v", true},
    StageMaterial{"F_SP121", 6, "part-0", "cd_MA09_MeraWater_v", false},
    StageMaterial{"F_SP121", 6, "part-1", "cc_MA02_water_v", true},
    StageMaterial{"F_SP122", 16, "part-0", "cc_MA06_NigoriWater_v_x", true},
    StageMaterial{"F_SP122", 16, "part-0", "cd_MA09_MeraWater_v", false},
    StageMaterial{"F_SP122", 16, "part-1", "cc_MA02_water_v", true},
    StageMaterial{"F_SP122", 17, "part-0", "cc_MA06_NigoriWater_v_x", true},
    StageMaterial{"F_SP122", 17, "part-0", "cd_MA09_MeraWater_v", false},
    StageMaterial{"F_SP122", 17, "part-1", "cc_MA02_water_v", true},
    StageMaterial{"F_SP124", 0, "part-1", "cc_MA06_NigoriWater_v_x", true},
    StageMaterial{"F_SP124", 0, "part-1", "cd_MA09_MeraWater_v", false},
    StageMaterial{"F_SP127", 0, "part-1", "cc_MA02_IndirectWater_v", true},
    StageMaterial{"F_SP127", 0, "part-4", "cc_MA06_Nigori_Water_v_x", true},
    StageMaterial{"F_SP127", 0, "part-4", "cc_MA09_Nigori_Water_v", false},
    StageMaterial{"F_SP128", 0, "part-0", "cc_MA09_water_v", false},
    StageMaterial{"F_SP128", 0, "part-1", "cc_MA02_water_v", true},
    StageMaterial{"R_SP116", 6, "part-1", "cc_MA02_water_v", true},
    StageMaterial{"R_SP127", 0, "part-1", "cc_MA02_IndirectWater_v", true},
    StageMaterial{"R_SP127", 0, "part-4", "cc_MA06_Nigori_Water_v_x", true},
    StageMaterial{"R_SP127", 0, "part-4", "cc_MA09_Nigori_Water_v", false},
};
} // namespace

StageMaterialDecision classify_stage_material(std::string_view stage, int room,
                                               std::string_view role,
                                               std::string_view material) {
    for (const auto& identity : kStageMaterials) {
        if (identity.stage == stage && identity.room == room && identity.role == role &&
            identity.material == material)
            return {true, identity.thickness};
    }
    return {};
}

bool has_stage_water(std::string_view stage) {
    for (const auto& identity : kStageMaterials) {
        if (identity.stage == stage)
            return true;
    }
    return false;
}

} // namespace midnafx::game::water
