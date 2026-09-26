#include "config/grade.hpp"
#include "config/presets.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#define CHECK(condition) if (!(condition)) return __LINE__

// CPU reference for apply_grading in shaders/grading.wgsl. Flat patches have
// zero detail highpass, so they exercise the preset's tonal response directly.
std::array<float, 3> shade(std::array<float, 3> input,
                           const midnafx::grade::Uniforms& u) {
    std::array<float, 3> color{
        input[0] * u.gain_r, input[1] * u.gain_g, input[2] * u.gain_b};
    for (auto& channel : color) {
        channel = std::max(channel - u.black_point, 0.0f) / (1.0f - u.black_point);
        channel = (channel - 0.5f) * u.contrast + 0.5f;
        const float above = std::max(channel - 0.65f, 0.0f);
        channel -= u.rolloff * above * above / (0.35f + above);
    }
    const float luma = color[0] * 0.2126f + color[1] * 0.7152f + color[2] * 0.0722f;
    for (auto& channel : color)
        channel = std::pow(std::max(luma + (channel - luma) * u.saturation, 0.0f),
                           u.gamma_inverse);
    return color;
}

std::array<float, 3> shade(float gray, const midnafx::grade::Uniforms& u) {
    return shade({gray, gray, gray}, u);
}

int main() {
    using namespace midnafx;
    const auto preset = presets::vivid_realism();
    grade::Controls controls;
    for (unsigned i = 0; i < grade::Count; ++i) {
        controls.value[i] = static_cast<float>(preset.values[i]) / 100.0f;
        controls.active[i] = preset.active[i];
    }
    const auto prepared = grade::prepare(controls);
    CHECK(!prepared.neutral);
    CHECK(shade(0.0f, prepared.uniforms)[0] == 0.0f);
    float previous = 0.0f;
    for (int step = 1; step <= 255; ++step) {
        const auto color = shade(static_cast<float>(step) / 255.0f, prepared.uniforms);
        CHECK(std::isfinite(color[0]) && color[0] > previous && color[0] < 1.0f);
        CHECK(std::abs(color[0] - color[1]) < 0.000001f);
        CHECK(std::abs(color[1] - color[2]) < 0.000001f);
        previous = color[0];
    }
    CHECK(shade(0.02f, prepared.uniforms)[0] >= 0.02f);
    CHECK(shade(0.5f, prepared.uniforms)[0] > 0.5f);
    CHECK(previous > 0.94f && previous < 0.98f);

    const auto skin = shade({0.45f, 0.30f, 0.20f}, prepared.uniforms);
    CHECK(std::isfinite(skin[0]) && std::isfinite(skin[1]) && std::isfinite(skin[2]));
    CHECK(skin[0] > skin[1] && skin[1] > skin[2]);
    CHECK(skin[0] - skin[2] > 0.45f - 0.20f);

    const auto foliage = shade({0.18f, 0.42f, 0.12f}, prepared.uniforms);
    CHECK(foliage[1] > foliage[0] && foliage[0] > foliage[2]);
    CHECK(foliage[1] - foliage[2] > 0.42f - 0.12f);

    const auto dark_saturated = shade({0.02f, 0.0f, 0.0f}, prepared.uniforms);
    for (const auto channel : dark_saturated)
        CHECK(std::isfinite(channel) && channel >= 0.0f);

    const auto bright_warm = shade({1.0f, 0.85f, 0.70f}, prepared.uniforms);
    CHECK(bright_warm[0] < 1.0f && bright_warm[0] > bright_warm[1] &&
          bright_warm[1] > bright_warm[2]);
    CHECK(preset.detail_enabled && preset.detail_strength > 0 && preset.detail_strength <= 15);
}
