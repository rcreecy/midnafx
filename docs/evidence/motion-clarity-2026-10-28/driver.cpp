#include <mods/service.hpp>
#include <mods/svc/gfx.h>
#include <d/d_com_inf_game.h>
#include <d/d_kankyo.h>
#include <m_Do/m_Do_graphic.h>
#include <m_Do/m_Do_controller_pad.h>
#include <JSystem/JUtility/JUTGamePad.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
DEFINE_MOD();
IMPORT_OPTIONAL_SERVICE_VERSION(GfxService, svc_gfx, 2);
namespace {
FILE* file = nullptr;
GfxStageHookHandle begin_hook = 0, opaque_hook = 0, hud_hook = 0;
unsigned tick = 0;
bool done = false, saved = false;
unsigned char original_rate = 0;
Mtx original_matrix{};
PADStatus input{};
struct Replay : JUTGamePadRecordBase {
    Replay() { mActive = true; }
    void getStatus(PADStatus* out) override { *out = input; }
    void write(PADStatus*) override {}
} replay;
JUTGamePad* pad = nullptr;
JUTGamePadRecordBase* previous = nullptr;
void restore() {
    if (pad && pad->mPadReplay == &replay) pad->mPadReplay = previous;
    pad = nullptr;
    if (saved) {
        if (mDoGph_gInf_c::getBlureRate() == 160) mDoGph_gInf_c::setBlureRate(original_rate);
        std::memcpy(mDoGph_gInf_c::getBlureMtx(), original_matrix, sizeof(Mtx));
        saved = false;
    }
}
void record(const char* stage) {
    if (!file || done) return;
    std::fprintf(file, "%u,%s,%u,%u,%u,%d,%d,%u\n", tick, stage,
        unsigned(mDoGph_gInf_c::getBlureRate()), unsigned(mDoGph_gInf_c::isBlure()),
        unsigned(g_env_light.is_blure), int(dComIfGp_event_runCheck()),
        int(dComIfGp_isPauseFlag()), unsigned(mDoGph_gInf_c::isFade()));
    std::fflush(file);
}
void begin(ModContext*, const GfxStageContext*, void*) {
    if (done || !dComIfGp_getLinkPlayer()) return;
    if (!saved) {
        original_rate = mDoGph_gInf_c::getBlureRate();
        std::memcpy(original_matrix, mDoGph_gInf_c::getBlureMtx(), sizeof(Mtx));
        saved = true;
    }
    Mtx identity{}; identity[0][0] = identity[1][1] = identity[2][2] = 1;
    mDoGph_gInf_c::setBlureRate(160);
    mDoGph_gInf_c::onBlure(identity);
    record("begin");
}
void opaque(ModContext*, const GfxStageContext*, void*) { record("opaque"); }
void hud(ModContext*, const GfxStageContext*, void*) { record("hud"); }
}
extern "C" {
MOD_EXPORT ModResult mod_initialize(ModError*) {
    const char* path = std::getenv("MIDNAFX_MOTION_AUDIT_FILE");
    if (!path || !svc_gfx) return MOD_ERROR;
    file = std::fopen(path, "w"); if (!file) return MOD_ERROR;
    std::fputs("tick,stage,rate,enabled,history,event,paused,fading\n", file);
    GfxStageHookDesc desc = GFX_STAGE_HOOK_DESC_INIT;
    desc.callback = begin;
    if (svc_gfx->register_stage_hook(mod_ctx,GFX_STAGE_SCENE_BEGIN,&desc,&begin_hook) != MOD_OK) return MOD_ERROR;
    desc.callback = opaque;
    if (svc_gfx->register_stage_hook(mod_ctx,GFX_STAGE_SCENE_AFTER_OPAQUE,&desc,&opaque_hook) != MOD_OK) return MOD_ERROR;
    desc.callback = hud;
    return svc_gfx->register_stage_hook(mod_ctx,GFX_STAGE_FRAME_BEFORE_HUD,&desc,&hud_hook);
}
MOD_EXPORT ModResult mod_update(ModError*) {
    if (done || !dComIfGp_getLinkPlayer()) return MOD_OK;
    if (!pad && mDoCPd_c::m_gamePad[0] && !mDoCPd_c::m_gamePad[0]->mPadReplay) {
        pad = mDoCPd_c::m_gamePad[0]; previous = pad->mPadReplay; pad->mPadReplay = &replay;
    }
    input = {};
    if (tick >= 90 && tick < 135 && !dComIfGp_event_runCheck()) input.stickY = 127;
    if (++tick >= 390) {
        restore(); done = true; std::fputs("# complete\n", file); std::fflush(file);
    }
    return MOD_OK;
}
MOD_EXPORT ModResult mod_shutdown(ModError*) {
    restore();
    if (begin_hook) svc_gfx->unregister_stage_hook(mod_ctx,begin_hook);
    if (opaque_hook) svc_gfx->unregister_stage_hook(mod_ctx,opaque_hook);
    if (hud_hook) svc_gfx->unregister_stage_hook(mod_ctx,hud_hook);
    if (file) std::fclose(file); file = nullptr;
    return MOD_OK;
}
}
