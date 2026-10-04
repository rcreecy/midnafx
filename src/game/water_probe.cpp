#include "water_probe.hpp"

#include "services.hpp"
#include "ui/settings.hpp"

#include <JSystem/J3DGraphAnimator/J3DModel.h>
#include <JSystem/J3DGraphAnimator/J3DModelData.h>
#include <JSystem/J3DGraphBase/J3DMaterial.h>
#include <JSystem/J3DGraphBase/J3DPacket.h>
#include <JSystem/JUtility/JUTNameTab.h>
#include <m_Do/m_Do_ext.h>
#include <m_Do/m_Do_graphic.h>
#include <d/actor/d_a_bg.h>
#include <d/actor/d_a_bg_obj.h>
#include <d/actor/d_a_obj_groundwater.h>
#include <d/actor/d_a_obj_lv3Water.h>
#include <d/actor/d_a_obj_lv3Water2.h>
#include <d/actor/d_a_obj_lv3WaterB.h>
#include <d/actor/d_a_obj_onsen.h>
#include <d/actor/d_a_obj_rstair.h>
#include <dolphin/gx/GXAurora.h>
#include <dolphin/gx/GXBump.h>
#include <dolphin/gx/GXExtra.h>
#include <dolphin/gx/GXFrameBuffer.h>
#include <dolphin/gx/GXPixel.h>
#include <dolphin/gx/GXTev.h>
#include <dolphin/gx/GXTexture.h>
#include <f_op/f_op_actor_iter.h>
#include <f_op/f_op_actor_mng.h>
#include <mods/svc/hook.hpp>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <unordered_set>

