#include "ambient_occlusion.hpp"
#include "gpu_validation.hpp"
#include "sunlight_shafts_state.hpp"
#include "local_light_state.hpp"
#include "midnafx_shader.hpp"
#include "services.hpp"
#include "ui/settings.hpp"
#include "game/water_probe.hpp"
#include <f_op/f_op_view.h>
#include <helpers/gx_helper.h>
#include <m_Do/m_Do_graphic.h>
#include <d/d_kankyo.h>
#include <d/d_kankyo_wether.h>
#include <d/d_com_inf_game.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <iterator>
#include <vector>

namespace midnafx::render::ambient_occlusion {
namespace {
struct Uniforms {
    float view_from_proj[16];
    float radius, strength, projection_y, background_depth;
    float fog_color[4];
    float fog_range[4];
    float proj_from_view[16];
    float light_position[4];
    float shadow_range[4];
    float bounce[4];
    float sun_screen[4];
    float sun_color[4];
    float local[4];
    float local_position[4][4];
    float local_color[4][4];
    float local_distance[4][4];
};
static_assert(sizeof(Uniforms) == 464);
struct Pipeline {
    GfxRenderTargetLayout layout = GFX_RENDER_TARGET_LAYOUT_INIT;
    WGPURenderPipeline pipeline = nullptr;
    WGPUBindGroupLayout bindings = nullptr;
};
struct Targets {
    std::uint32_t width = 0, height = 0;
    bool bounce = false;
    WGPUTexture textures[4]{};
    WGPUTextureView views[4]{};
    std::atomic<unsigned> pending{0};
    ~Targets() {
        for (auto view : views) if (view) wgpuTextureViewRelease(view);
        for (auto texture : textures) if (texture) wgpuTextureRelease(texture);
    }
};
struct Pending {
    Targets* targets;
    ~Pending() { if (targets) targets->pending.fetch_sub(1); }
};
struct Payload {
    const Pipeline* pipeline;
    WGPUTextureView color, depth;
    std::uint32_t uniform_offset, uniform_size;
    Targets* targets;
};
struct ComputePayload {
    WGPUTextureView depth, color;
    Targets* targets;
    std::uint32_t uniform_offset, uniform_size;
};
static_assert(sizeof(Payload) <= GFX_INLINE_DRAW_PAYLOAD_SIZE);
static_assert(sizeof(ComputePayload) <= GFX_INLINE_DRAW_PAYLOAD_SIZE);
GfxDeviceInfo device = GFX_DEVICE_INFO_INIT;
GfxStageHookHandle stage_hook = 0;
GfxStageHookHandle begin_hook = 0;
GfxDrawTypeHandle draw_type = 0;
GfxComputeTypeHandle compute_type = 0;
WGPUComputePipeline compute_pipelines[4]{};
WGPUBindGroupLayout compute_layouts[4]{};
WGPUTexture neutral_texture = nullptr;
WGPUTextureView neutral_view = nullptr;
std::vector<std::unique_ptr<Targets>> target_sets;
Targets* current_targets = nullptr;
std::atomic<bool> compute_failed{false};
std::atomic<std::uint64_t> encoded_compute{0}, encoded_draw{0};
std::vector<std::unique_ptr<Pipeline>> pipelines;
bool logged = false;
bool shadows_logged = false;
bool bounce_logged = false;
bool local_logged = false;
bool lantern_logged = false;
bool atmosphere_logged = false;
bool sunlight_logged = false;
bool submitted = false;

void collect_targets() {
    std::erase_if(target_sets, [](const auto& target) {
        return target.get() != current_targets && target->pending.load() == 0;
    });
}

Targets* ensure_targets(std::uint32_t width, std::uint32_t height, bool bounce) {
    collect_targets();
    width = (width + 1) / 2; height = (height + 1) / 2;
    if (current_targets && current_targets->width == width && current_targets->height == height &&
        current_targets->bounce == bounce)
        return current_targets;
    current_targets = nullptr;
    // Inline host payloads borrow these views. Retire after their last callback
    // encodes, not after an assumed number of frames. GPU commands retain resources.
    std::erase_if(target_sets, [](const auto& target) { return target->pending.load() == 0; });
    auto next = std::make_unique<Targets>();
    next->width = width; next->height = height;
    next->bounce = bounce;
    gpu::ValidationScopes scopes(device.device, device.instance);
    for (unsigned i = 0; i < 4; ++i) {
        WGPUTextureDescriptor desc = WGPU_TEXTURE_DESCRIPTOR_INIT;
        desc.label = {"MidnaFX half-resolution contact shading", WGPU_STRLEN};
        // Bindable dummies avoid full-sized color fields when bounce is off.
        const bool full_size = i < 2 || bounce;
        desc.size = {full_size ? width : 1u, full_size ? height : 1u, 1};
        desc.format = WGPUTextureFormat_RGBA16Float;
        desc.usage = WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding;
        next->textures[i] = wgpuDeviceCreateTexture(device.device, &desc);
        if (next->textures[i]) next->views[i] = wgpuTextureCreateView(next->textures[i], nullptr);
    }
    const bool valid = scopes.finish();
    if (!valid || !next->views[0] || !next->views[1] || !next->views[2] || !next->views[3]) {
        compute_failed.store(true);
        if (svc_log) svc_log->warn(mod_ctx, "Contact lighting targets unavailable; AO, shadows and bounce bypassed");
        return nullptr;
    }
    current_targets = next.get();
    target_sets.push_back(std::move(next));
    return current_targets;
}

void compute(ModContext*, const GfxComputeContext* context, const void* bytes, size_t size, void*) {
    if (!context || size != sizeof(ComputePayload)) return;
    ComputePayload payload{};
    std::memcpy(&payload, bytes, sizeof(payload));
    Pending pending{payload.targets};
    if (!payload.targets || !payload.depth || !payload.color || compute_failed.load()) return;
    auto& target = *payload.targets;
    WGPUBindGroup groups[4]{};
    for (unsigned i = 0; i < 4; ++i) {
        WGPUBindGroupEntry entries[6];
        for (auto& entry : entries) entry = WGPU_BIND_GROUP_ENTRY_INIT;
        unsigned count = 0;
        entries[count].binding = 0; entries[count++].textureView = payload.depth;
        if (i > 0) {
            entries[count].binding = 1; entries[count++].textureView = target.views[i == 3 ? 1 : 0];
        }
        if (i != 1) {
            entries[count].binding = 2; entries[count++].textureView = target.views[i == 2 ? 1 : 0];
        }
        entries[count].binding = 3; entries[count].buffer = context->uniform_buffer;
        entries[count].offset = payload.uniform_offset; entries[count++].size = payload.uniform_size;
        if (i == 1) {
            entries[count].binding = 4; entries[count++].textureView = payload.color;
        }
        if (i >= 2) {
            entries[count].binding = 5; entries[count++].textureView = target.views[i == 2 ? 2 : 3];
        }
        if (i >= 1) {
            entries[count].binding = 6; entries[count++].textureView = target.views[i == 2 ? 3 : 2];
        }
        WGPUBindGroupDescriptor desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
        desc.layout = compute_layouts[i]; desc.entryCount = count; desc.entries = entries;
        groups[i] = wgpuDeviceCreateBindGroup(context->device, &desc);
    }
    if (!groups[0] || !groups[1] || !groups[2] || !groups[3]) {
        for (auto group : groups) if (group) wgpuBindGroupRelease(group);
        compute_failed.store(true); return;
    }
    WGPUComputePassDescriptor desc = WGPU_COMPUTE_PASS_DESCRIPTOR_INIT;
    desc.label = {"MidnaFX filtered contact shading", WGPU_STRLEN};
    auto pass = wgpuCommandEncoderBeginComputePass(context->encoder, &desc);
    for (unsigned i = 0; i < 4; ++i) {
        if (i == 1 && !target.bounce) continue;
        wgpuComputePassEncoderSetPipeline(pass, compute_pipelines[i]);
        wgpuComputePassEncoderSetBindGroup(pass, 0, groups[i], 0, nullptr);
        wgpuComputePassEncoderDispatchWorkgroups(pass, (target.width + 7) / 8, (target.height + 7) / 8, 1);
    }
    wgpuComputePassEncoderEnd(pass);
    wgpuComputePassEncoderRelease(pass);
    for (auto group : groups) wgpuBindGroupRelease(group);
    encoded_compute.fetch_add(1);
}

bool supported(const GfxRenderTargetLayout& layout) {
    return layout.sample_count == 1 && layout.color_attachment_count >= 1 &&
        layout.color_attachments[0].semantic == GFX_ATTACHMENT_SCENE_COLOR &&
        (layout.color_attachments[0].format == WGPUTextureFormat_RGBA8Unorm ||
         layout.color_attachments[0].format == WGPUTextureFormat_BGRA8Unorm);
}
Pipeline* get_pipeline(const GfxRenderTargetLayout& layout) {
    for (auto& existing : pipelines) if (existing->layout.key == layout.key) return existing.get();
    auto next = std::make_unique<Pipeline>();
    next->layout = layout;
    gpu::ValidationScopes scopes(device.device, device.instance);
    WGPUShaderSourceWGSL source = WGPU_SHADER_SOURCE_WGSL_INIT;
    source.code = {ambient_occlusion_shader, WGPU_STRLEN};
    WGPUShaderModuleDescriptor shader_desc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    shader_desc.nextInChain = &source.chain;
    shader_desc.label = {"MidnaFX ambient occlusion", WGPU_STRLEN};
    auto shader = wgpuDeviceCreateShaderModule(device.device, &shader_desc);
    if (!shader) return nullptr;
    WGPUColorTargetState colors[GFX_MAX_COLOR_ATTACHMENTS];
    const auto count = gfx_init_color_target_states(&layout, colors, nullptr, WGPUColorWriteMask_All);
    WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
    fragment.module = shader;
    fragment.entryPoint = {"fs_ambient", WGPU_STRLEN};
    fragment.targetCount = count;
    fragment.targets = colors;
    WGPUDepthStencilState depth = WGPU_DEPTH_STENCIL_STATE_INIT;
    depth.format = layout.depth_stencil_format;
    depth.depthWriteEnabled = WGPUOptionalBool_False;
    depth.depthCompare = WGPUCompareFunction_Always;
    WGPURenderPipelineDescriptor desc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    desc.label = {"MidnaFX ambient contact shading", WGPU_STRLEN};
    desc.vertex.module = shader;
    desc.vertex.entryPoint = {"vs_main", WGPU_STRLEN};
    desc.fragment = &fragment;
    desc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    desc.multisample.count = layout.sample_count;
    desc.depthStencil = layout.depth_stencil_format == WGPUTextureFormat_Undefined ? nullptr : &depth;
    next->pipeline = wgpuDeviceCreateRenderPipeline(device.device, &desc);
    wgpuShaderModuleRelease(shader);
    if (!next->pipeline) return nullptr;
    next->bindings = wgpuRenderPipelineGetBindGroupLayout(next->pipeline, 0);
    const bool valid = scopes.finish();
    if (!valid || !next->bindings) {
        if (next->bindings) wgpuBindGroupLayoutRelease(next->bindings);
        wgpuRenderPipelineRelease(next->pipeline); return nullptr;
    }
    auto* result = next.get();
    pipelines.push_back(std::move(next));
    return result;
}
void draw(ModContext*, const GfxDrawContext* context, const void* bytes, size_t size, void*) {
    if (!context || !bytes || size != sizeof(Payload)) return;
    Payload payload{};
    std::memcpy(&payload, bytes, sizeof(payload));
    Pending pending{payload.targets};
    if (!payload.pipeline || payload.pipeline->layout.key != context->layout.key ||
        !payload.color || !payload.depth || payload.uniform_size != sizeof(Uniforms)) return;
    WGPUBindGroupEntry entries[5];
    for (auto& entry : entries) entry = WGPU_BIND_GROUP_ENTRY_INIT;
    entries[0].binding = 0; entries[0].textureView = payload.color;
    entries[1].binding = 1; entries[1].textureView = payload.depth;
    entries[2].binding = 2; entries[2].buffer = context->uniform_buffer;
    entries[2].offset = payload.uniform_offset; entries[2].size = payload.uniform_size;
    entries[3].binding = 3;
    entries[3].textureView = payload.targets && !compute_failed.load()
                                ? payload.targets->views[0] : neutral_view;
    entries[4].binding = 4;
    entries[4].textureView = payload.targets && !compute_failed.load()
                                ? payload.targets->views[2] : neutral_view;
    WGPUBindGroupDescriptor group_desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    group_desc.layout = payload.pipeline->bindings;
    group_desc.entryCount = 5; group_desc.entries = entries;
    auto group = wgpuDeviceCreateBindGroup(context->device, &group_desc);
    if (!group) return;
    const auto width = context->layout.color_attachments[0].width;
    const auto height = context->layout.color_attachments[0].height;
    wgpuRenderPassEncoderSetViewport(context->pass, 0, 0, float(width), float(height), 0, 1);
    wgpuRenderPassEncoderSetScissorRect(context->pass, 0, 0, width, height);
    wgpuRenderPassEncoderSetPipeline(context->pass, payload.pipeline->pipeline);
    wgpuRenderPassEncoderSetBindGroup(context->pass, 0, group, 0, nullptr);
    wgpuRenderPassEncoderDraw(context->pass, 3, 1, 0, 0);
    encoded_draw.fetch_add(1);
    wgpuBindGroupRelease(group);
}
void stage(ModContext*, const GfxStageContext* context, void*) {
    if (submitted || !draw_type || !context || context->stage != GFX_STAGE_SCENE_AFTER_OPAQUE ||
        settings::atmosphere_depth_view_enabled() || settings::dof_coc_view_enabled()) return;
    float occlusion = settings::ambient_occlusion_enabled() && !compute_failed.load() && compute_type &&
                          dComIfGp_getPlayer(0) && !dComIfGp_isPauseFlag()
                                ? settings::ambient_occlusion_strength() : 0.0f;
    float shadows = settings::contact_shadows_enabled() && !compute_failed.load() && compute_type &&
        dComIfGp_getPlayer(0) && !dComIfGp_isPauseFlag() && !dKy_camera_water_in_status_check()
                         ? settings::contact_shadow_strength() : 0.0f;
    float bounce = settings::color_bounce_enabled() && !compute_failed.load() && compute_type &&
        dComIfGp_getPlayer(0) && !dComIfGp_isPauseFlag() && !dKy_camera_water_in_status_check()
                         ? settings::color_bounce_strength() : 0.0f;
    float local = settings::local_lights_enabled() && !compute_failed.load() && compute_type &&
        dComIfGp_getPlayer(0) && !dComIfGp_isPauseFlag() && !dComIfGp_event_runCheck() &&
        !dKy_camera_water_in_status_check() ? settings::local_lights_strength() : 0.0f;
    const auto* sun = g_env_light.mpSunPacket;
    float shafts = settings::sunlight_shafts_enabled() && dComIfGp_getPlayer(0) &&
        !dComIfGp_isPauseFlag() && !dKy_camera_water_in_status_check() &&
        !dKy_darkworld_check() && !g_env_light.hide_vrbox && g_env_light.mSunInitialized &&
        sun && dKy_Outdoor_check() ? settings::sunlight_shafts_strength() : 0.0f;
    const float fog_near = std::max(g_env_light.mFogNear, 0.0f);
    const bool atmosphere_available = settings::atmosphere_enabled() &&
        settings::atmosphere_strength() > 0 && dComIfGp_getPlayer(0) && !dComIfGp_isPauseFlag() &&
        !dKy_camera_water_in_status_check() && std::isfinite(g_env_light.mFogNear) &&
        std::isfinite(g_env_light.mFogFar) && g_env_light.mFogFar > fog_near + 1;
    const float haze = settings::atmosphere_enabled() && atmosphere_available
                           ? settings::atmosphere_strength() : 0.0f;
    if (occlusion <= 0 && shadows <= 0 && bounce <= 0 && local <= 0 && haze <= 0 && shafts <= 0) return;
    // Camera reconstruction currently supports one full native world viewport.
    const auto* viewport = static_cast<const view_port_class*>(context->game_viewport);
    if (!viewport || viewport->x_orig != 0 || viewport->y_orig != 0 ||
        std::abs(viewport->width - mDoGph_gInf_c::getWidth()) > 0.5f ||
        std::abs(viewport->height - mDoGph_gInf_c::getHeight()) > 0.5f) return;
    CameraInfo camera = CAMERA_INFO_INIT;
    if (!svc_camera || !svc_camera->get_camera ||
        svc_camera->get_camera(mod_ctx, context->game_view, &camera) != MOD_OK ||
        !std::all_of(std::begin(camera.view_from_proj), std::end(camera.view_from_proj),
                     [](float v) { return std::isfinite(v); }) ||
        !std::isfinite(camera.proj_from_view[5]) || camera.proj_from_view[5] <= 0) return;
    float light_world[3]{};
    float light_view[3]{};
    if (bounce > 0 && !std::all_of(std::begin(camera.proj_from_view), std::end(camera.proj_from_view),
                                  [](float v) { return std::isfinite(v); })) bounce = 0;
    local_lights::Prepared local_sources;
    if (local > 0) {
        if (!std::all_of(std::begin(camera.proj_from_view), std::end(camera.proj_from_view),
                         [](float value) { return std::isfinite(value); })) local = 0;
        else {
            std::array<local_lights::Source,101> sources{};
            unsigned count = 0;
            // Copy values on the game thread. Actor-owned registry pointers never
            // reach a queued GPU callback or survive into another frame.
            for (const auto* source : g_env_light.pointlight) {
                if (!source) continue;
                sources[count++] = {{source->mPosition.x,source->mPosition.y,source->mPosition.z},
                    {float(source->mColor.r),float(source->mColor.g),float(source->mColor.b)},
                    source->mPow,source->mIndex};
            }
            // Slot zero is the native player-light channel. Its active marker
            // is reset and refreshed each game update; do not reuse stale data.
            const auto& player_light = g_env_light.field_0x0c18[0];
            if (player_light.field_0x26 == 1) {
                sources[count++] = local_lights::lantern_source(
                    {player_light.mPos.x,player_light.mPos.y,player_light.mPos.z},
                    {float(player_light.mColor.r),float(player_light.mColor.g),float(player_light.mColor.b)},
                    player_light.mRefDistance,player_light.field_0x26,player_light.mColor.a,
                    player_light.mAngleAttenuation,player_light.mDistAttenuation);
            }
            local_sources = local_lights::prepare({sources.data(),count}, camera.view_from_world);
            if (!local_sources.count) local = 0;
        }
    }
    if (shadows > 0) {
        // Use the same native base-light position sampled by enhanced water.
        if (!water_probe::latest_environment_light(light_world) ||
            !std::all_of(std::begin(camera.view_from_world), std::end(camera.view_from_world),
                         [](float v) { return std::isfinite(v); }) ||
            !std::all_of(std::begin(camera.proj_from_view), std::end(camera.proj_from_view),
                         [](float v) { return std::isfinite(v); })) {
            shadows = 0;
        } else {
            for (unsigned row = 0; row < 3; ++row)
                light_view[row] = camera.view_from_world[row] * light_world[0] +
                                  camera.view_from_world[4 + row] * light_world[1] +
                                  camera.view_from_world[8 + row] * light_world[2] +
                                  camera.view_from_world[12 + row];
            if (!std::all_of(std::begin(light_view), std::end(light_view),
                             [](float v) { return std::isfinite(v); })) shadows = 0;
        }
    }
    sunlight_shafts::Source sunlight;
    if (shafts > 0) {
        const float world[]{sun->mPos[0].x, sun->mPos[0].y, sun->mPos[0].z};
        const float color[]{sun->mColor.r / 255.0f, sun->mColor.g / 255.0f, sun->mColor.b / 255.0f};
        sunlight = sunlight_shafts::prepare(camera.view_from_world, camera.proj_from_view,
            world, color, shafts, sun->mSunAlpha, sun->mVisibility,
            mDoGph_gInf_c::getWidth(), mDoGph_gInf_c::getHeight());
        shafts = sunlight.screen[2];
    }
    if (occlusion <= 0 && shadows <= 0 && bounce <= 0 && local <= 0 && haze <= 0 && shafts <= 0) return;
    GfxRenderTargetLayout layout = GFX_RENDER_TARGET_LAYOUT_INIT;
    if (svc_gfx->get_scene_target_layout(mod_ctx, &layout) != MOD_OK || !supported(layout)) return;
    auto* pipeline = get_pipeline(layout);
    if (!pipeline) return;
    Targets* targets = occlusion > 0 || shadows > 0 || bounce > 0 || local > 0
        ? ensure_targets(layout.color_attachments[0].width, layout.color_attachments[0].height, bounce > 0 || local > 0) : nullptr;
    if (!targets) { occlusion = 0; shadows = 0; bounce = 0; local = 0; }
    if (occlusion <= 0 && shadows <= 0 && bounce <= 0 && local <= 0 && haze <= 0 && shafts <= 0) return;
    GfxResolveDesc request = GFX_RESOLVE_DESC_INIT;
    request.color = request.depth = true;
    GfxResolvedTargets snapshot = GFX_RESOLVED_TARGETS_INIT;
    if (svc_gfx->resolve_pass(mod_ctx, &request, &snapshot) != MOD_OK ||
        !snapshot.color || !snapshot.depth || snapshot.width != layout.color_attachments[0].width ||
        snapshot.height != layout.color_attachments[0].height ||
        snapshot.color_format != layout.color_attachments[0].format) return;
    Uniforms uniforms{};
    std::memcpy(uniforms.view_from_proj, camera.view_from_proj, sizeof(uniforms.view_from_proj));
    uniforms.radius = settings::ambient_occlusion_radius();
    uniforms.strength = occlusion;
    uniforms.projection_y = camera.proj_from_view[5];
    uniforms.background_depth = device.uses_reversed_z ? 0.0f : 1.0f;
    uniforms.bounce[0] = bounce;
    uniforms.bounce[1] = 180.0f;
    std::copy(sunlight.screen.begin(), sunlight.screen.end(), uniforms.sun_screen);
    std::copy(sunlight.color.begin(), sunlight.color.end(), uniforms.sun_color);
    uniforms.local[0] = local;
    uniforms.local[1] = float(local_sources.count);
    for (unsigned i = 0; i < local_sources.count; ++i) {
        std::copy(local_sources.lights[i].position_radius.begin(),local_sources.lights[i].position_radius.end(),uniforms.local_position[i]);
        std::copy(local_sources.lights[i].color.begin(),local_sources.lights[i].color.end(),uniforms.local_color[i]);
        std::copy(local_sources.lights[i].distance.begin(),local_sources.lights[i].distance.end(),uniforms.local_distance[i]);
    }
    if (shadows > 0 || bounce > 0 || local > 0)
        std::memcpy(uniforms.proj_from_view, camera.proj_from_view, sizeof(uniforms.proj_from_view));
    if (shadows > 0) {
        std::copy(std::begin(light_view), std::end(light_view), uniforms.light_position);
        uniforms.light_position[3] = shadows;
        uniforms.shadow_range[0] = 120.0f;
    }
    if (haze > 0) {
        uniforms.fog_color[0] = std::clamp(float(g_env_light.fog_col.r) / 255.0f, 0.0f, 1.0f);
        uniforms.fog_color[1] = std::clamp(float(g_env_light.fog_col.g) / 255.0f, 0.0f, 1.0f);
        uniforms.fog_color[2] = std::clamp(float(g_env_light.fog_col.b) / 255.0f, 0.0f, 1.0f);
        uniforms.fog_color[3] = haze;
        uniforms.fog_range[0] = fog_near;
        uniforms.fog_range[1] = g_env_light.mFogFar;
    }
    GfxRange range{};
    if (svc_gfx->push_uniform(mod_ctx, &uniforms, sizeof(uniforms), &range) != MOD_OK) return;
    if (targets) {
        const ComputePayload work{snapshot.depth, snapshot.color, targets, range.offset, range.size};
        targets->pending.fetch_add(1);
        if (svc_gfx->push_compute(mod_ctx, compute_type, &work, sizeof(work)) != MOD_OK) {
            targets->pending.fetch_sub(1);
            targets = nullptr;
            if (haze <= 0 && shafts <= 0) return;
        }
    }
    const Payload payload{pipeline, snapshot.color, snapshot.depth, range.offset, range.size, targets};
    if (targets) targets->pending.fetch_add(1);
    if (svc_gfx->push_draw(mod_ctx, draw_type, &payload, sizeof(payload)) != MOD_OK) {
        if (targets) targets->pending.fetch_sub(1); return;
    }
    submitted = true;
    if (!logged && occlusion > 0 && svc_log) {
        svc_log->info(mod_ctx, "Ambient occlusion active: half-resolution 32-tap contacts, edge-aware filter, HUD excluded");
        logged = true;
    }
    if (!local_logged && local > 0 && svc_log) {
        char message[192];
        std::snprintf(message,sizeof(message),
            "Local light response active: sources=%u strength=%.2f bounded linear-light blend, 8 visibility taps, HUD excluded",
            local_sources.count,local);
        svc_log->info(mod_ctx,message);
        local_logged = true;
    }
    if (!lantern_logged && local > 0 && svc_log &&
        std::any_of(local_sources.lights.begin(),local_sources.lights.begin()+local_sources.count,
                    [](const local_lights::Light& source) { return source.distance[3] == 1; })) {
        svc_log->info(mod_ctx,"Player local light active: current native slot, omnidirectional steep GX attenuation, bounded trace envelope");
        lantern_logged = true;
    }
    if (!atmosphere_logged && uniforms.fog_color[3] > 0 && svc_log) {
        char message[192];
        std::snprintf(message, sizeof(message),
                      "Depth atmosphere active: native fog RGB=%d,%d,%d range=%.0f..%.0f strength=%.2f",
                      g_env_light.fog_col.r, g_env_light.fog_col.g, g_env_light.fog_col.b,
                      uniforms.fog_range[0], uniforms.fog_range[1], haze);
        svc_log->info(mod_ctx, message);
        atmosphere_logged = true;
    }
    if (!shadows_logged && shadows > 0 && svc_log) {
        char message[224];
        std::snprintf(message, sizeof(message),
                      "Contact shadows active: native base light view=(%.1f,%.1f,%.1f) length=120 strength=%.2f steps=12 HUD excluded",
                      light_view[0], light_view[1], light_view[2], shadows);
        svc_log->info(mod_ctx, message);
        shadows_logged = true;
    }
    if (!bounce_logged && bounce > 0 && svc_log) {
        svc_log->info(mod_ctx, "Color bounce active: 16 visible surface samples, range=180, linear-light bounded blend, HUD excluded");
        bounce_logged = true;
    }
    if (!sunlight_logged && shafts > 0 && svc_log) {
        char message[224];
        std::snprintf(message, sizeof(message),
            "Sunlight shafts active: native sun UV=(%.3f,%.3f) RGB=(%.2f,%.2f,%.2f) strength=%.3f sky-depth taps=16 HUD excluded",
            sunlight.screen[0], sunlight.screen[1], sunlight.color[0], sunlight.color[1], sunlight.color[2], shafts);
        svc_log->info(mod_ctx,message);
        sunlight_logged = true;
    }
}
void begin_scene(ModContext*, const GfxStageContext*, void*) {
    submitted = false;
    collect_targets();
}
}
void apply(const GfxStageContext* context) { stage(nullptr, context, nullptr); }
void initialize() {
    sunlight_logged = false;
    logged = false;
    shadows_logged = false;
    bounce_logged = false;
    local_logged = false;
    lantern_logged = false;
    atmosphere_logged = false;
    submitted = false;
    compute_failed.store(false); encoded_compute.store(0); encoded_draw.store(0);
    if (!svc_gfx || svc_gfx->get_device_info(mod_ctx, &device) != MOD_OK || !device.device || !device.instance) return;
    gpu::ValidationScopes neutral_scopes(device.device, device.instance);
    WGPUTextureDescriptor texture_desc = WGPU_TEXTURE_DESCRIPTOR_INIT;
    texture_desc.size = {1,1,1}; texture_desc.format = WGPUTextureFormat_RGBA16Float;
    texture_desc.usage = WGPUTextureUsage_TextureBinding;
    neutral_texture = wgpuDeviceCreateTexture(device.device, &texture_desc);
    if (neutral_texture) neutral_view = wgpuTextureCreateView(neutral_texture, nullptr);
    const bool neutral_valid = neutral_scopes.finish();
    if (!neutral_valid || !neutral_view) return;
    gpu::ValidationScopes compute_scopes(device.device, device.instance);
    WGPUShaderSourceWGSL source = WGPU_SHADER_SOURCE_WGSL_INIT;
    source.code = {ambient_occlusion_compute_shader, WGPU_STRLEN};
    WGPUShaderModuleDescriptor shader_desc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    shader_desc.nextInChain = &source.chain;
    auto shader = wgpuDeviceCreateShaderModule(device.device, &shader_desc);
    constexpr const char* entries[]{"cs_occlusion", "cs_bounce", "cs_filter_horizontal", "cs_filter_vertical"};
    if (shader) for (unsigned i = 0; i < 4; ++i) {
        WGPUComputePipelineDescriptor desc = WGPU_COMPUTE_PIPELINE_DESCRIPTOR_INIT;
        desc.compute.module = shader; desc.compute.entryPoint = {entries[i], WGPU_STRLEN};
        compute_pipelines[i] = wgpuDeviceCreateComputePipeline(device.device, &desc);
        if (compute_pipelines[i]) compute_layouts[i] = wgpuComputePipelineGetBindGroupLayout(compute_pipelines[i], 0);
    }
    if (shader) wgpuShaderModuleRelease(shader);
    const bool compute_valid = compute_scopes.finish();
    GfxComputeTypeDesc compute_desc = GFX_COMPUTE_TYPE_DESC_INIT;
    compute_desc.label = "MidnaFX filtered contact shading"; compute_desc.callback = compute;
    if (!compute_valid || !compute_layouts[0] || !compute_layouts[1] || !compute_layouts[2] || !compute_layouts[3] ||
        svc_gfx->register_compute_type(mod_ctx, &compute_desc, &compute_type) != MOD_OK) {
        compute_failed.store(true);
        if (svc_log) svc_log->warn(mod_ctx, "Contact lighting compute unavailable; AO, shadows and bounce bypassed");
    }
    GfxDrawTypeDesc draw_desc = GFX_DRAW_TYPE_DESC_INIT;
    draw_desc.label = "MidnaFX ambient contact shading";
    draw_desc.draw = draw;
    if (svc_gfx->register_draw_type(mod_ctx, &draw_desc, &draw_type) != MOD_OK) return;
    GfxStageHookDesc hook = GFX_STAGE_HOOK_DESC_INIT;
    hook.callback = begin_scene;
    if (svc_gfx->register_stage_hook(mod_ctx, GFX_STAGE_SCENE_BEGIN, &hook, &begin_hook) != MOD_OK) {
        svc_gfx->unregister_draw_type(mod_ctx, draw_type);
        draw_type = 0;
        return;
    }
    hook.callback = stage;
    if (svc_gfx->register_stage_hook(mod_ctx, GFX_STAGE_SCENE_AFTER_OPAQUE, &hook, &stage_hook) != MOD_OK) {
        svc_gfx->unregister_stage_hook(mod_ctx, begin_hook);
        begin_hook = 0;
        svc_gfx->unregister_draw_type(mod_ctx, draw_type);
        draw_type = 0;
    }
}
void shutdown() {
    if (svc_gfx && begin_hook) svc_gfx->unregister_stage_hook(mod_ctx, begin_hook);
    if (svc_gfx && stage_hook) svc_gfx->unregister_stage_hook(mod_ctx, stage_hook);
    if (svc_gfx && draw_type) svc_gfx->unregister_draw_type(mod_ctx, draw_type);
    if (svc_gfx && compute_type) svc_gfx->unregister_compute_type(mod_ctx, compute_type);
    compute_type = 0;
    stage_hook = begin_hook = draw_type = 0;
    for (auto& pipeline : pipelines) {
        wgpuBindGroupLayoutRelease(pipeline->bindings);
        wgpuRenderPipelineRelease(pipeline->pipeline);
    }
    pipelines.clear();
    if (svc_log && encoded_draw.load()) {
        char message[192];
        std::snprintf(message, sizeof(message), "Contact shading shutdown: compute=%llu draw=%llu target_sets=%zu",
                      static_cast<unsigned long long>(encoded_compute.load()),
                      static_cast<unsigned long long>(encoded_draw.load()), target_sets.size());
        svc_log->info(mod_ctx, message);
    }
    target_sets.clear(); current_targets = nullptr;
    for (auto& layout : compute_layouts) if (layout) { wgpuBindGroupLayoutRelease(layout); layout = nullptr; }
    for (auto& pipeline : compute_pipelines) if (pipeline) { wgpuComputePipelineRelease(pipeline); pipeline = nullptr; }
    if (neutral_view) { wgpuTextureViewRelease(neutral_view); neutral_view = nullptr; }
    if (neutral_texture) { wgpuTextureRelease(neutral_texture); neutral_texture = nullptr; }
}
}
