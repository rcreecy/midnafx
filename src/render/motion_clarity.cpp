#include "motion_clarity.hpp"
#include "motion_clarity_state.hpp"
#include "services.hpp"
#include "ui/settings.hpp"
#include <mods/svc/gfx.h>
#include <d/d_com_inf_game.h>
#include <d/d_demo.h>
#include <d/d_kankyo.h>
#include <m_Do/m_Do_graphic.h>
#include <cstdint>
#include <cstdio>

namespace midnafx::motion_clarity {
namespace {
GfxStageHookHandle begin_hook = 0, apply_hook = 0, restore_hook = 0;
RateOverride rate;
std::uint64_t reductions = 0, restorations = 0, conflicts = 0;
bool logged = false;

void restore() {
    if (!rate.active) return;
    if (rate.restore(mDoGph_gInf_c::mBlureRate)) ++restorations;
    else ++conflicts;
}
void reset_stage(ModContext*, const GfxStageContext*, void*) { restore(); }
void apply_stage(ModContext*, const GfxStageContext* context, void*) {
    // Also recover a scope from an earlier view if its frame-end hook was skipped.
    restore();
    if (!context || !context->game_view || !context->game_viewport) return;
    if (!settings::motion_clarity_enabled() || !dComIfGp_getLinkPlayer()) return;
    const auto retained = settings::motion_blur_retained_percent();
    if (retained >= 100) return;
    const GameplayContext gameplay{true, true, dComIfGp_isPauseFlag() != 0,
        dComIfGp_event_runCheck() != 0, !dDemo_c::m_object || dDemo_c::getCamera() != nullptr,
        mDoGph_gInf_c::isFade() != 0};
    // Only the rate is borrowed. The native enable flag, history latch, matrix
    // and framebuffer copies continue to belong to the game.
    if (!eligible(gameplay) || !g_env_light.is_blure) return;
    if (!rate.begin(mDoGph_gInf_c::mBlureRate, retained)) return;
    ++reductions;
    if (!logged && svc_log) {
        char message[176];
        std::snprintf(message, sizeof(message),
            "Motion clarity active: native blend alpha %u -> %u; scoped until pre-HUD, event/demo/fade bypass",
            unsigned(rate.original), unsigned(rate.applied));
        svc_log->info(mod_ctx, message);
        logged = true;
    }
}
}
void initialize() {
    rate = {}; reductions = restorations = conflicts = 0; logged = false;
    if (!svc_gfx) return;
    GfxStageHookDesc desc = GFX_STAGE_HOOK_DESC_INIT;
    desc.callback = reset_stage;
    // Register recovery before the applying hook; partial initialization cannot
    // leave a rate override without an end-of-frame restoration path.
    if (svc_gfx->register_stage_hook(mod_ctx, GFX_STAGE_SCENE_BEGIN, &desc, &begin_hook) != MOD_OK ||
        svc_gfx->register_stage_hook(mod_ctx, GFX_STAGE_FRAME_BEFORE_HUD, &desc, &restore_hook) != MOD_OK) {
        if (svc_log) svc_log->warn(mod_ctx, "Motion clarity stage hooks unavailable; native blur preserved");
        shutdown(); return;
    }
    desc.callback = apply_stage;
    if (svc_gfx->register_stage_hook(mod_ctx, GFX_STAGE_SCENE_AFTER_OPAQUE, &desc, &apply_hook) != MOD_OK) {
        if (svc_log) svc_log->warn(mod_ctx, "Motion clarity stage hooks unavailable; native blur preserved");
        shutdown();
    }
}
void shutdown() {
    if (svc_gfx && apply_hook) svc_gfx->unregister_stage_hook(mod_ctx, apply_hook);
    if (svc_gfx && restore_hook) svc_gfx->unregister_stage_hook(mod_ctx, restore_hook);
    if (svc_gfx && begin_hook) svc_gfx->unregister_stage_hook(mod_ctx, begin_hook);
    apply_hook = restore_hook = begin_hook = 0;
    restore();
    if (svc_log && reductions) {
        char message[176];
        std::snprintf(message, sizeof(message),
            "Motion clarity shutdown: reductions=%llu restorations=%llu conflicts=%llu pending=%s",
            static_cast<unsigned long long>(reductions), static_cast<unsigned long long>(restorations),
            static_cast<unsigned long long>(conflicts), rate.active ? "yes" : "no");
        svc_log->info(mod_ctx, message);
    }
}
} // namespace midnafx::motion_clarity
