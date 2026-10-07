#include "game/water_identity.h"

using midnafx::game::water::classify_stage_material;
using midnafx::game::water::has_stage_water;

int main() {
    bool ok = true;
    auto decision = classify_stage_material("F_SP103", 0, "part-1", "cc_MA02_IndirectWater_v");
    ok &= decision.selected && decision.thickness;

    decision = classify_stage_material("F_SP115", 0, "bgobj-0", "cc_MA06_NigoriWater_v_x");
    ok &= decision.selected && decision.thickness;
    decision = classify_stage_material("F_SP115", 0, "bgobj-1", "cc_MA02_IndirectWater_v");
    ok &= decision.selected && !decision.thickness;

    decision = classify_stage_material("F_SP118", 2, "part-0",
                                       "Desert_Lake_051214a_cc_MA02_water_v_x");
    ok &= decision.selected && decision.thickness;

    decision = classify_stage_material("F_SP127", 0, "part-4", "cc_MA09_Nigori_Water_v");
    ok &= decision.selected && !decision.thickness;

    ok &= !classify_stage_material("F_SP103", 1, "part-1", "cc_MA02_IndirectWater_v").selected;
    ok &= !classify_stage_material("F_SP103", 0, "part-0", "cc_MA09_Sunbeam_v").selected;
    ok &= !classify_stage_material("F_SP109", 0, "part-0", "cc_MA09_Oil_Suimen_meramera_v").selected;
    ok &= !classify_stage_material("F_SP103", 0, "part-0", "cd_MA03_TakiKasan_v_x").selected;
    ok &= !classify_stage_material("UNKNOWN", 0, "part-0", "cc_MA06_water_v_x").selected;

    ok &= has_stage_water("F_SP103");
    ok &= has_stage_water("D_MN01");
    ok &= !has_stage_water("F_SP112");
    return ok ? 0 : 1;
}
