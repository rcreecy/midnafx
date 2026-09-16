#pragma once

#include <array>
#include <cstdint>
#include <type_traits>

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

// Eight grading floats plus detail/debug parameters match WGSL's 48-byte uniform struct.
struct alignas(16) Uniforms {
    float gain_r;
    float gain_g;
    float gain_b;
    float black_point;
    float contrast;
    float saturation;
    float gamma_inverse;
    float rolloff;
    float detail_strength;
    float difference_gain;
    std::uint32_t debug_mode;
    std::uint32_t split_x;
};
static_assert(sizeof(Uniforms) == 48);
static_assert(std::is_standard_layout_v<Uniforms>);

struct Prepared {
    Uniforms uniforms;
    bool neutral;
};

Prepared prepare(const Controls& controls);
} // namespace midnafx::grade
