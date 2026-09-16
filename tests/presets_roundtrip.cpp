#include "config/presets.hpp"

#define CHECK(condition)                                                                           \
    if (!(condition))                                                                              \
    return __LINE__

int main() {
    using namespace midnafx;
    std::vector<presets::Entry> entries{{"Warm Night", {}}};
    entries[0].snapshot.values[grade::Temperature] = 45;
    entries[0].snapshot.active[grade::Saturation] = false;
    const auto encoded = presets::encode(entries);
    CHECK(!encoded.empty());
    std::vector<presets::Entry> decoded;
    CHECK(presets::decode(encoded, decoded));
    CHECK(decoded.size() == 1);
    CHECK(decoded[0].name == "Warm Night");
    CHECK(decoded[0].snapshot.values[grade::Temperature] == 45);
    CHECK(!decoded[0].snapshot.active[grade::Saturation]);
    CHECK(!presets::decode("MFX1\nBroken\t0,0,100,100,100,0,0,999,255\n", decoded));
    CHECK(decoded[0].name == "Warm Night");
    CHECK(!presets::decode(encoded + encoded.substr(5), decoded));
    CHECK(!presets::decode("MFX2\n", decoded));
}
