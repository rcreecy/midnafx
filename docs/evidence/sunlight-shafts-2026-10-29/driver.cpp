#include <mods/service.hpp>
#include <mods/svc/camera.h>
#include <mods/svc/gfx.h>
#include <mods/svc/log.h>
#include <d/d_com_inf_game.h>
#include <d/d_kankyo.h>
#include <d/d_kankyo_wether.h>
#include <m_Do/m_Do_graphic.h>
#include <m_Do/m_Do_lib.h>
#include "render/sunlight_shafts_state.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>

DEFINE_MOD();
IMPORT_OPTIONAL_SERVICE_VERSION(CameraService, svc_camera, 1);
IMPORT_OPTIONAL_SERVICE_VERSION(GfxService, svc_gfx, 2);
IMPORT_SERVICE(LogService, svc_log);

namespace {
FILE* file = nullptr;
CameraOperatorHandle camera_operator = 0;
GfxStageHookHandle stage_hook = 0;
bool time_set = false, eye_set = false;
float fixed_eye[3]{};
std::uint64_t frame = 0, ticks = 0, last_frame = UINT64_MAX;
unsigned captured = 0;

bool ready() {
    return dComIfGp_getPlayer(0) && !dComIfGp_isPauseFlag() &&
        !dComIfGp_event_runCheck() && g_env_light.mSunInitialized &&
        g_env_light.mpSunPacket;
}

bool aim_at_sun(ModContext*, CameraOperatorState* state, void*) {
    if (!state || state->struct_size < sizeof(CameraOperatorState) || !time_set || !ready())
        return false;
    const auto& sun = g_env_light.mpSunPacket->mPos[0];
    if (!std::isfinite(sun.x) || !std::isfinite(sun.y) || !std::isfinite(sun.z)) return false;
    if (!eye_set) {
        if (!std::all_of(state->eye, state->eye + 3, [](float v) { return std::isfinite(v); }))
            return false;
        std::copy(state->eye, state->eye + 3, fixed_eye);
        eye_set = true;
    }
    const float dx = sun.x-fixed_eye[0], dy = sun.y-fixed_eye[1], dz = sun.z-fixed_eye[2];
    if (dx*dx+dy*dy+dz*dz < 1) return false;
    std::copy(fixed_eye, fixed_eye + 3, state->eye);
    state->center[0] = sun.x; state->center[1] = sun.y; state->center[2] = sun.z;
    if (!std::isfinite(state->fovy) || state->fovy < 10 || state->fovy > 120) state->fovy = 75;
    state->bank_degrees = 0;
    frame = state->frame_counter; ticks = state->ticks;
    return true;
}

void observe(ModContext*, const GfxStageContext* context, void*) {
    if (!file || captured >= 300 || !eye_set || !ready() || !context || !context->game_view ||
        frame == last_frame) return;
    CameraInfo camera = CAMERA_INFO_INIT;
    if (svc_camera->get_camera(mod_ctx, context->game_view, &camera) != MOD_OK) return;
    const auto* sun = g_env_light.mpSunPacket;
    const float world[]{sun->mPos[0].x, sun->mPos[0].y, sun->mPos[0].z};
    const float color[]{sun->mColor.r/255.0f, sun->mColor.g/255.0f, sun->mColor.b/255.0f};
    const unsigned width = mDoGph_gInf_c::getWidth(), height = mDoGph_gInf_c::getHeight();
    const bool outdoors = dKy_Outdoor_check();
    const bool underwater = dKy_camera_water_in_status_check();
    const bool twilight = dKy_darkworld_check();
    const bool hidden = g_env_light.hide_vrbox;
    const auto projected = midnafx::render::sunlight_shafts::prepare(
        camera.view_from_world, camera.proj_from_view, world, color, .10f, 1, 1, width, height);
    const auto prepared = midnafx::render::sunlight_shafts::prepare(
        camera.view_from_world, camera.proj_from_view, world, color,
        outdoors && !underwater && !twilight && !hidden ? .10f : 0,
        sun->mSunAlpha, sun->mVisibility, width, height);
    cXyz native_sun = sun->mPos[0], native_projection;
    mDoLib_project(&native_sun, &native_projection);
    std::fprintf(file,
        "%llu,%llu,%.6f,%.6f,%u,%u,%u,%.6f,%.6f,%d,%d,%d,%d,%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%d,%d,%u,%u,%u,%u,%u,%u,%.6f,%.6f,%.6f\n",
        static_cast<unsigned long long>(frame), static_cast<unsigned long long>(ticks),
        sun->mSunAlpha, sun->mVisibility, unsigned(sun->mColor.r), unsigned(sun->mColor.g),
        unsigned(sun->mColor.b), projected.screen[0], projected.screen[1], int(outdoors),
        int(underwater), int(dComIfGp_isPauseFlag()), int(dComIfGp_event_runCheck()),
        int(twilight), int(hidden), prepared.screen[2], camera.fovy,
        camera.eye[0], camera.eye[1], camera.eye[2], int(g_env_light.mColpatWeather),
        int(g_env_light.mSnowCount), unsigned(sun->field_0x44[0]), unsigned(sun->field_0x44[1]),
        unsigned(sun->field_0x44[2]), unsigned(sun->field_0x44[3]), unsigned(sun->field_0x44[4]),
        unsigned(sun->field_0x29), native_projection.x, native_projection.y, native_projection.z);
    last_frame = frame;
    if (++captured == 300) std::fputs("# complete\n", file);
    std::fflush(file);
}

void cleanup() {
    if (stage_hook && svc_gfx) svc_gfx->unregister_stage_hook(mod_ctx, stage_hook);
    if (camera_operator && svc_camera) svc_camera->unregister_camera_operator(mod_ctx, camera_operator);
    stage_hook = 0; camera_operator = 0;
    if (file) std::fclose(file);
    file = nullptr;
}
}

