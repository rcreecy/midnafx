#include "camera_probe.hpp"

#include "services.hpp"

#include <mods/svc/camera.h>
#include <mods/svc/gfx.h>

#include <chrono>

namespace midnafx::camera_probe {
namespace {
GfxStageHookHandle hook = 0;
Snapshot current;
std::chrono::steady_clock::time_point last_sample{};

void observe(ModContext*, const GfxStageContext* context, void*) {
    if (!context || context->stage != GFX_STAGE_SCENE_BEGIN || !context->game_view || !svc_camera)
        return;
    const auto start = std::chrono::steady_clock::now();
    CameraInfo info = CAMERA_INFO_INIT;
    if (svc_camera->get_camera(mod_ctx, context->game_view, &info) == MOD_OK) {
        current.valid = true;
        current.native_fovy = info.fovy;
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
    if (!svc_gfx || !svc_camera)
        return;
    GfxStageHookDesc desc = GFX_STAGE_HOOK_DESC_INIT;
    desc.callback = observe;
    current.registered =
        svc_gfx->register_stage_hook(mod_ctx, GFX_STAGE_SCENE_BEGIN, &desc, &hook) == MOD_OK;
}

void shutdown() {
    if (hook && svc_gfx)
        (void)svc_gfx->unregister_stage_hook(mod_ctx, hook);
    hook = 0;
    current = {};
    last_sample = {};
}

Snapshot snapshot() {
    auto result = current;
    if (result.valid &&
        std::chrono::steady_clock::now() - last_sample > std::chrono::milliseconds(500))
        result.valid = false;
    return result;
}
} // namespace midnafx::camera_probe
