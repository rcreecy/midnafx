#include "config/presets.hpp"

#define CHECK(condition)                                                                           \
    if (!(condition))                                                                              \
    return __LINE__

int main() {
    using namespace midnafx;
    std::vector<presets::Entry> entries{{"Warm Night", {}}};
    entries[0].snapshot.values[grade::Temperature] = 45;
    entries[0].snapshot.active[grade::Saturation] = false;
    entries[0].snapshot.detail_enabled = true;
    entries[0].snapshot.detail_strength = 35;
    const auto encoded = presets::encode(entries);
    CHECK(!encoded.empty());
    CHECK(encoded.starts_with("MFX2\n"));
    std::vector<presets::Entry> decoded;
    CHECK(presets::decode(encoded, decoded));
    CHECK(decoded.size() == 1);
    CHECK(decoded[0].name == "Warm Night");
    CHECK(decoded[0].snapshot.values[grade::Temperature] == 45);
    CHECK(!decoded[0].snapshot.active[grade::Saturation]);
    CHECK(decoded[0].snapshot.detail_enabled);
    CHECK(decoded[0].snapshot.detail_strength == 35);
    CHECK(!presets::decode("MFX1\nBroken\t0,0,100,100,100,0,0,999,255\n", decoded));
    CHECK(decoded[0].name == "Warm Night");
    CHECK(!presets::decode(encoded + encoded.substr(5), decoded));
    CHECK(!presets::decode("MFX5\n", decoded));
    CHECK(!presets::decode("MFX2\nBad\t0,0,100,100,100,0,0,0,255,1,99\n", decoded));
    CHECK(presets::decode("MFX1\nLegacy\t0,0,100,100,100,0,0,0,255\n", decoded));
    CHECK(decoded.size() == 1 && !decoded[0].snapshot.detail_enabled);
    CHECK(decoded[0].snapshot.detail_strength == 20);
    const presets::Entry custom{"Live", entries[0].snapshot};
    CHECK(presets::decode(presets::encode({custom}), decoded));
    CHECK(decoded.size() == 1 && decoded[0].name == "Live");
    CHECK(decoded[0].snapshot.values[grade::Temperature] == 45);
    const auto smoke = presets::smoke_test();
    CHECK(smoke.detail_enabled);
    CHECK(smoke.detail_strength <= 50);
    CHECK(!presets::encode({{"Smoke", smoke}}).empty());
    const auto realism = presets::vivid_realism();
    CHECK(presets::decode(presets::encode({{"Realism copy", realism}}), decoded));
    CHECK(decoded[0].snapshot.values == realism.values);
    CHECK(decoded[0].snapshot.active == realism.active);
    CHECK(decoded[0].snapshot.detail_enabled);
    CHECK(decoded[0].snapshot.detail_strength == realism.detail_strength);
    // Built-in labels use a slash so existing saved names cannot collide.
    CHECK(!presets::valid_name("Natural / Vivid Realism"));
    CHECK(presets::decode("MFX2\nLegacy\t0,0,100,100,100,0,0,0,255,1,12\n", decoded));
    CHECK(decoded[0].snapshot.product_flags == -1);
    CHECK(decoded[0].snapshot.scene_flags == -1);
    for (int flags = -1; flags <= 127; ++flags) {
        auto snapshot = realism;
        snapshot.product_flags = flags;
        CHECK(presets::decode(presets::encode({{"Product", snapshot}}), decoded));
        CHECK(decoded[0].snapshot.product_flags == flags);
        CHECK(decoded[0].snapshot.values == realism.values);
    }
    const auto retained = decoded[0].snapshot.product_flags;
    CHECK(!presets::decode("MFX3\nBad\t0,0,100,100,100,0,0,0,255,1,12,128\n", decoded));
    CHECK(decoded[0].snapshot.product_flags == retained);
    CHECK(presets::vanilla_plus().product_flags == presets::Grading);
    CHECK(presets::enhanced().product_flags == (presets::Grading | presets::Camera));
    CHECK((presets::enhanced().product_flags & presets::Dof) == 0);
    const auto remaster = presets::remaster();
    CHECK((remaster.product_flags & presets::SkinnedGeometry) != 0);
    CHECK((remaster.product_flags & presets::MovementCamera) != 0);
    CHECK(remaster.scene_flags == (presets::AmbientOcclusion | presets::AntiAliasing | presets::Water | presets::Atmosphere | presets::AdaptiveExposure | presets::Bloom | presets::ContactShadows));
    CHECK(presets::vanilla().scene_flags == 0 && presets::vanilla().product_flags == 0);
    for (int flags = -1; flags <= 127; ++flags) {
        auto snapshot = remaster;
        snapshot.scene_flags = flags;
        const auto text = presets::encode({{"Remaster copy", snapshot}, {"Legacy copy", realism}});
        CHECK(presets::decode(text, decoded));
        CHECK(decoded[0].snapshot.scene_flags == flags);
        CHECK(decoded[0].snapshot.product_flags == remaster.product_flags);
        CHECK(decoded[1].snapshot.scene_flags == -1);
    }
    CHECK(!presets::decode("MFX4\nBad\t0,0,100,100,100,0,0,0,255,1,12,9,128\n", decoded));
    CHECK(decoded.size() == 2 && decoded[0].snapshot.scene_flags == 127);
    CHECK(presets::decode("MFX4\nOld scene\t0,0,100,100,100,0,0,0,255,1,12,9,15\n", decoded));
    CHECK((decoded[0].snapshot.scene_flags & presets::AdaptiveExposure) == 0);
    CHECK(presets::decode("MFX4\nOld lighting\t0,0,100,100,100,0,0,0,255,1,12,9,31\n", decoded));
    CHECK((decoded[0].snapshot.scene_flags & presets::Bloom) == 0);
    CHECK(presets::decode("MFX4\nOld bloom\t0,0,100,100,100,0,0,0,255,1,12,9,63\n", decoded));
    CHECK((decoded[0].snapshot.scene_flags & presets::ContactShadows) == 0);
    CHECK(presets::decode("MFX3\nOld camera\t0,0,100,100,100,0,0,0,255,1,12,63\n", decoded));
    CHECK((decoded[0].snapshot.product_flags & presets::MovementCamera) == 0);
    auto invalid = remaster;
    invalid.scene_flags = 128;
    CHECK(presets::encode({{"Invalid", invalid}}).empty());
}
