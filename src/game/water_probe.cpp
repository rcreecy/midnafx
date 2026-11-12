#include "water_probe.hpp"

#include "water_identity.h"

#include "services.hpp"
#include "render/ambient_occlusion.hpp"
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
#include <d/d_kankyo.h>
#include <d/d_kankyo_wether.h>
#include "game/wind.hpp"
#include <dolphin/gx/GXAurora.h>
#include <dolphin/gx/GXBump.h>
#include <dolphin/gx/GXGet.h>
#include <dolphin/gx/GXTransform.h>
#include <dolphin/gx/GXExtra.h>
#include <dolphin/gx/GXFrameBuffer.h>
#include <dolphin/gx/GXPixel.h>
#include <dolphin/gx/GXTev.h>
#include <dolphin/gx/GXTexture.h>
#include <f_op/f_op_actor_iter.h>
#include <f_op/f_op_actor_mng.h>
#include <mods/svc/hook.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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
bool frame_has_thickness_candidate = false;
bool water_expected = false;
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
std::uint64_t classified_draws = 0;
std::uint64_t stage_scan_ns = 0;
std::uint64_t stage_scan_frames = 0;
std::uint64_t thickness_captures = 0;
std::uint64_t thickness_capture_failures = 0;
std::uint64_t thickness_capture_ns = 0;
std::uint64_t thickness_packet_total = 0;
std::size_t thickness_packet_max = 0;
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
std::array<float, 4> pre_water_clip{0, 0, 1, 1};
struct SurfacePacket {
    J3DShapePacket* packet;
    std::array<float, 7> projection;
};
std::vector<SurfacePacket> pending_surface_packets;
constexpr std::size_t kMaxSurfacePackets = 64;
bool pending_surface_rejected = false;
bool surface_rejection_warned = false;

bool diagnostic_requested() {
    return settings::water_classification_diagnostic_enabled() ||
           settings::water_surface_capture_diagnostic_enabled() ||
           settings::water_thickness_diagnostic_enabled() || settings::enhanced_water_enabled();
}

bool thickness_requested() {
    return settings::water_thickness_diagnostic_enabled() || settings::enhanced_water_enabled();
}

bool diagnostic_active() { return hooks_ready && cleanup_hook != 0 && diagnostic_requested(); }

void log_summary(const char* reason) {
    if (!svc_log || (classified_draws == 0 && thickness_captures == 0))
        return;
    char message[384];
    std::snprintf(message, sizeof(message),
                  "WaterClass summary {reason=%s marked_draws=%llu active_models=%u "
                  "stage_scan_frames=%llu stage_scan_us=%llu thickness_captures=%llu "
                  "thickness_failures=%llu thickness_capture_us=%llu packets_total=%llu "
                  "packets_max=%zu}",
                  reason, static_cast<unsigned long long>(classified_draws),
                  static_cast<unsigned>(previous_classified_models.size()),
                  static_cast<unsigned long long>(stage_scan_frames),
                  static_cast<unsigned long long>(stage_scan_ns / 1000),
                  static_cast<unsigned long long>(thickness_captures),
                  static_cast<unsigned long long>(thickness_capture_failures),
                  static_cast<unsigned long long>(thickness_capture_ns / 1000),
                  static_cast<unsigned long long>(thickness_packet_total), thickness_packet_max);
    svc_log->info(mod_ctx, message);
}

void log_model(J3DModelData* data, const char* water_class, const char* role, float water_y,
               int room = -1) {
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
                      "depth=%d/%d/%d water_y=%.2f room=%d stay_room=%d}",
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
                      static_cast<double>(water_y), room,
                      static_cast<int>(dComIfGp_roomControl_getStayNo()));
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
            const bool thickness_candidate =
                index == 0 &&
                (std::strcmp(role, "primary") == 0 || std::strcmp(role, "surface") == 0);
            frame_material_identities[material] = {names ? names->getName(index) : nullptr,
                                                   thickness_candidate};
            frame_has_thickness_candidate |= thickness_candidate;
        }
    }
}

bool has_level_water_shape(J3DModelData& data, const J3DMaterial* material) {
    bool found = false;
    for (u16 index = 0; index < data.getShapeNum(); ++index) {
        auto* shape = data.getShapeNodePointer(index);
        if (!shape || shape->getMaterial() != material) continue;
        const auto& low = *shape->getMin();
        const auto& high = *shape->getMax();
        const float width = std::min(high.x - low.x, high.z - low.z);
        const float rise = high.y - low.y;
        if (!(width > 16.0f && rise >= 0.0f && rise <= 8.0f + 0.12f * width)) return false;
        found = true;
    }
    return found;
}

