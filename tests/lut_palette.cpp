#include "render/lut.hpp"
#include <cmath>
#define CHECK(condition) if (!(condition)) return __LINE__
float decode(std::uint16_t value) {
    const unsigned exponent = (value >> 10) & 31, mantissa = value & 1023;
    return exponent ? std::ldexp(1.0f+float(mantissa)/1024, int(exponent)-15)
                    : std::ldexp(float(mantissa), -24);
}
int main() {
    using namespace midnafx::lut;
    const auto neutral = lattice(0), natural = lattice(1), twilight = lattice(2);
    const auto sample = [](const auto& table, unsigned r, unsigned g, unsigned b, unsigned c) {
        return decode(table[((b*Size+g)*Size+r)*4+c]);
    };
    for (unsigned b = 0; b < Size; ++b) for (unsigned g = 0; g < Size; ++g)
        for (unsigned r = 0; r < Size; ++r) {
            const unsigned coordinates[]{r,g,b};
            for (unsigned c = 0; c < 3; ++c) {
                CHECK(std::abs(sample(neutral,r,g,b,c)-float(coordinates[c])/32) < 0.001f);
                const float channel = sample(natural,r,g,b,c);
                CHECK(std::isfinite(channel) && channel >= 0 && channel <= 1);
            }
            CHECK(sample(natural,r,g,b,3) == 1);
        }
    for (unsigned c = 0; c < 3; ++c) {
        CHECK(sample(natural,0,0,0,c) == 0);
        CHECK(sample(natural,32,32,32,c) == 1);
        for (unsigned i = 1; i < Size; ++i) {
            CHECK(sample(natural,i,i,i,c) >= sample(natural,i-1,i-1,i-1,c));
            CHECK(std::abs(sample(natural,i,i,i,c)-float(i)/32) < 0.035f);
        }
    }
    // Muted foliage and blue midtones gain separation without reversing hues.
    CHECK(sample(natural,12,16,6,1)-sample(natural,12,16,6,0) > 4.0f/32);
    CHECK(sample(natural,8,12,18,2)-sample(natural,8,12,18,1) > 6.0f/32);
    // Warm complexion retains its ordering and bounded tone shift.
    CHECK(sample(natural,22,16,12,0) > sample(natural,22,16,12,1));
    CHECK(sample(natural,22,16,12,1) > sample(natural,22,16,12,2));
    for (unsigned c = 0; c < 3; ++c)
        CHECK(std::abs(sample(natural,22,16,12,c)-sample(neutral,22,16,12,c)) < 0.065f);
    CHECK(natural != twilight);
}