namespace midnafx::water_probe {
namespace {
DEFINE_HOOK_SYMBOL("src/d/actor/d_a_obj_lv3Water.cpp#daLv3Water_c::Draw", int(daLv3Water_c*),
                   Lv3WaterDraw);
DEFINE_HOOK_SYMBOL("src/d/actor/d_a_obj_lv3Water2.cpp#daLv3Water2_c::Draw", int(daLv3Water2_c*),
                   Lv3Water2Draw);
DEFINE_HOOK_SYMBOL("src/d/actor/d_a_obj_groundwater.cpp#daGrdWater_c::Draw", int(daGrdWater_c*),
                   GroundWaterDraw);
DEFINE_HOOK_SYMBOL("src/d/actor/d_a_obj_onsen.cpp#daObjOnsen_c::Draw", int(daObjOnsen_c*),
                   OnsenDraw);
DEFINE_HOOK_SYMBOL("src/d/actor/d_a_obj_rstair.cpp#daObjRotStair_c::Draw", int(daObjRotStair_c*),
                   RotStairDraw);
DEFINE_HOOK_SYMBOL("src/d/actor/d_a_obj_lv3WaterB.cpp#daObj_Lv3waterB_Draw",
                   int(obj_lv3WaterB_class*), Lv3BossWaterDraw);
DEFINE_HOOK(&J3DShapePacket::drawFast, ShapeDrawFast);

std::unordered_set<J3DMaterial*> frame_materials;
struct MaterialIdentity {
    const char* name = nullptr;
    bool thickness_candidate = false;
};
std::unordered_map<J3DMaterial*, MaterialIdentity> frame_material_identities;
std::unordered_set<J3DModelData*> frame_models;
std::unordered_set<J3DModelData*> previous_models;
std::unordered_set<J3DModelData*> frame_classified_models;
std::unordered_set<J3DModelData*> previous_classified_models;
GfxStageHookHandle cleanup_hook = 0;
GfxStageHookHandle capture_hook = 0;
bool hooks_ready = false;
bool lv3_water_hook = false;
bool lv3_water2_hook = false;
bool ground_water_hook = false;
bool onsen_hook = false;
bool rot_stair_hook = false;
bool boss_water_hook = false;
bool shape_hook = false;
bool shape_post_hook = false;
std::uint64_t classified_draws = 0;
std::uint64_t stage_scan_ns = 0;
std::uint64_t stage_scan_frames = 0;
std::uint64_t thickness_captures = 0;
std::uint64_t thickness_capture_failures = 0;
std::uint64_t thickness_capture_ns = 0;
std::chrono::steady_clock::time_point thickness_capture_started{};
bool diagnostic_was_enabled = false;
bool surface_capture_logged = false;
bool surface_sample_logged = false;
bool thickness_capture_logged = false;
bool thickness_capture_warned = false;
bool thickness_depth_logged = false;
bool thickness_target_logged = false;
alignas(32) unsigned char pre_water_capture_key[32]{};
GXTexObj pre_water_capture_texture{};
u16 pre_water_capture_width = 0;
u16 pre_water_capture_height = 0;
bool pre_water_capture_ready = false;
GfxResolvedTargets pre_water_depth = GFX_RESOLVED_TARGETS_INIT;
GfxResolvedTargets water_surface = GFX_RESOLVED_TARGETS_INIT;
bool pre_water_depth_ready = false;
bool water_surface_ready = false;
bool mask_capture_active = false;
bool mask_capture_consumed = false;
enum class CaptureOwner { None, Shape };
CaptureOwner capture_owner = CaptureOwner::None;

bool diagnostic_requested() {
    return settings::water_classification_diagnostic_enabled() ||
           settings::water_surface_capture_diagnostic_enabled() ||
           settings::water_thickness_diagnostic_enabled() || settings::enhanced_water_enabled();
}

bool thickness_requested() {
    return settings::water_thickness_diagnostic_enabled() || settings::enhanced_water_enabled();
}

bool diagnostic_active() { return hooks_ready && cleanup_hook != 0 && diagnostic_requested(); }

bool is_stage_water_candidate(const char* stage);

bool is_allowed_stage_material(const char* stage, const char* name) {
    if (!stage || !name)
        return false;
    if (std::strcmp(stage, "F_SP115") == 0)
        return std::strcmp(name, "cc_MA06_NigoriWater_v_x") == 0 ||
               std::strcmp(name, "cd_MA09_MeraWater_v_x") == 0;
    if (std::strcmp(stage, "F_SP127") == 0)
        return std::strcmp(name, "cc_MA02_IndirectWater_v") == 0 ||
               std::strcmp(name, "cc_MA06_Nigori_Water_v_x") == 0 ||
               std::strcmp(name, "cc_MA09_Nigori_Water_v") == 0;
    return false;
}

void log_summary(const char* reason) {
    if (!svc_log || (classified_draws == 0 && thickness_captures == 0))
        return;
    char message[320];
    std::snprintf(message, sizeof(message),
                  "WaterClass summary {reason=%s marked_draws=%llu active_models=%u "
                  "stage_scan_frames=%llu stage_scan_us=%llu thickness_captures=%llu "
                  "thickness_failures=%llu thickness_capture_us=%llu}",
                  reason, static_cast<unsigned long long>(classified_draws),
                  static_cast<unsigned>(previous_classified_models.size()),
                  static_cast<unsigned long long>(stage_scan_frames),
                  static_cast<unsigned long long>(stage_scan_ns / 1000),
                  static_cast<unsigned long long>(thickness_captures),
                  static_cast<unsigned long long>(thickness_capture_failures),
                  static_cast<unsigned long long>(thickness_capture_ns / 1000));
    svc_log->info(mod_ctx, message);
}

void log_model(J3DModelData* data, const char* water_class, const char* role, float water_y) {
    if (!data || !svc_log)
        return;
    JUTNameTab* names = data->getMaterialName();
    for (u16 index = 0; index < data->getMaterialNum(); ++index) {
        J3DMaterial* material = data->getMaterialNodePointer(index);
        if (!material)
            continue;
        J3DBlend* blend = material->getBlend();
        J3DZMode* depth = material->getZMode();
        const char* name = names ? names->getName(index) : nullptr;
        char message[448];
        std::snprintf(message, sizeof(message),
                      "WaterClass {class=%.40s role=%.24s model=%p material=%u name=%.80s "
                      "shapes=%u materials=%u texgens=%u tev_stages=%u blend=%d/%d/%d "
                      "depth=%d/%d/%d water_y=%.2f}",
                      water_class, role, static_cast<void*>(data), static_cast<unsigned>(index),
                      name ? name : "<unnamed>", static_cast<unsigned>(data->getShapeNum()),
                      static_cast<unsigned>(data->getMaterialNum()),
                      static_cast<unsigned>(material->getTexGenNum()),
                      static_cast<unsigned>(material->getTevStageNum()),
                      blend ? static_cast<int>(blend->getBlendMode()) : -1,
                      blend ? static_cast<int>(blend->getSrcFactor()) : -1,
                      blend ? static_cast<int>(blend->getDstFactor()) : -1,
                      depth ? static_cast<int>(depth->getCompareEnable()) : -1,
                      depth ? static_cast<int>(depth->getFunc()) : -1,
                      depth ? static_cast<int>(depth->getUpdateEnable()) : -1,
                      static_cast<double>(water_y));
        svc_log->info(mod_ctx, message);
    }
}

void classify(J3DModel* model, const char* water_class, const char* role, float water_y) {
    if (!diagnostic_active() || !model)
        return;
    J3DModelData* data = model->getModelData();
    if (!data)
        return;
    const bool first_this_frame = frame_models.insert(data).second;
    if (first_this_frame && !previous_models.contains(data))
        log_model(data, water_class, role, water_y);
    frame_classified_models.insert(data);
    JUTNameTab* names = data->getMaterialName();
    for (u16 index = 0; index < data->getMaterialNum(); ++index) {
        if (J3DMaterial* material = data->getMaterialNodePointer(index)) {
            frame_materials.insert(material);
            frame_material_identities[material] = {
                names ? names->getName(index) : nullptr,
                index == 0 &&
                    (std::strcmp(role, "primary") == 0 || std::strcmp(role, "surface") == 0),
            };
        }
    }
}

void inspect(J3DModel* model, const char* water_class, const char* role) {
    if (!diagnostic_active() || !model)
        return;
    J3DModelData* data = model->getModelData();
    if (!data)
        return;
    const bool first_this_frame = frame_models.insert(data).second;
    if (first_this_frame && !previous_models.contains(data))
        log_model(data, water_class, role, model->getBaseTRMtx()[1][3]);
    JUTNameTab* names = data->getMaterialName();
    bool selected = false;
    for (u16 index = 0; names && index < data->getMaterialNum(); ++index) {
        if (is_allowed_stage_material(water_class, names->getName(index))) {
            if (J3DMaterial* material = data->getMaterialNodePointer(index)) {
                frame_materials.insert(material);
                const char* name = names->getName(index);
                frame_material_identities[material] = {
                    name,
                    name && (std::strstr(name, "MA06") != nullptr ||
                             std::strcmp(name, "cc_MA02_IndirectWater_v") == 0),
                };
                selected = true;
            }
        }
    }
    if (selected)
        frame_classified_models.insert(data);
}

HookAction on_lv3_water(ModContext*, void* args, void*, void*) {
    auto* actor = args ? mods::arg<daLv3Water_c*>(args, 0) : nullptr;
    if (actor) {
        classify(actor->mpModel1, "lakebed", "primary", actor->current.pos.y);
        classify(actor->mpModel2, "lakebed", "projected", actor->current.pos.y);
    }
    return HOOK_CONTINUE;
}

HookAction on_lv3_water2(ModContext*, void* args, void*, void*) {
    auto* actor = args ? mods::arg<daLv3Water2_c*>(args, 0) : nullptr;
    if (actor)
        classify(actor->mpModel, "lakebed-central", "projected", actor->current.pos.y);
    return HOOK_CONTINUE;
}

HookAction on_ground_water(ModContext*, void* args, void*, void*) {
    auto* actor = args ? mods::arg<daGrdWater_c*>(args, 0) : nullptr;
    if (actor) {
        classify(actor->mModel1, "ground-water", "primary", actor->current.pos.y);
        classify(actor->mModel2, "ground-water", "projected", actor->current.pos.y);
    }
    return HOOK_CONTINUE;
}

HookAction on_onsen(ModContext*, void* args, void*, void*) {
    auto* actor = args ? mods::arg<daObjOnsen_c*>(args, 0) : nullptr;
    if (actor) {
        classify(actor->mpModel[0], "hot-spring", "primary", actor->current.pos.y);
        classify(actor->mpModel[1], "hot-spring", "projected", actor->current.pos.y);
    }
    return HOOK_CONTINUE;
}

HookAction on_rot_stair(ModContext*, void* args, void*, void*) {
    auto* actor = args ? mods::arg<daObjRotStair_c*>(args, 0) : nullptr;
    if (actor && actor->mWaterModelOn) {
        classify(actor->mWaterModels[0], "rotating-stair-water", "primary", actor->current.pos.y);
        classify(actor->mWaterModels[1], "rotating-stair-water", "projected", actor->current.pos.y);
    }
    return HOOK_CONTINUE;
}

HookAction on_boss_water(ModContext*, void* args, void*, void*) {
    auto* actor = args ? mods::arg<obj_lv3WaterB_class*>(args, 0) : nullptr;
    if (actor)
        classify(actor->mpBWaterModel, "lakebed-boss", "surface", actor->current.pos.y);
    return HOOK_CONTINUE;
}

bool is_stage_water_candidate(const char* stage) {
    return stage && (std::strcmp(stage, "F_SP115") == 0 || std::strcmp(stage, "F_SP112") == 0 ||
                     std::strcmp(stage, "F_SP126") == 0 || std::strcmp(stage, "F_SP127") == 0);
}

int inspect_stage_actor(void* raw_actor, void*) {
    if (!raw_actor)
        return 0;
    const char* stage = dComIfGp_getStartStageName();
    if (!is_stage_water_candidate(stage))
        return 0;
    const s16 name = fopAcM_GetName(raw_actor);
    if (name == fpcNm_BG_e) {
        auto* actor = static_cast<daBg_c*>(raw_actor);
        for (unsigned i = 0; i < 6; ++i) {
            char role[16];
            std::snprintf(role, sizeof(role), "part-%u", i);
            inspect(actor->mBgParts[i].model, stage, role);
        }
    } else if (name == fpcNm_BG_OBJ_e) {
        auto* actor = static_cast<daBgObj_c*>(raw_actor);
        for (unsigned i = 0; i < 2; ++i) {
            char role[24];
            std::snprintf(role, sizeof(role), "bgobj-%u", i);
            inspect(actor->field_0x5a8[actor->field_0xcc8][i], stage, role);
        }
    }
    return 0;
}

void scan_stage_actors() {
    if (!diagnostic_active() || !is_stage_water_candidate(dComIfGp_getStartStageName()))
        return;
    const auto started = std::chrono::steady_clock::now();
    (void)fopAcIt_Executor(inspect_stage_actor, nullptr);
    stage_scan_ns +=
        static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                       std::chrono::steady_clock::now() - started)
                                       .count());
    ++stage_scan_frames;
}