void inspect(J3DModel* model, const char* water_class, int room, const char* role, bool known_stage) {
    if (!diagnostic_active() || !model)
        return;
    J3DModelData* data = model->getModelData();
    if (!data)
        return;
    const bool first_this_frame = frame_models.insert(data).second;
    JUTNameTab* names = data->getMaterialName();
    bool selected = false;
    for (u16 index = 0; names && index < data->getMaterialNum(); ++index) {
        const char* name = names->getName(index);
        auto* material = data->getMaterialNodePointer(index);
        if (!material || !name) continue;
        auto decision = known_stage ? game::water::classify_stage_material(water_class, room, role, name) :
                                      game::water::StageMaterialDecision{};
        if (!decision.selected && game::water::is_water_surface_family(name)) {
            const auto* blend = material->getBlend();
            const auto* depth = material->getZMode();
            // Match the verified translucent surface state. Foam, overlays and
            // depth-writing geometry must stay out of the thickness capture.
            if (blend && depth && blend->getBlendMode() == GX_BM_BLEND &&
                blend->getSrcFactor() == GX_BL_SRCALPHA && blend->getDstFactor() == GX_BL_INVSRCALPHA &&
                depth->getCompareEnable() && depth->getFunc() == GX_LEQUAL && !depth->getUpdateEnable() &&
                (!std::string_view(name).starts_with("cc_MA03_") || has_level_water_shape(*data, material)))
                decision = {true, true};
        }
        if (decision.selected) {
            frame_materials.insert(material);
            frame_material_identities[material] = {name, decision.thickness};
            frame_has_thickness_candidate |= decision.thickness;
            selected = true;
        }
    }
    if (selected)
        frame_classified_models.insert(data);
    if ((selected || settings::water_classification_diagnostic_enabled()) &&
        first_this_frame && !previous_models.contains(data))
        log_model(data, water_class, role, model->getBaseTRMtx()[1][3], room);
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

struct StageScan {
    const char* stage;
    bool known;
};

int inspect_stage_actor(void* raw_actor, void* user) {
    if (!raw_actor)
        return 0;
    const auto& scan = *static_cast<const StageScan*>(user);
    int room = fopAcM_GetRoomNo(static_cast<fopAc_ac_c*>(raw_actor));
    if (room < 0)
        room = dComIfGp_roomControl_getStayNo();
    const s16 name = fopAcM_GetName(raw_actor);
    if (name == fpcNm_BG_e) {
        auto* actor = static_cast<daBg_c*>(raw_actor);
        for (unsigned i = 0; i < 6; ++i) {
            char role[16];
            std::snprintf(role, sizeof(role), "part-%u", i);
            inspect(actor->mBgParts[i].model, scan.stage, room, role, scan.known);
        }
    } else if (name == fpcNm_BG_OBJ_e) {
        auto* actor = static_cast<daBgObj_c*>(raw_actor);
        if (actor->field_0xcc8 >= 2) return 0;
        for (unsigned i = 0; i < 2; ++i) {
            char role[24];
            std::snprintf(role, sizeof(role), "bgobj-%u", i);
            inspect(actor->field_0x5a8[actor->field_0xcc8][i], scan.stage, room, role, scan.known);
        }
    }
    return 0;
}

void scan_stage_actors() {
    if (!diagnostic_active())
        return;
    const auto* stage = dComIfGp_getStartStageName();
    if (!stage || !*stage) return;
    StageScan scan{stage, game::water::has_stage_water(stage)};
    const auto started = std::chrono::steady_clock::now();
    (void)fopAcIt_Executor(inspect_stage_actor, &scan);
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
    if (classified && thickness_candidate && thickness_requested() && pre_water_depth_ready &&
        !pending_surface_rejected &&
        std::none_of(pending_surface_packets.begin(), pending_surface_packets.end(),
                     [&](const auto& pending) { return pending.packet == packet; })) {
        if (pending_surface_packets.size() >= kMaxSurfacePackets)
            pending_surface_rejected = true;
        else {
            SurfacePacket pending{packet, {}};
            GXGetProjectionv(pending.projection.data());
            if (pending.projection[0] != 0.0f || pending.projection[1] == 0.0f ||
                pending.projection[3] == 0.0f || pending.projection[6] == 0.0f ||
                !std::all_of(pending.projection.begin(), pending.projection.end(),
                             [](float value) { return std::isfinite(value); }))
                pending_surface_rejected = true; // Reject an unusable frame before replay.
            else
                pending_surface_packets.push_back(pending);
        }
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
    if (!classified)
        return HOOK_CONTINUE;

    // Product thickness/absorption never changes the native water draw.
    // Developer surface/classification overrides remain mutually exclusive.
    if (thickness_requested())
        return HOOK_CONTINUE;

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

void resolve_pending_surface_packets() {
    if (pending_surface_rejected) {
        pending_surface_packets.clear();
        pending_surface_rejected = false;
        water_surface_ready = false;
        ++thickness_captures;
        ++thickness_capture_failures;
        if (!surface_rejection_warned && svc_log) {
            svc_log->warn(mod_ctx,
                          "Water thickness capture rejected invalid projection or packet limit; frame bypassed");
            surface_rejection_warned = true;
        }
        return;
    }
    if (pending_surface_packets.empty())
        return;
    thickness_capture_started = std::chrono::steady_clock::now();
    water_surface = GFX_RESOLVED_TARGETS_INIT;
    water_surface_ready = false;
    if (pre_water_depth_ready && svc_gfx &&
        svc_gfx->create_pass(mod_ctx, pre_water_depth.width, pre_water_depth.height) == MOD_OK) {
        std::array<float, 7> saved_projection{};
        GXGetProjectionv(saved_projection.data());
        std::array<u32, 4> saved_scissor{};
        GXGetScissor(&saved_scissor[0], &saved_scissor[1], &saved_scissor[2], &saved_scissor[3]);
        const u32 left = static_cast<u32>(std::ceil(pre_water_clip[0] * pre_water_depth.width));
        const u32 top = static_cast<u32>(std::ceil(pre_water_clip[1] * pre_water_depth.height));
        const u32 right = static_cast<u32>(pre_water_clip[2] * pre_water_depth.width);
        const u32 bottom = static_cast<u32>(pre_water_clip[3] * pre_water_depth.height);
        for (const auto& pending : pending_surface_packets) {
            auto* packet = pending.packet;
            J3DShape* shape = packet ? packet->getShape() : nullptr;
            J3DMaterial* material = shape ? shape->getMaterial() : nullptr;
            if (!packet || !shape || !material)
                continue;
            // Replay occurs after native 2D/fade setup has changed projection.
            // Each water draw must use the projection observed at its original draw.
            // The replay is untextured. Loading the source material would only rebind its
            // textures/NBT scale and can reference a stale J3D texture table at this stage.
            shape->loadPreDrawSetting();
            constexpr GXColor white{255, 255, 255, 255};
            GXSetNumIndStages(0);
            GXSetNumTevStages(1);
            GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);
            GXSetTevColor(GX_TEVREG0, white);
            GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_C0);
            GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_A0);
            GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE,
                            GX_TEVPREV);
            GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE,
                            GX_TEVPREV);
            GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
            GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
            GXSetColorUpdate(GX_TRUE);
            GXSetAlphaUpdate(GX_TRUE);
            GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
            if (packet->getDisplayListObj())
                packet->getDisplayListObj()->callDL();
            GXSetProjectionv(pending.projection.data());
            // The source geometry extends behind the native letterbox trim.
            // Keep those pixels out of the mask so the final water pass cannot recolor bars.
            GXSetScissorRender(left, top, right > left ? right - left : 0,
                              bottom > top ? bottom - top : 0);
            ShapeDrawFast::g_orig(packet);
        }

        GfxResolveDesc request = GFX_RESOLVE_DESC_INIT;
        request.color = true;
        request.depth = true;
        water_surface_ready =
            svc_gfx->resolve_pass(mod_ctx, &request, &water_surface) == MOD_OK &&
            water_surface.color && water_surface.depth &&
            water_surface.width == pre_water_depth.width &&
            water_surface.height == pre_water_depth.height;
        GXSetProjectionv(saved_projection.data());
        GXSetScissor(saved_scissor[0], saved_scissor[1], saved_scissor[2], saved_scissor[3]);
    }
    const auto packet_count = pending_surface_packets.size();
    thickness_packet_total += packet_count;
    thickness_packet_max = std::max(thickness_packet_max, packet_count);
    pending_surface_packets.clear();
    if (water_surface_ready && !thickness_capture_logged && svc_log) {
        char message[192];
        std::snprintf(message, sizeof(message),
                      "Water thickness surface capture ready {size=%ux%u packets=%zu mask=color depth=R32F}",
                      water_surface.width, water_surface.height, packet_count);
        svc_log->info(mod_ctx, message);
        thickness_capture_logged = true;
    } else if (!water_surface_ready && !thickness_capture_warned && svc_log) {
        svc_log->warn(mod_ctx,
                      "Water thickness surface capture unavailable; diagnostic failed closed");
        thickness_capture_warned = true;
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
    // Slot reuse can reverse hook order after reload. Compose explicitly so
    // refracted scene color includes contact shading exactly once per scene.
    render::ambient_occlusion::apply(context);
    pre_water_capture_ready = false;
    pre_water_depth_ready = false;
    water_surface_ready = false;
    pending_surface_packets.clear();
    pending_surface_rejected = false;
    if (!context || context->stage != GFX_STAGE_SCENE_AFTER_OPAQUE ||
        (!settings::water_surface_capture_diagnostic_enabled() &&
         !thickness_requested()))
        return;
    const auto* viewport = static_cast<const view_port_class*>(context->game_viewport);
    if (!viewport || viewport->x_orig != 0 || viewport->y_orig != 0 ||
        !std::isfinite(viewport->width) || !std::isfinite(viewport->height) ||
        viewport->width <= 0 || viewport->height <= 0) return;
    const auto& clip = viewport->scissor;
    const std::array<float, 4> edges{clip.x_orig, clip.y_orig,
                                    clip.x_orig + clip.width, clip.y_orig + clip.height};
    if (!std::all_of(edges.begin(), edges.end(), [](float value) { return std::isfinite(value); })) return;
    pre_water_clip = {std::clamp(edges[0] / viewport->width, 0.0f, 1.0f),
                      std::clamp(edges[1] / viewport->height, 0.0f, 1.0f),
                      std::clamp(edges[2] / viewport->width, 0.0f, 1.0f),
                      std::clamp(edges[3] / viewport->height, 0.0f, 1.0f)};
    if (pre_water_clip[0] >= pre_water_clip[2] || pre_water_clip[1] >= pre_water_clip[3]) return;
    // Presentation can run between 30 Hz simulation ticks. Refresh stage-model
    // identity here so interpolated frames do not lose exact water classification.
    scan_stage_actors();
    // Avoid a scene-color/depth resolve where no exact surface can consume it.
    // Dynamic actor water becomes eligible after its first native frame; static
    // stage water is discovered by the render-cadence scan above.
    if (!water_expected && !frame_has_thickness_candidate)
        return;
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
        request.color = settings::enhanced_water_enabled();
        request.depth = true;
        pre_water_depth = GFX_RESOLVED_TARGETS_INIT;
        pre_water_depth_ready =
            svc_gfx->resolve_pass(mod_ctx, &request, &pre_water_depth) == MOD_OK &&
            pre_water_depth.depth && pre_water_depth.width != 0 && pre_water_depth.height != 0;
        if (!thickness_depth_logged && svc_log) {
            char message[224];
            std::snprintf(
                message, sizeof(message),
                "Water thickness pre-depth {ready=%s requested=%ux%u resolved=%ux%u color=%s depth=%s}",
                pre_water_depth_ready ? "yes" : "no", width, height, pre_water_depth.width,
                pre_water_depth.height, pre_water_depth.color ? "yes" : "no",
                pre_water_depth.depth ? "yes" : "no");
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
    pending_surface_packets.clear();
    pending_surface_rejected = false;
    frame_materials.clear();
    frame_material_identities.clear();
    previous_models = frame_models;
    frame_models.clear();
    previous_classified_models = frame_classified_models;
    frame_classified_models.clear();
    water_expected = frame_has_thickness_candidate;
    frame_has_thickness_candidate = false;
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
}
} // namespace