extern "C" {
MOD_EXPORT ModResult mod_initialize(ModError*) {
    const char* path = std::getenv("MIDNAFX_SUNLIGHT_AUDIT_FILE");
    if (!path || !svc_gfx || !svc_camera ||
        !SERVICE_HAS(svc_camera, CameraService, unregister_camera_operator)) return MOD_ERROR;
    file = std::fopen(path, "w");
    if (!file) return MOD_ERROR;
    std::fputs("frame,ticks,sun_alpha,visibility,red,green,blue,sun_u,sun_v,outdoors,underwater,pause,event,twilight,hidden,prepared_strength,fovy,eye_x,eye_y,eye_z,weather,snow,peek_z0,peek_z1,peek_z2,peek_z3,peek_z4,points_visible,native_sun_x,native_sun_y,native_sun_z\n", file);
    std::fflush(file);
    CameraOperatorDesc camera = CAMERA_OPERATOR_DESC_INIT;
    camera.debug_name = "Temporary native sunlight audit";
    camera.priority = 10000;
    camera.operate = aim_at_sun;
    auto result = svc_camera->register_camera_operator(mod_ctx, &camera, &camera_operator);
    if (result != MOD_OK) { cleanup(); return result; }
    GfxStageHookDesc hook = GFX_STAGE_HOOK_DESC_INIT;
    hook.callback = observe;
    result = svc_gfx->register_stage_hook(mod_ctx, GFX_STAGE_SCENE_AFTER_OPAQUE, &hook, &stage_hook);
    if (result != MOD_OK) cleanup();
    return result;
}
MOD_EXPORT ModResult mod_update(ModError*) {
    if (!time_set && dComIfGp_getPlayer(0) && !dComIfGp_isPauseFlag() && !dComIfGp_event_runCheck()) {
        dKy_instant_timechg(180);
        time_set = true;
        if (svc_log) svc_log->info(mod_ctx, "Sunlight audit: native time set to 180; camera will face native sun");
    }
    return MOD_OK;
}
MOD_EXPORT ModResult mod_shutdown(ModError*) {
    cleanup();
    return MOD_OK;
}
}

