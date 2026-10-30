#include <mods/service.hpp>
#include <mods/svc/log.h>
#include <d/d_com_inf_game.h>
#include <d/d_kankyo.h>
#include <f_op/f_op_actor.h>
#include <algorithm>
#include <cstdio>

DEFINE_MOD();
IMPORT_SERVICE(LogService, svc_log);
namespace {
LIGHT_INFLUENCE light{};
bool placed = false;
unsigned frames = 0;
bool registered() {
    return std::find(std::begin(g_env_light.pointlight),std::end(g_env_light.pointlight),&light) !=
           std::end(g_env_light.pointlight);
}
}
extern "C" {
MOD_EXPORT ModResult mod_initialize(ModError*) { return MOD_OK; }
MOD_EXPORT ModResult mod_update(ModError*) {
    auto* player = dComIfGp_getPlayer(0);
    if (!player || dComIfGp_isPauseFlag() || dComIfGp_event_runCheck() || ++frames < 60) return MOD_OK;
    if (!placed) {
        light.mPosition = player->current.pos;
        light.mPosition.x -= 100;
        light.mPosition.y += 140;
        light.mPosition.z += 40;
        light.mColor = {188,102,66,255};
        light.mPow = 500;
        light.mFluctuation = 1;
        placed = true;
    }
    if (!registered()) {
        light.mIndex = 0;
        dKy_plight_set(&light);
        char message[180];
        std::snprintf(message,sizeof(message),"Audit ordinary light: index=%d world=(%.1f,%.1f,%.1f) RGB=188,102,66 radius=500",
            light.mIndex,light.mPosition.x,light.mPosition.y,light.mPosition.z);
        svc_log->info(mod_ctx,message);
    }
    return MOD_OK;
}
MOD_EXPORT ModResult mod_shutdown(ModError*) {
    if (registered()) dKy_plight_cut(&light);
    svc_log->info(mod_ctx,"Audit ordinary light retired");
    placed = false;
    return MOD_OK;
}
}
