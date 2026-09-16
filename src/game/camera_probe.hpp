#pragma once

#include <cstdint>

namespace midnafx::camera_probe {
struct Snapshot {
    bool registered = false;
    bool valid = false;
    float native_fovy = 0.0f;
    float aspect = 0.0f;
    float near_plane = 0.0f;
    float far_plane = 0.0f;
    float eye[3]{};
    double callback_us = 0.0;
    std::uint64_t samples = 0;
};

void initialize();
void shutdown();
Snapshot snapshot();
} // namespace midnafx::camera_probe