HookAction mark_classified_shape(ModContext*, void* args, void*, void*) {
    if (!args || !diagnostic_active())
        return HOOK_CONTINUE;
    auto* packet = mods::arg<J3DShapePacket*>(args, 0);
    J3DShape* shape = packet ? packet->getShape() : nullptr;
    J3DMaterial* material = shape ? shape->getMaterial() : nullptr;
    const bool classified = material && frame_materials.contains(material);
    const auto identity = frame_material_identities.find(material);
    const bool thickness_candidate =
        identity != frame_material_identities.end() && identity->second.thickness_candidate;
    if (classified && thickness_candidate && shape_post_hook &&
        thickness_requested() && pre_water_depth_ready &&
        !mask_capture_consumed && !mask_capture_active) {
        const auto started = std::chrono::steady_clock::now();
        if (svc_gfx->create_pass(mod_ctx, pre_water_depth.width, pre_water_depth.height) == MOD_OK) {
            thickness_capture_started = started;
            mask_capture_active = true;
            mask_capture_consumed = true;
            capture_owner = CaptureOwner::Shape;
            if (!thickness_target_logged && svc_log) {
                char message[224];
                std::snprintf(message, sizeof(message),
                              "Water thickness target {material=%p name=%.96s shape=%p}",
                              static_cast<void*>(material),
                              identity != frame_material_identities.end() && identity->second.name
                                  ? identity->second.name
                                  : "<unknown>",
                              static_cast<void*>(shape));
                svc_log->info(mod_ctx, message);
                thickness_target_logged = true;
            }
        }
    }
    if (!classified)
        return HOOK_CONTINUE;

    if (mask_capture_active) {
        constexpr GXColor white{255, 255, 255, 255};
        GXSetNumIndStages(0);
        GXSetNumTevStages(1);
        GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);
        GXSetTevColor(GX_TEVREG0, white);
        GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_C0);
        GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_A0);
        GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
        GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
        GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
        GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
        GXSetColorUpdate(GX_TRUE);
        GXSetAlphaUpdate(GX_TRUE);
        GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
        return HOOK_CONTINUE;
    }

    if (settings::water_surface_capture_diagnostic_enabled() && pre_water_capture_ready) {
        GXLoadTexObj(&pre_water_capture_texture, GX_TEXMAP0);
        GXSetNumIndStages(0);
        GXSetNumTevStages(1);
        GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
        GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
        GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);
        GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
        GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
        if (!surface_sample_logged && svc_log) {
            char message[192];
            std::snprintf(
                message, sizeof(message), "Water surface capture sampled {material=%p size=%ux%u}",
                static_cast<void*>(material), pre_water_capture_width, pre_water_capture_height);
            svc_log->info(mod_ctx, message);
            surface_sample_logged = true;
        }
        ++classified_draws;
        return HOOK_CONTINUE;
    }

    if (!settings::water_classification_diagnostic_enabled())
        return HOOK_CONTINUE;

    constexpr GXColor magenta{255, 0, 255, 255};
    GXSetNumIndStages(0);
    GXSetNumTevStages(1);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);
    GXSetTevColor(GX_TEVREG0, magenta);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_C0);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_A0);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    ++classified_draws;
    return HOOK_CONTINUE;
}

