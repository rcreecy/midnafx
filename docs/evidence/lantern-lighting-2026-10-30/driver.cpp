#include <mods/service.hpp>
#include <mods/svc/gfx.h>
#include <mods/svc/log.h>
#include <d/d_com_inf_game.h>
#include <d/d_kankyo.h>
#include <f_op/f_op_actor.h>
#include "render/local_light_state.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

DEFINE_MOD();
IMPORT_OPTIONAL_SERVICE_VERSION(GfxService,svc_gfx,2);
IMPORT_SERVICE(LogService,svc_log);
namespace {
GfxStageHookHandle begin_hook=0,end_hook=0;
BOSS_LIGHT previous{},injected{};
bool owns=false,base_ready=false,complete=false;
unsigned tick=0,last_record=~0u,restores=0,conflicts=0;
cXyz base{};
FILE* file=nullptr;
void restore() {
    if (!owns) return;
    auto& slot=g_env_light.field_0x0c18[0];
    if (std::memcmp(&slot,&injected,sizeof(slot))==0) {
        std::memcpy(&slot,&previous,sizeof(slot));
        ++restores;
    } else ++conflicts;
    owns=false;
}
void begin(ModContext*,const GfxStageContext* context,void*) {
    restore();
    auto* player=dComIfGp_getPlayer(0);
    if (!player || !context || !context->game_view || dComIfGp_isPauseFlag() || dComIfGp_event_runCheck()) return;
    if (!base_ready) { base=player->current.pos; base.y+=140; base_ready=true; }
    auto& slot=g_env_light.field_0x0c18[0];
    const unsigned phase=tick<90 ? 0 : tick<240 ? 1 : tick<300 ? 2 : 3;
    if ((phase==1 || phase==3) && slot.field_0x26==0) {
        std::memcpy(&previous,&slot,sizeof(slot));
        std::memset(&injected,0,sizeof(injected));
        injected.mPos=base;
        injected.mPos.x+=80*std::sin(float(tick)*0.035f);
        injected.mPos.z+=40;
        injected.mColor={181,112,40,255};
        injected.mRefDistance=phase==1 ? 0.25f+0.75f*float(tick-90)/150 : 1;
        injected.field_0x14=0.99f;
        injected.mCutoffAngle=50;
        injected.mAngleX=-70; injected.mAngleY=90;
        injected.mAngleAttenuation=0; injected.mDistAttenuation=3;
        injected.field_0x26=1;
        std::memcpy(&slot,&injected,sizeof(slot));
        owns=true;
    }
    if (file && tick!=last_record && tick<420) {
        const auto source=midnafx::render::local_lights::lantern_source(
            {slot.mPos.x,slot.mPos.y,slot.mPos.z},
            {float(slot.mColor.r),float(slot.mColor.g),float(slot.mColor.b)},slot.mRefDistance,
            slot.field_0x26,slot.mColor.a,slot.mAngleAttenuation,slot.mDistAttenuation);
        std::fprintf(file,"%u,%u,%u,%u,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g\n",
            tick,phase,unsigned(owns),unsigned(slot.field_0x26),slot.mRefDistance,
            slot.mPos.x,slot.mPos.y,slot.mPos.z,source.distance[2],source.radius);
        std::fflush(file); last_record=tick;
    }
    if (!complete && tick>=420) { svc_log->info(mod_ctx,"Player-light audit phases complete; source continues until shutdown"); complete=true; }
}
void end(ModContext*,const GfxStageContext*,void*) { restore(); }
}
extern "C" {
MOD_EXPORT ModResult mod_initialize(ModError*) {
    const char* path=std::getenv("MIDNAFX_LANTERN_AUDIT_FILE");
    if (!path || !svc_gfx) return MOD_ERROR;
    file=std::fopen(path,"w"); if (!file) return MOD_ERROR;
    std::fputs("tick,phase,injected,active,reference_distance,x,y,z,k2,envelope\n",file);
    GfxStageHookDesc desc=GFX_STAGE_HOOK_DESC_INIT; desc.callback=begin;
    if (svc_gfx->register_stage_hook(mod_ctx,GFX_STAGE_SCENE_BEGIN,&desc,&begin_hook)!=MOD_OK) return MOD_ERROR;
    desc.callback=end;
    return svc_gfx->register_stage_hook(mod_ctx,GFX_STAGE_FRAME_BEFORE_HUD,&desc,&end_hook);
}
MOD_EXPORT ModResult mod_update(ModError*) {
    if (dComIfGp_getPlayer(0) && !dComIfGp_isPauseFlag() && !dComIfGp_event_runCheck()) ++tick;
    return MOD_OK;
}
MOD_EXPORT ModResult mod_shutdown(ModError*) {
    restore();
    if (begin_hook) svc_gfx->unregister_stage_hook(mod_ctx,begin_hook);
    if (end_hook) svc_gfx->unregister_stage_hook(mod_ctx,end_hook);
    if (file) std::fclose(file); file=nullptr;
    char message[160];
    std::snprintf(message,sizeof(message),"Player-light audit retired: restores=%u conflicts=%u",restores,conflicts);
    svc_log->info(mod_ctx,message);
    return MOD_OK;
}
}
