#include "ambient_occlusion.hpp"
#include "midnafx_shader.hpp"
#include "services.hpp"
#include "ui/settings.hpp"
#include <f_op/f_op_view.h>
#include <helpers/gx_helper.h>
#include <m_Do/m_Do_graphic.h>
#include <d/d_kankyo.h>
#include <d/d_com_inf_game.h>
#include <algorithm>
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
};
static_assert(sizeof(Uniforms) == 112);
struct Pipeline {
    GfxRenderTargetLayout layout = GFX_RENDER_TARGET_LAYOUT_INIT;
    WGPURenderPipeline pipeline = nullptr;
    WGPUBindGroupLayout bindings = nullptr;
};
struct Payload {
    const Pipeline* pipeline;
    WGPUTextureView color, depth;
    std::uint32_t uniform_offset, uniform_size;
};
static_assert(sizeof(Payload) <= GFX_INLINE_DRAW_PAYLOAD_SIZE);
GfxDeviceInfo device = GFX_DEVICE_INFO_INIT;
GfxStageHookHandle stage_hook = 0;
GfxStageHookHandle begin_hook = 0;
GfxDrawTypeHandle draw_type = 0;
std::vector<std::unique_ptr<Pipeline>> pipelines;
bool logged = false;
bool atmosphere_logged = false;
bool submitted = false;

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
    if (!next->bindings) { wgpuRenderPipelineRelease(next->pipeline); return nullptr; }
    auto* result = next.get();
    pipelines.push_back(std::move(next));
    return result;
}
void draw(ModContext*, const GfxDrawContext* context, const void* bytes, size_t size, void*) {
    if (!context || !bytes || size != sizeof(Payload)) return;
    Payload payload{};
    std::memcpy(&payload, bytes, sizeof(payload));
    if (!payload.pipeline || payload.pipeline->layout.key != context->layout.key ||
        !payload.color || !payload.depth || payload.uniform_size != sizeof(Uniforms)) return;
    WGPUBindGroupEntry entries[3]{WGPU_BIND_GROUP_ENTRY_INIT, WGPU_BIND_GROUP_ENTRY_INIT,
                                  WGPU_BIND_GROUP_ENTRY_INIT};
    entries[0].binding = 0; entries[0].textureView = payload.color;
    entries[1].binding = 1; entries[1].textureView = payload.depth;
    entries[2].binding = 2; entries[2].buffer = context->uniform_buffer;
    entries[2].offset = payload.uniform_offset; entries[2].size = payload.uniform_size;
    WGPUBindGroupDescriptor group_desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    group_desc.layout = payload.pipeline->bindings;
    group_desc.entryCount = 3; group_desc.entries = entries;
    auto group = wgpuDeviceCreateBindGroup(context->device, &group_desc);
    if (!group) return;
    const auto width = context->layout.color_attachments[0].width;
    const auto height = context->layout.color_attachments[0].height;
    wgpuRenderPassEncoderSetViewport(context->pass, 0, 0, float(width), float(height), 0, 1);
    wgpuRenderPassEncoderSetScissorRect(context->pass, 0, 0, width, height);
    wgpuRenderPassEncoderSetPipeline(context->pass, payload.pipeline->pipeline);
    wgpuRenderPassEncoderSetBindGroup(context->pass, 0, group, 0, nullptr);
    wgpuRenderPassEncoderDraw(context->pass, 3, 1, 0, 0);
    wgpuBindGroupRelease(group);
}
void stage(ModContext*, const GfxStageContext* context, void*) {
    if (submitted || !draw_type || !context || context->stage != GFX_STAGE_SCENE_AFTER_OPAQUE ||
        settings::atmosphere_depth_view_enabled() || settings::dof_coc_view_enabled()) return;
    const float occlusion = settings::ambient_occlusion_enabled()
                                ? settings::ambient_occlusion_strength() : 0.0f;
    const float fog_near = std::max(g_env_light.mFogNear, 0.0f);
    const bool atmosphere_available = settings::atmosphere_enabled() &&
        settings::atmosphere_strength() > 0 && dComIfGp_getPlayer(0) && !dComIfGp_isPauseFlag() &&
        !dKy_camera_water_in_status_check() && std::isfinite(g_env_light.mFogNear) &&
        std::isfinite(g_env_light.mFogFar) && g_env_light.mFogFar > fog_near + 1;
    const float haze = settings::atmosphere_enabled() && atmosphere_available
                           ? settings::atmosphere_strength() : 0.0f;
    if (occlusion <= 0 && haze <= 0) return;
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
    GfxRenderTargetLayout layout = GFX_RENDER_TARGET_LAYOUT_INIT;
    if (svc_gfx->get_scene_target_layout(mod_ctx, &layout) != MOD_OK || !supported(layout)) return;
    auto* pipeline = get_pipeline(layout);
    if (!pipeline) return;
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
    const Payload payload{pipeline, snapshot.color, snapshot.depth, range.offset, range.size};
    if (svc_gfx->push_draw(mod_ctx, draw_type, &payload, sizeof(payload)) != MOD_OK) return;
    submitted = true;
    if (!logged && occlusion > 0 && svc_log) {
        svc_log->info(mod_ctx, "Ambient occlusion active: opaque scene, 16 depth samples, HUD excluded");
        logged = true;
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
}
void begin_scene(ModContext*, const GfxStageContext*, void*) { submitted = false; }
}
void apply(const GfxStageContext* context) { stage(nullptr, context, nullptr); }
void initialize() {
    logged = false;
    atmosphere_logged = false;
    submitted = false;
    if (!svc_gfx || svc_gfx->get_device_info(mod_ctx, &device) != MOD_OK || !device.device) return;
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
    stage_hook = begin_hook = draw_type = 0;
    for (auto& pipeline : pipelines) {
        wgpuBindGroupLayoutRelease(pipeline->bindings);
        wgpuRenderPipelineRelease(pipeline->pipeline);
    }
    pipelines.clear();
}
}
