#pragma once

#include <array>

namespace midnafx::grade {
enum Effect : unsigned {
    Exposure,
    BlackPoint,
    Contrast,
    Gamma,
    Saturation,
    HighlightRolloff,
    Temperature,
    Tint,
    Count,
};

struct Controls {
    std::array<float, Count> value{0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f};
    std::array<bool, Count> active{true, true, true, true, true, true, true, true};
};

// Eight scalar floats match the WGSL uniform struct exactly (32 bytes).
struct alignas(16) Uniforms {
    float gain_r;
    float gain_g;
    float gain_b;
    float black_point;
    float contrast;
    float saturation;
    float gamma_inverse;
    float rolloff;
};
static_assert(sizeof(Uniforms) == 32);

struct Prepared {
    Uniforms uniforms;
    bool neutral;
};

Prepared prepare(const Controls& controls);
} // namespace midnafx::grade
