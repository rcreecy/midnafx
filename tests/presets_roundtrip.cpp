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
    CHECK(!presets::decode("MFX3\n", decoded));
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
}