void after_classified_shape(ModContext*, void* args, void*, void*) {
    if (!mask_capture_active || capture_owner != CaptureOwner::Shape || !args)
        return;
    auto* packet = mods::arg<J3DShapePacket*>(args, 0);
    J3DShape* shape = packet ? packet->getShape() : nullptr;
    J3DMaterial* material = shape ? shape->getMaterial() : nullptr;
    GfxResolveDesc request = GFX_RESOLVE_DESC_INIT;
    request.color = true;
    request.depth = true;
    water_surface = GFX_RESOLVED_TARGETS_INIT;
    water_surface_ready = svc_gfx->resolve_pass(mod_ctx, &request, &water_surface) == MOD_OK &&
                          water_surface.color && water_surface.depth &&
                          water_surface.width == pre_water_depth.width &&
                          water_surface.height == pre_water_depth.height;
    mask_capture_active = false;
    capture_owner = CaptureOwner::None;
    if (water_surface_ready && !thickness_capture_logged && svc_log) {
        char message[192];
        std::snprintf(message, sizeof(message),
                      "Water thickness surface capture ready {size=%ux%u mask=color depth=R32F}",
                      water_surface.width, water_surface.height);
        svc_log->info(mod_ctx, message);
        thickness_capture_logged = true;
    } else if (!water_surface_ready && !thickness_capture_warned && svc_log) {
        svc_log->warn(mod_ctx,
                      "Water thickness surface capture unavailable; diagnostic failed closed");
        thickness_capture_warned = true;
    }
    if (packet && material) {
        material->load();
        packet->prepareDraw();
        shape->loadPreDrawSetting();
        if (packet->getDisplayListObj())
            packet->getDisplayListObj()->callDL();
        ShapeDrawFast::g_orig(packet);
    }
    ++thickness_captures;
    if (!water_surface_ready)
        ++thickness_capture_failures;
    thickness_capture_ns +=
        static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                       std::chrono::steady_clock::now() - thickness_capture_started)
                                       .count());
}