void initialize() {
    frame_materials.clear();
    frame_material_identities.clear();
    frame_models.clear();
    previous_models.clear();
    frame_classified_models.clear();
    previous_classified_models.clear();
    frame_has_thickness_candidate = false;
    water_expected = false;
    classified_draws = 0;
    stage_scan_ns = 0;
    stage_scan_frames = 0;
    thickness_captures = 0;
    thickness_capture_failures = 0;
    thickness_capture_ns = 0;
    thickness_packet_total = 0;
    thickness_packet_max = 0;
    thickness_capture_started = {};
    diagnostic_was_enabled = false;
    surface_capture_logged = false;
    surface_sample_logged = false;
    thickness_capture_logged = false;
    thickness_capture_warned = false;
    thickness_depth_logged = false;
    thickness_target_logged = false;
    surface_rejection_warned = false;
    cleanup_hook = 0;
    capture_hook = 0;
    pre_water_depth = GFX_RESOLVED_TARGETS_INIT;
    water_surface = GFX_RESOLVED_TARGETS_INIT;
    pre_water_depth_ready = false;
    water_surface_ready = false;
    pending_surface_packets.clear();
    pending_surface_rejected = false;
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
        thickness_packet_total = 0;
        thickness_packet_max = 0;
        frame_materials.clear();
        frame_material_identities.clear();
        frame_classified_models.clear();
        frame_has_thickness_candidate = false;
        water_expected = false;
    }
    diagnostic_was_enabled = enabled;
}

