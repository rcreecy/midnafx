#pragma once

#include <cstdint>

namespace midnafx::camera_probe {
struct Snapshot {
    bool registered = false;
    bool modifier_supported = false;
    bool modifier_registered = false;
    bool modifier_active = false;
    bool valid = false;
    float native_fovy = 0.0f;
    float effective_fovy = 0.0f;
    float observed_fovy = 0.0f;
    float aspect = 0.0f;
    float near_plane = 0.0f;
    float far_plane = 0.0f;
    float eye[3]{};
    double callback_us = 0.0;
    std::uint64_t samples = 0;
    std::uint64_t modifier_samples = 0;
    std::uint32_t context_flags = 0;
    std::int32_t camera_type = 0;
    std::int32_t camera_mode = 0;
};

void initialize();
void shutdown();
Snapshot snapshot();
} // namespace midnafx::camera_probe