void capture_pre_water_scene(ModContext*, const GfxStageContext* context, void*) {
    pre_water_capture_ready = false;
    pre_water_depth_ready = false;
    water_surface_ready = false;
    mask_capture_consumed = false;
    if (!context || context->stage != GFX_STAGE_SCENE_AFTER_OPAQUE ||
        (!settings::water_surface_capture_diagnostic_enabled() &&
         !thickness_requested()))
        return;
    // Presentation can run between 30 Hz simulation ticks. Refresh stage-model
    // identity here so interpolated frames do not lose exact water classification.
    scan_stage_actors();
    const u16 width = static_cast<u16>(mDoGph_gInf_c::getWidth());
    const u16 height = static_cast<u16>(mDoGph_gInf_c::getHeight());
    if (width == 0 || height == 0)
        return;
    if (settings::water_surface_capture_diagnostic_enabled() &&
        (width != pre_water_capture_width || height != pre_water_capture_height)) {
        GXDestroyTexObj(&pre_water_capture_texture);
        GXDestroyCopyTex(pre_water_capture_key);
        GXInitTexObj(&pre_water_capture_texture, pre_water_capture_key, width, height, GX_TF_RGBA8,
                     GX_CLAMP, GX_CLAMP, GX_FALSE);
        GXInitTexObjLOD(&pre_water_capture_texture, GX_LINEAR, GX_LINEAR, 0.0f, 0.0f, 0.0f,
                        GX_FALSE, GX_FALSE, GX_ANISO_1);
        pre_water_capture_width = width;
        pre_water_capture_height = height;
    }
    if (settings::water_surface_capture_diagnostic_enabled()) {
        GXSetTexCopySrc(0, 0, width, height);
        GXSetTexCopyDst(width, height, GX_TF_RGBA8, GX_FALSE);
        GXCopyTex(pre_water_capture_key, GX_FALSE);
        pre_water_capture_ready = true;
    }
    if (thickness_requested()) {
        GfxResolveDesc request = GFX_RESOLVE_DESC_INIT;
        request.color = false;
        request.depth = true;
        pre_water_depth = GFX_RESOLVED_TARGETS_INIT;
        pre_water_depth_ready =
            svc_gfx->resolve_pass(mod_ctx, &request, &pre_water_depth) == MOD_OK &&
            pre_water_depth.depth && pre_water_depth.width != 0 && pre_water_depth.height != 0;
        if (!thickness_depth_logged && svc_log) {
            char message[192];
            std::snprintf(
                message, sizeof(message),
                "Water thickness pre-depth {ready=%s requested=%ux%u resolved=%ux%u depth=%s}",
                pre_water_depth_ready ? "yes" : "no", width, height, pre_water_depth.width,
                pre_water_depth.height, pre_water_depth.depth ? "yes" : "no");
            svc_log->info(mod_ctx, message);
            thickness_depth_logged = true;
        }
    }
    if (settings::water_surface_capture_diagnostic_enabled() && !surface_capture_logged &&
        svc_log) {
        char message[160];
        std::snprintf(message, sizeof(message),
                      "Water surface capture ready {size=%ux%u format=RGBA8}", width, height);
        svc_log->info(mod_ctx, message);
        surface_capture_logged = true;
    }
}

