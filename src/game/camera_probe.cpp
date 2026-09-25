#include "camera_probe.hpp"

#include "camera_fov.hpp"
#include "services.hpp"
#include "ui/settings.hpp"

#include <mods/svc/camera.h>
#include <mods/svc/gfx.h>

#include <chrono>
#include <cmath>
#include <cstdio>

namespace midnafx::camera_probe {
namespace {
GfxStageHookHandle hook = 0;
CameraFovModifierHandle modifier = 0;
Snapshot current;
std::chrono::steady_clock::time_point last_sample{};
std::uint64_t last_ticks = 0;
float blend = 0.0f;
bool log_initialized = false;
bool logged_active = false;
bool logged_endpoint = false;
std::int32_t logged_type = 0;
std::int32_t logged_mode = 0;

bool modify_fov(ModContext*, CameraFovModifierState* state, void*) {
    if (!state || state->struct_size < sizeof(CameraFovModifierState))
        return false;
    current.modifier_samples++;
    current.context_flags = state->context_flags;
    current.camera_type = state->camera_type;
    current.camera_mode = state->camera_mode;
    current.native_fovy = state->native_fovy;
    current.effective_fovy = state->native_fovy;
    const auto report = [&](bool active) {
        const bool endpoint = active && blend >= 1.0f;
        if (!settings::diagnostics_enabled()) {
            log_initialized = false;
            return;
        }
        if (!svc_log || (log_initialized && active == logged_active && endpoint == logged_endpoint &&
             state->camera_type == logged_type && state->camera_mode == logged_mode))
            return;
        char message[208];
        std::snprintf(message, sizeof(message),
                      "Camera FOV: type=%d mode=%d flags=0x%02x active=%s native=%.2f "
                      "effective=%.2f",
                      state->camera_type, state->camera_mode, state->context_flags,
                      active ? "yes" : "no", state->native_fovy, current.effective_fovy);
        svc_log->info(mod_ctx, message);
        log_initialized = true;
        logged_active = active;
        logged_endpoint = endpoint;
        logged_type = state->camera_type;
        logged_mode = state->camera_mode;
    };

    const bool safe = (state->context_flags & CAMERA_FOV_CONTEXT_CAN_MODIFY) != 0 &&
                      (state->context_flags & CAMERA_FOV_CONTEXT_NORMAL_MODE) != 0;
    const bool enabled = settings::camera_enabled();
    const std::uint64_t tick_delta = last_ticks != 0 && state->ticks > last_ticks
                                         ? state->ticks - last_ticks
                                         : 1u;
    last_ticks = state->ticks;
    if (!safe || !enabled) {
        blend = 0.0f;
        current.modifier_active = false;
        report(false);
        return false;
    }

    blend = camera_fov::advance(blend, 1.0f, tick_delta, settings::camera_transition_seconds());
    const float requested_scale = settings::camera_fov_scale();
    const float scale = 1.0f + (requested_scale - 1.0f) * blend;
    const float fovy = camera_fov::scale_vertical(state->native_fovy, scale);
    if (!std::isfinite(fovy) || std::abs(fovy - state->native_fovy) < 0.001f) {
        current.modifier_active = false;
        report(false);
        return false;
    }
    state->fovy = fovy;
    current.effective_fovy = fovy;
    current.modifier_active = true;
    report(true);
    return true;
}

void observe(ModContext*, const GfxStageContext* context, void*) {
    if (!context || context->stage != GFX_STAGE_SCENE_BEGIN || !context->game_view || !svc_camera)
        return;
    const auto start = std::chrono::steady_clock::now();
    CameraInfo info = CAMERA_INFO_INIT;
    if (svc_camera->get_camera(mod_ctx, context->game_view, &info) == MOD_OK) {
        current.valid = true;
        current.observed_fovy = info.fovy;
        if (!current.modifier_registered) {
            current.native_fovy = info.fovy;
            current.effective_fovy = info.fovy;
        }
        current.aspect = info.aspect;
        current.near_plane = info.near_plane;
        current.far_plane = info.far_plane;
        for (unsigned i = 0; i < 3; ++i)
            current.eye[i] = info.eye[i];
        last_sample = std::chrono::steady_clock::now();
        ++current.samples;
    } else
        current.valid = false;
    current.callback_us =
        std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
}
} // namespace

void initialize() {
    current = {};
    last_sample = {};
    hook = 0;
    modifier = 0;
    last_ticks = 0;
    blend = 0.0f;
    log_initialized = false;
    if (!svc_gfx || !svc_camera)
        return;
    GfxStageHookDesc desc = GFX_STAGE_HOOK_DESC_INIT;
    desc.callback = observe;
    current.registered =
        svc_gfx->register_stage_hook(mod_ctx, GFX_STAGE_SCENE_BEGIN, &desc, &hook) == MOD_OK;
    current.modifier_supported =
        SERVICE_HAS(svc_camera, CameraService, unregister_fov_modifier) &&
        svc_camera->register_fov_modifier && svc_camera->unregister_fov_modifier;
    if (current.modifier_supported) {
        CameraFovModifierDesc modifier_desc = CAMERA_FOV_MODIFIER_DESC_INIT;
        modifier_desc.debug_name = "MidnaFX exploration FOV";
        modifier_desc.modify = modify_fov;
        current.modifier_registered =
            svc_camera->register_fov_modifier(mod_ctx, &modifier_desc, &modifier) == MOD_OK;
    }
}

void shutdown() {
    if (modifier && svc_camera &&
        SERVICE_HAS(svc_camera, CameraService, unregister_fov_modifier) &&
        svc_camera->unregister_fov_modifier)
        (void)svc_camera->unregister_fov_modifier(mod_ctx, modifier);
    if (hook && svc_gfx)
        (void)svc_gfx->unregister_stage_hook(mod_ctx, hook);
    modifier = 0;
    hook = 0;
    current = {};
    last_sample = {};
    last_ticks = 0;
    blend = 0.0f;
    log_initialized = false;
}

Snapshot snapshot() {
    auto result = current;
    if (result.valid &&
        std::chrono::steady_clock::now() - last_sample > std::chrono::milliseconds(500))
        result.valid = false;
    return result;
}
} // namespace midnafx::camera_probe
