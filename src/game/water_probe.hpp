#pragma once

#include <webgpu/webgpu.h>

namespace midnafx::water_probe {
struct ThicknessInputs {
    WGPUTextureView scene_color = nullptr;
    WGPUTextureView scene_depth = nullptr;
    WGPUTextureView surface_depth = nullptr;
    WGPUTextureView surface_mask = nullptr;
    unsigned width = 0;
    unsigned height = 0;
};
struct Environment {
    float sky[4]{};          // RGB and sky availability.
    float horizon[4]{};
    float sun_position[4]{};
    float sun_color[4]{};    // RGB and visibility.
    float motion[4]{1, 0, 0, 0.65f}; // Wind XZ, power, and water exposure.
};

void initialize();
void update();
void finalize_thickness_capture();
bool latest_thickness_inputs(ThicknessInputs& out);
bool latest_environment_light(float out_position[3]);
Environment environment();
void shutdown();
} // namespace midnafx::water_probe
