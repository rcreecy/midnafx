#include "game/surface_class.hpp"
#define CHECK(c) if (!(c)) return __LINE__
int main() {
    using namespace midnafx::surface_class;
    CHECK(classify("aa_MA00_IshidanKabe_v","I_iwa") == Rock);
    CHECK(classify("aa_MA00_KokeWood_v","wood_grain") == Wood);
    CHECK(classify("aa_MA04_KoyojyuMiki_v","") == Wood);
    CHECK(classify("aa_ita04_v","") == Wood);
    CHECK(classify("cloth_body","linen_01") == Cloth);
    CHECK(classify("steel_plate","metal_01") == Metal);
    CHECK(classify("wet_rock","stone_01") == Wet);
    CHECK(classify("belt","leather_01") == Leather);
    CHECK(classify("garment_skirt","linen") == Cloth);
    CHECK(classify("boots","") == Leather);
    CHECK(classify("iron_boots","") == Unknown);
    CHECK(classify("cloth_wood","") == Unknown);
    CHECK(classify("aa_KiwaMiti_v_x","") == Unknown);
    CHECK(classify("body","skin_01") == Unknown);
    CHECK(classify("","") == Unknown);
    // Replacement files do not supply semantics; original material names do.
    CHECK(classify("aa_MA04_Rock_v","tex1_256x256_abcdef_14") == Rock);
    CHECK(classify("generic","tex1_256x256_abcdef_14") == Unknown);
}