void clear_after_frame(ModContext*, const GfxStageContext*, void*) {
    pre_water_capture_ready = false;
    pre_water_depth_ready = false;
    water_surface_ready = false;
    mask_capture_active = false;
    capture_owner = CaptureOwner::None;
    frame_materials.clear();
    frame_material_identities.clear();
    previous_models = frame_models;
    frame_models.clear();
    previous_classified_models = frame_classified_models;
    frame_classified_models.clear();
}

void release_pre_water_capture() {
    pre_water_capture_ready = false;
    if (pre_water_capture_width != 0 || pre_water_capture_height != 0) {
        GXDestroyTexObj(&pre_water_capture_texture);
        GXDestroyCopyTex(pre_water_capture_key);
        pre_water_capture_width = 0;
        pre_water_capture_height = 0;
    }
}

void update_stage_hooks() {
    if (!svc_gfx || !hooks_ready)
        return;
    const bool requested = diagnostic_requested();
    if (requested && cleanup_hook == 0) {
        GfxStageHookDesc cleanup = GFX_STAGE_HOOK_DESC_INIT;
        cleanup.callback = clear_after_frame;
        if (svc_gfx->register_stage_hook(mod_ctx, GFX_STAGE_FRAME_AFTER_HUD, &cleanup,
                                         &cleanup_hook) != MOD_OK)
            return;
    } else if (!requested && cleanup_hook != 0) {
        svc_gfx->unregister_stage_hook(mod_ctx, cleanup_hook);
        cleanup_hook = 0;
    }

    const bool capture_requested =
        settings::water_surface_capture_diagnostic_enabled() || thickness_requested();
    if (capture_requested && capture_hook == 0) {
        GfxStageHookDesc capture = GFX_STAGE_HOOK_DESC_INIT;
        capture.callback = capture_pre_water_scene;
        if (svc_gfx->register_stage_hook(mod_ctx, GFX_STAGE_SCENE_AFTER_OPAQUE, &capture,
                                         &capture_hook) != MOD_OK)
            release_pre_water_capture();
    } else if (!capture_requested && capture_hook != 0) {
        svc_gfx->unregister_stage_hook(mod_ctx, capture_hook);
        capture_hook = 0;
        release_pre_water_capture();
    }
}

