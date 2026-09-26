#pragma once

#include <mods/svc/camera.h>

#include <cstdint>

namespace midnafx::camera_probe {
struct Snapshot {
    bool registered = false;
    bool modifier_supported = false;
    bool modifier_registered = false;
    bool modifier_active = false;
    bool chase_modifier_supported = false;
    bool chase_modifier_registered = false;
    bool chase_modifier_active = false;
    bool target_supported = false;
    bool target_valid = false;
    bool valid = false;
    float native_fovy = 0.0f;
    float effective_fovy = 0.0f;
    float observed_fovy = 0.0f;
    float aspect = 0.0f;
    float near_plane = 0.0f;
    float far_plane = 0.0f;
    float eye[3]{};
    float target[3]{};
    float focus_distance = 0.0f;
    double callback_us = 0.0;
    std::uint64_t samples = 0;
    std::uint64_t modifier_samples = 0;
    std::uint64_t chase_modifier_samples = 0;
    std::uint32_t context_flags = 0;
    std::int32_t camera_type = 0;
    std::int32_t camera_mode = 0;
    float native_latitude_far = 0.0f;
    float native_latitude_near = 0.0f;
    float latitude_offset = 0.0f;
};

void initialize();
void shutdown();
Snapshot snapshot();
bool latest_camera_info(CameraInfo& out);
bool latest_focus_distance(float& out);
} // namespace midnafx::camera_probe
