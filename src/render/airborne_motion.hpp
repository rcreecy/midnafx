#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace midnafx::render::airborne {
inline float random(std::uint32_t seed) {
    seed = (seed ^ (seed >> 16)) * 0x7feb352du;
    seed = (seed ^ (seed >> 15)) * 0x846ca68bu;
    return float((seed ^ (seed >> 16)) & 0x00ffffffu) / 16777216.0f;
}
inline float smooth(float a, float b, float value) {
    const float t = std::clamp((value-a)/(b-a), 0.0f, 1.0f);
    return t*t*(3.0f-2.0f*t);
}
inline float night_strength(float angle) {
    if (!std::isfinite(angle)) return 0;
    return smooth(0.1f, 0.8f, std::cos(angle * 0.01745329252f));
}
struct Motion { std::array<float, 3> offset; float opacity; };
inline Motion pollen(std::uint32_t seed, float time, const std::array<float, 4>& wind) {
    const float age = time / (9.0f + random(seed+1)*8.0f) + random(seed+2);
    const float phase = age - std::floor(age);
    const float drift = phase * (110.0f + random(seed+3)*120.0f) * (0.15f + 0.85f*wind[2]);
    const float sway = std::sin(time * 0.6f + random(seed+4)*6.283185f);
    return {{{wind[0] * drift + sway*18.0f,
              45.0f + random(seed+5)*160.0f + phase*120.0f,
              wind[1] * drift + std::cos(time*0.43f + random(seed+6)*6.283185f)*18.0f}},
            smooth(0, 0.15f, phase) * (1.0f-smooth(0.65f, 1.0f, phase))};
}
inline Motion firefly(std::uint32_t seed, float time, const std::array<float, 4>& wind) {
    const float phase = random(seed)*6.283185f;
    return {{{std::sin(time*0.47f + phase)*65.0f + wind[0]*wind[2]*25.0f,
              65.0f + random(seed+1)*100.0f + std::sin(time*0.63f + phase)*26.0f,
              std::cos(time*0.39f + phase)*65.0f + wind[1]*wind[2]*25.0f}},
            0.12f + 0.88f * smooth(-0.1f, 0.8f, std::sin(time*(0.8f+random(seed+2)) + phase))};
}
}