template <class Hook> bool add_pre(HookPreFn callback, const char* label) {
    const ModResult result = mods::hook::add_pre<Hook>(callback);
    if (result != MOD_OK && svc_log) {
        char message[160];
        std::snprintf(message, sizeof(message), "WaterClass hook unavailable {target=%s result=%d}",
                      label, static_cast<int>(result));
        svc_log->info(mod_ctx, message);
    }
    return result == MOD_OK;
}

template <class Hook> bool add_post(HookPostFn callback, const char* label) {
    const ModResult result = mods::hook::add_post<Hook>(callback);
    if (result != MOD_OK && svc_log) {
        char message[160];
        std::snprintf(message, sizeof(message),
                      "WaterClass post-hook unavailable {target=%s result=%d}", label,
                      static_cast<int>(result));
        svc_log->info(mod_ctx, message);
    }
    return result == MOD_OK;
}

void uninstall_hooks() {
    if (svc_hook) {
        if (lv3_water_hook)
            (void)mods::hook::uninstall<Lv3WaterDraw>();
        if (lv3_water2_hook)
            (void)mods::hook::uninstall<Lv3Water2Draw>();
        if (ground_water_hook)
            (void)mods::hook::uninstall<GroundWaterDraw>();
        if (onsen_hook)
            (void)mods::hook::uninstall<OnsenDraw>();
        if (rot_stair_hook)
            (void)mods::hook::uninstall<RotStairDraw>();
        if (boss_water_hook)
            (void)mods::hook::uninstall<Lv3BossWaterDraw>();
        if (shape_hook)
            (void)mods::hook::uninstall<ShapeDrawFast>();
    }
    lv3_water_hook = lv3_water2_hook = ground_water_hook = onsen_hook = false;
    rot_stair_hook = boss_water_hook = shape_hook = false;
    shape_post_hook = false;
}
} // namespace