void finalize_thickness_capture() { resolve_pending_surface_packets(); }

bool latest_thickness_inputs(ThicknessInputs& out) {
    out = {};
    if (!thickness_requested() || !pre_water_depth_ready || !water_surface_ready ||
        !pre_water_depth.depth || !water_surface.depth ||
        !water_surface.color || pre_water_depth.width != water_surface.width ||
        pre_water_depth.height != water_surface.height)
        return false;
    out.scene_depth = pre_water_depth.depth;
    out.scene_color = pre_water_depth.color;
    out.surface_depth = water_surface.depth;
    out.surface_mask = water_surface.color;
    out.width = water_surface.width;
    out.height = water_surface.height;
    return true;
}

bool latest_environment_light(float out_position[3]) {
    if (!out_position)
        return false;
    const auto& position = g_env_light.base_light.mPosition;
    if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
        !std::isfinite(position.z))
        return false;
    out_position[0] = position.x;
    out_position[1] = position.y;
    out_position[2] = position.z;
    return true;
}

Environment environment() {
    Environment out;
    const bool outdoor = dKy_Outdoor_check();
    const auto wind_state = wind::sample();
    std::copy_n(wind_state.begin(), 3, out.motion);
    out.motion[3] = outdoor ? std::lerp(0.34f, 1.0f, wind_state[2]) : 0.42f;
    if (!outdoor || g_env_light.hide_vrbox || dKy_camera_water_in_status_check()) return out;

    const auto channel = [](float value) { return std::clamp(value / 255.0f, 0.0f, 1.0f); };
    const auto sky = g_env_light.vrbox_sky_col;
    const auto horizon = g_env_light.vrbox_kasumi_outer_col;
    if (sky.r || sky.g || sky.b || horizon.r || horizon.g || horizon.b) {
        out.sky[0] = channel(sky.r); out.sky[1] = channel(sky.g); out.sky[2] = channel(sky.b);
        out.sky[3] = 1.0f;
        const auto edge = (horizon.r || horizon.g || horizon.b) ? horizon : sky;
        out.horizon[0] = channel(edge.r); out.horizon[1] = channel(edge.g); out.horizon[2] = channel(edge.b);
    }
    const auto* sun = g_env_light.mpSunPacket;
    if (g_env_light.mSunInitialized && sun && std::isfinite(sun->mSunAlpha) &&
        std::isfinite(sun->mPos[0].x) && std::isfinite(sun->mPos[0].y) && std::isfinite(sun->mPos[0].z)) {
        out.sun_position[0] = sun->mPos[0].x;
        out.sun_position[1] = sun->mPos[0].y;
        out.sun_position[2] = sun->mPos[0].z;
        out.sun_color[0] = channel(sun->mColor.r);
        out.sun_color[1] = channel(sun->mColor.g);
        out.sun_color[2] = channel(sun->mColor.b);
        out.sun_color[3] = std::clamp(sun->mSunAlpha, 0.0f, 1.0f);
    }
    return out;
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
    frame_has_thickness_candidate = false;
    water_expected = false;
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
    pending_surface_packets.clear();
    pending_surface_rejected = false;
    release_pre_water_capture();
}
} // namespace midnafx::water_probe
