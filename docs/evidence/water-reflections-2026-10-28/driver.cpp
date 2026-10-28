#include <mods/service.hpp>
#include <mods/svc/gfx.h>
#include <mods/svc/camera.h>
#include <mods/svc/log.h>
#include <d/d_com_inf_game.h>
#include <d/actor/d_a_player.h>
#include <dolphin/pad.h>
#include <m_Do/m_Do_controller_pad.h>
#include <JSystem/JUtility/JUTGamePad.h>
#include <cstdio>
#include <cstdlib>

DEFINE_MOD();
IMPORT_OPTIONAL_SERVICE_VERSION(GfxService, svc_gfx, 2);
IMPORT_OPTIONAL_SERVICE_VERSION(CameraService, svc_camera, 1);
IMPORT_SERVICE(LogService, svc_log);
namespace {
FILE* file = nullptr;
GfxStageHookHandle hook = 0;
unsigned tick = 0, waited = 0;
bool done = false;
PADStatus replay_status{};
struct RouteReplay : JUTGamePadRecordBase {
    RouteReplay() { mActive = true; }
    void getStatus(PADStatus* pad) override { *pad = replay_status; }
    void write(PADStatus*) override {}
} replay;
JUTGamePad* replay_pad = nullptr;
JUTGamePadRecordBase* previous_replay = nullptr;
void restore_replay() {
    if (replay_pad && replay_pad->mPadReplay == &replay)
        replay_pad->mPadReplay = previous_replay;
    replay_pad = nullptr;
}
const char* phase() {
    return tick < 90 ? "idle" : tick < 125 ? "run" : tick < 215 ? "rest" :
           tick < 245 ? "target" : "final";
}
void observe(ModContext*, const GfxStageContext* context, void*) {
    if (!file || done || !context || !context->game_view || !svc_camera) return;
    CameraInfo camera = CAMERA_INFO_INIT;
    const auto* player = dComIfGp_getLinkPlayer();
    if (!player || svc_camera->get_camera(mod_ctx, context->game_view, &camera) != MOD_OK) return;
    PADStatus observed[PAD_CHANMAX]{};
    PADRead(observed);
    std::fprintf(file, "%u,%s,%.6f,%.6f,%.6f,%d,%d,%d,%.6f\n", tick, phase(), player->getSpeedF(),
                 player->mMaxSpeed, camera.fovy, int(dComIfGp_event_runCheck()),
                 int(observed[0].err), int(observed[0].stickY), mDoCPd_c::getStickY(0));
    std::fflush(file);
}
}
extern "C" {
MOD_EXPORT ModResult mod_initialize(ModError*) {
    const char* path = std::getenv("MIDNAFX_MOVEMENT_ROUTE_FILE");
    if (!path || !svc_gfx || !svc_camera) return MOD_ERROR;
    file = std::fopen(path, "w");
    if (!file) return MOD_ERROR;
    std::fputs("tick,phase,speed,max_speed,fovy,event,pad_error,raw_y,game_y\n", file);
    GfxStageHookDesc desc = GFX_STAGE_HOOK_DESC_INIT;
    desc.callback = observe;
    return svc_gfx->register_stage_hook(mod_ctx, GFX_STAGE_SCENE_BEGIN, &desc, &hook);
}
MOD_EXPORT ModResult mod_update(ModError*) {
    if (done) return MOD_OK;
    if (!dComIfGp_getLinkPlayer() || dComIfGp_event_runCheck()) {
        PADClearVirtualStatus(0);
        if (++waited > 600) {
            std::fputs("# timeout\n", file); std::fflush(file); done = true;
            restore_replay();
        }
        return MOD_OK;
    }
    PADBlockInput(false);
    if (!replay_pad && mDoCPd_c::m_gamePad[0]) {
        replay_pad = mDoCPd_c::m_gamePad[0];
        previous_replay = replay_pad->mPadReplay;
        if (!previous_replay) replay_pad->mPadReplay = &replay;
        std::fprintf(file, "# replay pad=%p previous=%p\n", (void*)replay_pad, (void*)previous_replay);
    }
    PADStatus pad{};
    if (tick >= 90 && tick < 125) pad.stickY = -127;
    if (tick >= 215 && tick < 245) { pad.button = PAD_TRIGGER_L; pad.triggerLeft = 255; }
    PADSetVirtualStatus(0, &pad);
    replay_status = pad;
    if (++tick >= 275) {
        PADClearVirtualStatus(0); done = true;
        restore_replay();
        std::fputs("# complete\n", file); std::fflush(file);
    }
    return MOD_OK;
}
MOD_EXPORT ModResult mod_shutdown(ModError*) {
    PADClearVirtualStatus(0);
    restore_replay();
    if (hook && svc_gfx) svc_gfx->unregister_stage_hook(mod_ctx, hook);
    if (file) std::fclose(file);
    file = nullptr; hook = 0;
    return MOD_OK;
}
}
