#pragma once

#include <webgpu/webgpu.h>

namespace midnafx::water_probe {
struct ThicknessInputs {
    WGPUTextureView scene_depth = nullptr;
    WGPUTextureView surface_depth = nullptr;
    WGPUTextureView surface_mask = nullptr;
    unsigned width = 0;
    unsigned height = 0;
};

void initialize();
void update();
bool latest_thickness_inputs(ThicknessInputs& out);
void shutdown();
} // namespace midnafx::water_probe