void initialize() {
    frame_materials.clear();
    frame_material_identities.clear();
    frame_models.clear();
    previous_models.clear();
    frame_classified_models.clear();
    previous_classified_models.clear();
    classified_draws = 0;
    stage_scan_ns = 0;
    stage_scan_frames = 0;
    thickness_captures = 0;
    thickness_capture_failures = 0;
    thickness_capture_ns = 0;
    thickness_capture_started = {};
    diagnostic_was_enabled = false;
    surface_capture_logged = false;
    surface_sample_logged = false;
    thickness_capture_logged = false;
    thickness_capture_warned = false;
    thickness_depth_logged = false;
    thickness_target_logged = false;
    cleanup_hook = 0;
    capture_hook = 0;
    pre_water_depth = GFX_RESOLVED_TARGETS_INIT;
    water_surface = GFX_RESOLVED_TARGETS_INIT;
    pre_water_depth_ready = false;
    water_surface_ready = false;
    mask_capture_active = false;
    mask_capture_consumed = false;
    capture_owner = CaptureOwner::None;
    hooks_ready = false;
    if (!svc_hook || !svc_gfx)
        return;

    lv3_water_hook = add_pre<Lv3WaterDraw>(on_lv3_water, "lakebed");
    lv3_water2_hook = add_pre<Lv3Water2Draw>(on_lv3_water2, "lakebed-central");
    ground_water_hook = add_pre<GroundWaterDraw>(on_ground_water, "ground-water");
    onsen_hook = add_pre<OnsenDraw>(on_onsen, "hot-spring");
    rot_stair_hook = add_pre<RotStairDraw>(on_rot_stair, "rotating-stair-water");
    boss_water_hook = add_pre<Lv3BossWaterDraw>(on_boss_water, "lakebed-boss");
    shape_hook = add_pre<ShapeDrawFast>(mark_classified_shape, "J3DShapePacket::drawFast");
    shape_post_hook = add_post<ShapeDrawFast>(after_classified_shape, "J3DShapePacket::drawFast");
    const unsigned classifier_count =
        static_cast<unsigned>(lv3_water_hook) + static_cast<unsigned>(lv3_water2_hook) +
        static_cast<unsigned>(ground_water_hook) + static_cast<unsigned>(onsen_hook) +
        static_cast<unsigned>(rot_stair_hook) + static_cast<unsigned>(boss_water_hook);
    hooks_ready = shape_hook && classifier_count != 0;
    if (!hooks_ready) {
        uninstall_hooks();
        if (svc_log)
            svc_log->warn(mod_ctx, "Water classification diagnostic unavailable; failed closed");
    } else if (svc_log) {
        char message[128];
        std::snprintf(message, sizeof(message),
                      "Water classification diagnostic ready (default off, classifiers=%u)",
                      classifier_count);
        svc_log->info(mod_ctx, message);
    }
    update_stage_hooks();
}

void update() {
    update_stage_hooks();
    const bool enabled = diagnostic_active();
    // Classification-only mode has no pre-water stage hook, so retain the
    // simulation-tick scan there. Capture modes scan at render cadence instead.
    if (enabled && hooks_ready && capture_hook == 0)
        scan_stage_actors();
    if (!enabled) {
        if (diagnostic_was_enabled)
            log_summary("disabled");
        classified_draws = 0;
        stage_scan_ns = 0;
        stage_scan_frames = 0;
        thickness_captures = 0;
        thickness_capture_failures = 0;
        thickness_capture_ns = 0;
        frame_materials.clear();
        frame_material_identities.clear();
        frame_classified_models.clear();
    }
    diagnostic_was_enabled = enabled;
}

bool latest_thickness_inputs(ThicknessInputs& out) {
    out = {};
    if (!thickness_requested() || !pre_water_depth_ready || !water_surface_ready ||
        !pre_water_depth.depth || !water_surface.depth ||
        !water_surface.color || pre_water_depth.width != water_surface.width ||
        pre_water_depth.height != water_surface.height)
        return false;
    out.scene_depth = pre_water_depth.depth;
    out.surface_depth = water_surface.depth;
    out.surface_mask = water_surface.color;
    out.width = water_surface.width;
    out.height = water_surface.height;
    return true;
}

void shutdown() {
    log_summary("shutdown");
    hooks_ready = false;
    frame_materials.clear();
    frame_material_identities.clear();
    frame_models.clear();
    previous_models.clear();
    frame_classified_models.clear();
    previous_classified_models.clear();
    uninstall_hooks();
    if (svc_gfx && cleanup_hook)
        svc_gfx->unregister_stage_hook(mod_ctx, cleanup_hook);
    if (svc_gfx && capture_hook)
        svc_gfx->unregister_stage_hook(mod_ctx, capture_hook);
    cleanup_hook = 0;
    capture_hook = 0;
    pre_water_depth = GFX_RESOLVED_TARGETS_INIT;
    water_surface = GFX_RESOLVED_TARGETS_INIT;
    pre_water_depth_ready = false;
    water_surface_ready = false;
    mask_capture_active = false;
    capture_owner = CaptureOwner::None;
    release_pre_water_capture();
}
} // namespace midnafx::water_probe
