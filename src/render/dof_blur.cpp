#include "dof_blur.hpp"
#include "game/camera_probe.hpp"
#include "midnafx_shader.hpp"
#include "services.hpp"
#include "ui/settings.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace midnafx::render::dof_blur {
namespace {
struct Uniforms {
    float view_from_proj[16];
    float full_size[2];
    float half_size[2];
    float near_plane;
    float far_plane;
    float focus_distance;
    float focus_range;
    float background_depth;
    float blur_radius;
    float padding[2];
};
static_assert(sizeof(Uniforms) == 112);

struct Targets {
    std::uint32_t width = 0, height = 0;
    WGPUTexture textures[4]{};
    WGPUTextureView views[4]{};
};
struct RetiredTargets {
    Targets targets;
    unsigned frames_left = 0;
};
struct CompositePipeline {
    GfxRenderTargetLayout layout = GFX_RENDER_TARGET_LAYOUT_INIT;
    WGPURenderPipeline pipeline = nullptr;
    WGPUBindGroupLayout bind_layout = nullptr;
};
struct ComputePayload {
    WGPUTextureView scene;
    WGPUTextureView depth;
    WGPUTextureView near_a;
    WGPUTextureView far_a;
    WGPUTextureView near_b;
    WGPUTextureView far_b;
    std::uint32_t uniform_offset;
    std::uint32_t uniform_size;
    std::uint32_t width;
    std::uint32_t height;
};
struct DrawPayload {
    WGPUTextureView scene;
    WGPUTextureView depth;
    WGPUTextureView near_blur;
    WGPUTextureView far_blur;
    const CompositePipeline* pipeline;
    std::uint64_t layout_key;
    std::uint32_t uniform_offset;
    std::uint32_t uniform_size;
};
static_assert(sizeof(ComputePayload) <= GFX_INLINE_DRAW_PAYLOAD_SIZE);
static_assert(sizeof(DrawPayload) <= GFX_INLINE_DRAW_PAYLOAD_SIZE);
static_assert(std::is_trivially_copyable_v<ComputePayload>);
static_assert(std::is_trivially_copyable_v<DrawPayload>);

GfxDeviceInfo device = GFX_DEVICE_INFO_INIT;
GfxComputeTypeHandle compute_type = 0;
GfxDrawTypeHandle draw_type = 0;
GfxStageHookHandle stage_hook = 0;
WGPUShaderModule compute_shader = nullptr;
WGPUComputePipeline compute_pipelines[3]{};
WGPUBindGroupLayout compute_layouts[3]{};
std::vector<std::unique_ptr<CompositePipeline>> composite_pipelines;
Targets targets;
std::vector<RetiredTargets> retired_targets;
std::atomic<std::uint64_t> encoded_compute{0}, encoded_draw{0};
bool ready = false;
bool warned = false;
bool logged = false;
bool execution_logged = false;

void warn_once(const char* message) {
    if (!warned && svc_log) {
        svc_log->warn(mod_ctx, message);
        warned = true;
    }
}

WGPUShaderModule create_shader(const char* source, const char* label) {
    WGPUShaderSourceWGSL wgsl = WGPU_SHADER_SOURCE_WGSL_INIT;
    wgsl.code = {source, WGPU_STRLEN};
    WGPUShaderModuleDescriptor desc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    desc.nextInChain = &wgsl.chain;
    desc.label = {label, WGPU_STRLEN};
    return wgpuDeviceCreateShaderModule(device.device, &desc);
}

void release_targets(Targets& value) {
    for (auto& view : value.views) {
        if (view)
            wgpuTextureViewRelease(std::exchange(view, nullptr));
    }
    for (auto& texture : value.textures) {
        if (texture)
            wgpuTextureRelease(std::exchange(texture, nullptr));
    }
    value.width = value.height = 0;
}

void tick_retired_targets() {
    for (auto it = retired_targets.begin(); it != retired_targets.end();) {
        if (--it->frames_left == 0) {
            release_targets(it->targets);
            it = retired_targets.erase(it);
        } else {
            ++it;
        }
    }
}

bool ensure_targets(std::uint32_t width, std::uint32_t height) {
    if (targets.width == width && targets.height == height)
        return true;
    if (targets.width)
        retired_targets.push_back({std::exchange(targets, Targets{}), 4});
    for (unsigned i = 0; i < 4; ++i) {
        WGPUTextureDescriptor desc = WGPU_TEXTURE_DESCRIPTOR_INIT;
        desc.label = {i < 2 ? "MidnaFX DOF intermediate A" : "MidnaFX DOF intermediate B",
                      WGPU_STRLEN};
        desc.usage = WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding;
        desc.size = {width, height, 1};
        desc.format = WGPUTextureFormat_RGBA16Float;
        targets.textures[i] = wgpuDeviceCreateTexture(device.device, &desc);
        if (!targets.textures[i]) {
            release_targets(targets);
            return false;
        }
        targets.views[i] = wgpuTextureCreateView(targets.textures[i], nullptr);
        if (!targets.views[i]) {
            release_targets(targets);
            return false;
        }
    }
    targets.width = width;
    targets.height = height;
    return true;
}

bool supported(const GfxRenderTargetLayout& layout) {
    if (layout.color_attachment_count < 1 || layout.sample_count != 1 ||
        layout.color_attachments[0].semantic != GFX_ATTACHMENT_SCENE_COLOR)
        return false;
    const auto format = layout.color_attachments[0].format;
    return format == WGPUTextureFormat_RGBA8Unorm || format == WGPUTextureFormat_BGRA8Unorm;
}

CompositePipeline* find_pipeline(std::uint64_t key) {
    for (auto& pair : composite_pipelines)
        if (pair->layout.key == key)
            return pair.get();
    return nullptr;
}

CompositePipeline* create_composite_pipeline(const GfxRenderTargetLayout& layout) {
    auto value = std::make_unique<CompositePipeline>();
    value->layout = layout;
    auto shader = create_shader(dof_blur_composite_shader, "MidnaFX DOF composite shader");
    if (!shader)
        return nullptr;
    WGPUColorTargetState colors[GFX_MAX_COLOR_ATTACHMENTS];
    const auto color_count =
        gfx_init_color_target_states(&layout, colors, nullptr, WGPUColorWriteMask_All);
    WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
    fragment.module = shader;
    fragment.entryPoint = {"fs_composite", WGPU_STRLEN};
    fragment.targetCount = color_count;
    fragment.targets = colors;
    WGPUDepthStencilState depth = WGPU_DEPTH_STENCIL_STATE_INIT;
    depth.format = layout.depth_stencil_format;
    depth.depthWriteEnabled = WGPUOptionalBool_False;
    depth.depthCompare = WGPUCompareFunction_Always;
    WGPURenderPipelineDescriptor desc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    desc.label = {"MidnaFX DOF composite", WGPU_STRLEN};
    desc.vertex.module = shader;
    desc.vertex.entryPoint = {"vs_main", WGPU_STRLEN};
    desc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    desc.depthStencil = layout.depth_stencil_format == WGPUTextureFormat_Undefined ? nullptr : &depth;
    desc.multisample.count = layout.sample_count;
    desc.fragment = &fragment;
    value->pipeline = wgpuDeviceCreateRenderPipeline(device.device, &desc);
    wgpuShaderModuleRelease(shader);
    if (!value->pipeline)
        return nullptr;
    value->bind_layout = wgpuRenderPipelineGetBindGroupLayout(value->pipeline, 0);
    if (!value->bind_layout) {
        wgpuRenderPipelineRelease(value->pipeline);
        return nullptr;
    }
    auto* result = value.get();
    composite_pipelines.push_back(std::move(value));
    return result;
}

WGPUBindGroupEntry texture_entry(std::uint32_t binding, WGPUTextureView view) {
    WGPUBindGroupEntry entry = WGPU_BIND_GROUP_ENTRY_INIT;
    entry.binding = binding;
    entry.textureView = view;
    return entry;
}

void on_compute(ModContext*, const GfxComputeContext* ctx, const void* bytes, size_t size, void*) {
    if (!ctx || size != sizeof(ComputePayload))
        return;
    ComputePayload payload{};
    std::memcpy(&payload, bytes, sizeof(payload));
    if (!payload.scene || !payload.depth || !payload.near_a || !payload.far_a ||
        !payload.near_b || !payload.far_b || !compute_pipelines[0])
        return;
    auto make_group = [&](unsigned pipeline, WGPUTextureView input_near,
                          WGPUTextureView input_far, WGPUTextureView output_near,
                          WGPUTextureView output_far) {
        WGPUBindGroupEntry entries[5]{WGPU_BIND_GROUP_ENTRY_INIT, WGPU_BIND_GROUP_ENTRY_INIT,
                                      WGPU_BIND_GROUP_ENTRY_INIT, WGPU_BIND_GROUP_ENTRY_INIT,
                                      WGPU_BIND_GROUP_ENTRY_INIT};
        entries[0] = texture_entry(0, input_near);
        entries[1] = texture_entry(1, input_far);
        entries[2] = texture_entry(2, output_near);
        entries[3] = texture_entry(3, output_far);
        entries[4].binding = 4;
        entries[4].buffer = ctx->uniform_buffer;
        entries[4].offset = payload.uniform_offset;
        entries[4].size = payload.uniform_size;
        WGPUBindGroupDescriptor desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
        desc.layout = compute_layouts[pipeline];
        desc.entryCount = 5;
        desc.entries = entries;
        return wgpuDeviceCreateBindGroup(ctx->device, &desc);
    };
    WGPUBindGroup groups[3]{
        make_group(0, payload.scene, payload.depth, payload.near_a, payload.far_a),
        make_group(1, payload.near_a, payload.far_a, payload.near_b, payload.far_b),
        make_group(2, payload.near_b, payload.far_b, payload.near_a, payload.far_a)};
    if (!groups[0] || !groups[1] || !groups[2]) {
        for (auto group : groups)
            if (group)
                wgpuBindGroupRelease(group);
        return;
    }
    WGPUComputePassDescriptor pass_desc = WGPU_COMPUTE_PASS_DESCRIPTOR_INIT;
    pass_desc.label = {"MidnaFX DOF blur", WGPU_STRLEN};
    auto pass = wgpuCommandEncoderBeginComputePass(ctx->encoder, &pass_desc);
    const auto x = (payload.width + 7) / 8;
    const auto y = (payload.height + 7) / 8;
    for (unsigned i = 0; i < 3; ++i) {
        wgpuComputePassEncoderSetPipeline(pass, compute_pipelines[i]);
        wgpuComputePassEncoderSetBindGroup(pass, 0, groups[i], 0, nullptr);
        wgpuComputePassEncoderDispatchWorkgroups(pass, x, y, 1);
    }
    wgpuComputePassEncoderEnd(pass);
    wgpuComputePassEncoderRelease(pass);
    for (auto group : groups)
        wgpuBindGroupRelease(group);
    encoded_compute.fetch_add(1, std::memory_order_relaxed);
}

void on_draw(ModContext*, const GfxDrawContext* ctx, const void* bytes, size_t size, void*) {
    if (!ctx || size != sizeof(DrawPayload))
        return;
    DrawPayload payload{};
    std::memcpy(&payload, bytes, sizeof(payload));
    if (!payload.pipeline || payload.layout_key != ctx->layout.key ||
        payload.layout_key != payload.pipeline->layout.key || !payload.scene || !payload.depth ||
        !payload.near_blur || !payload.far_blur)
        return;
    WGPUBindGroupEntry entries[5]{WGPU_BIND_GROUP_ENTRY_INIT, WGPU_BIND_GROUP_ENTRY_INIT,
                                  WGPU_BIND_GROUP_ENTRY_INIT, WGPU_BIND_GROUP_ENTRY_INIT,
                                  WGPU_BIND_GROUP_ENTRY_INIT};
    entries[0] = texture_entry(0, payload.scene);
    entries[1] = texture_entry(1, payload.depth);
    entries[2] = texture_entry(2, payload.near_blur);
    entries[3] = texture_entry(3, payload.far_blur);
    entries[4].binding = 4;
    entries[4].buffer = ctx->uniform_buffer;
    entries[4].offset = payload.uniform_offset;
    entries[4].size = payload.uniform_size;
    WGPUBindGroupDescriptor desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    desc.label = {"MidnaFX DOF composite inputs", WGPU_STRLEN};
    desc.layout = payload.pipeline->bind_layout;
    desc.entryCount = 5;
    desc.entries = entries;
    auto group = wgpuDeviceCreateBindGroup(ctx->device, &desc);
    if (!group)
        return;
    const auto width = ctx->layout.color_attachments[0].width;
    const auto height = ctx->layout.color_attachments[0].height;
    wgpuRenderPassEncoderSetViewport(ctx->pass, 0, 0, static_cast<float>(width),
                                     static_cast<float>(height), 0, 1);
    wgpuRenderPassEncoderSetScissorRect(ctx->pass, 0, 0, width, height);
    wgpuRenderPassEncoderSetPipeline(ctx->pass, payload.pipeline->pipeline);
    wgpuRenderPassEncoderSetBindGroup(ctx->pass, 0, group, 0, nullptr);
    wgpuRenderPassEncoderDraw(ctx->pass, 3, 1, 0, 0);
    wgpuBindGroupRelease(group);
    encoded_draw.fetch_add(1, std::memory_order_relaxed);
}

bool valid_camera(const CameraInfo& camera) {
    if (!std::isfinite(camera.near_plane) || !std::isfinite(camera.far_plane) ||
        camera.near_plane <= 0.0f || camera.far_plane <= camera.near_plane)
        return false;
    return std::all_of(std::begin(camera.view_from_proj), std::end(camera.view_from_proj),
                       [](float value) { return std::isfinite(value); });
}

void on_stage(ModContext*, const GfxStageContext* context, void*) {
    tick_retired_targets();
    if (!ready || !settings::dof_blur_enabled())
        return;
    if (!context || context->stage != GFX_STAGE_FRAME_BEFORE_HUD ||
        settings::dof_coc_view_enabled() || settings::atmosphere_depth_view_enabled())
        return;
    CameraInfo camera = CAMERA_INFO_INIT;
    if (!camera_probe::latest_camera_info(camera) || !valid_camera(camera))
        return;
    float focus = settings::dof_focus_distance();
    if (settings::dof_autofocus_enabled() && !camera_probe::latest_focus_distance(focus))
        return;
    if (!std::isfinite(focus) || focus <= 0.0f)
        return;
    GfxRenderTargetLayout layout = GFX_RENDER_TARGET_LAYOUT_INIT;
    if (svc_gfx->get_scene_target_layout(mod_ctx, &layout) != MOD_OK || !supported(layout))
        return;
    auto* pipeline = find_pipeline(layout.key);
    if (!pipeline)
        pipeline = create_composite_pipeline(layout);
    if (!pipeline) {
        warn_once("Depth of field composite pipeline unavailable; effect bypassed");
        return;
    }
    GfxResolveDesc request = GFX_RESOLVE_DESC_INIT;
    request.color = true;
    request.depth = true;
    GfxResolvedTargets resolved = GFX_RESOLVED_TARGETS_INIT;
    if (svc_gfx->resolve_pass(mod_ctx, &request, &resolved) != MOD_OK || !resolved.color ||
        !resolved.depth || resolved.width == 0 || resolved.height == 0 ||
        resolved.color_format != layout.color_attachments[0].format)
        return;
    const std::uint32_t half_width = (resolved.width + 1) / 2;
    const std::uint32_t half_height = (resolved.height + 1) / 2;
    if (!ensure_targets(half_width, half_height)) {
        warn_once("Depth of field intermediate allocation failed; effect bypassed");
        return;
    }
    Uniforms uniforms{};
    std::memcpy(uniforms.view_from_proj, camera.view_from_proj,
                sizeof(uniforms.view_from_proj));
    uniforms.full_size[0] = static_cast<float>(resolved.width);
    uniforms.full_size[1] = static_cast<float>(resolved.height);
    uniforms.half_size[0] = static_cast<float>(half_width);
    uniforms.half_size[1] = static_cast<float>(half_height);
    uniforms.near_plane = camera.near_plane;
    uniforms.far_plane = camera.far_plane;
    uniforms.focus_distance = focus;
    uniforms.focus_range = std::max(settings::dof_focus_range(), 1.0f);
    uniforms.background_depth = device.uses_reversed_z ? 0.0f : 1.0f;
    uniforms.blur_radius = 12.0f;
    GfxRange range{};
    if (svc_gfx->push_uniform(mod_ctx, &uniforms, sizeof(uniforms), &range) != MOD_OK)
        return;
    ComputePayload compute{resolved.color, resolved.depth, targets.views[0], targets.views[1],
                           targets.views[2], targets.views[3], range.offset, range.size,
                           half_width, half_height};
    if (svc_gfx->push_compute(mod_ctx, compute_type, &compute, sizeof(compute)) != MOD_OK)
        return;
    DrawPayload draw{resolved.color, resolved.depth, targets.views[0], targets.views[1], pipeline,
                     layout.key, range.offset, range.size};
    if (svc_gfx->push_draw(mod_ctx, draw_type, &draw, sizeof(draw)) != MOD_OK)
        return;
    if (!logged && svc_log) {
        svc_log->info(mod_ctx,
                      "Depth of field blur prototype active: half-resolution, radius=12px");
        logged = true;
    }
}

void release_gpu() {
    release_targets(targets);
    for (auto& retired : retired_targets)
        release_targets(retired.targets);
    retired_targets.clear();
    for (auto& pair : composite_pipelines) {
        if (pair->bind_layout)
            wgpuBindGroupLayoutRelease(pair->bind_layout);
        if (pair->pipeline)
            wgpuRenderPipelineRelease(pair->pipeline);
    }
    composite_pipelines.clear();
    for (auto& layout : compute_layouts)
        if (layout)
            wgpuBindGroupLayoutRelease(std::exchange(layout, nullptr));
    for (auto& pipeline : compute_pipelines)
        if (pipeline)
            wgpuComputePipelineRelease(std::exchange(pipeline, nullptr));
    if (compute_shader)
        wgpuShaderModuleRelease(std::exchange(compute_shader, nullptr));
}
} // namespace

void initialize() {
    ready = warned = logged = execution_logged = false;
    encoded_compute.store(0);
    encoded_draw.store(0);
    if (!svc_gfx || svc_gfx->get_device_info(mod_ctx, &device) != MOD_OK || !device.device)
        return;
    compute_shader = create_shader(dof_blur_compute_shader, "MidnaFX DOF compute shader");
    if (!compute_shader) {
        warn_once("Depth of field compute shader unavailable; effect bypassed");
        return;
    }
    constexpr const char* entries[]{"cs_downsample", "cs_blur_horizontal", "cs_blur_vertical"};
    for (unsigned i = 0; i < 3; ++i) {
        WGPUComputePipelineDescriptor desc = WGPU_COMPUTE_PIPELINE_DESCRIPTOR_INIT;
        desc.label = {entries[i], WGPU_STRLEN};
        desc.compute.module = compute_shader;
        desc.compute.entryPoint = {entries[i], WGPU_STRLEN};
        compute_pipelines[i] = wgpuDeviceCreateComputePipeline(device.device, &desc);
        if (!compute_pipelines[i]) {
            warn_once("Depth of field compute pipeline unavailable; effect bypassed");
            release_gpu();
            return;
        }
        compute_layouts[i] = wgpuComputePipelineGetBindGroupLayout(compute_pipelines[i], 0);
        if (!compute_layouts[i]) {
            warn_once("Depth of field compute layout unavailable; effect bypassed");
            release_gpu();
            return;
        }
    }
    GfxComputeTypeDesc compute_desc = GFX_COMPUTE_TYPE_DESC_INIT;
    compute_desc.label = "MidnaFX DOF blur";
    compute_desc.callback = on_compute;
    if (svc_gfx->register_compute_type(mod_ctx, &compute_desc, &compute_type) != MOD_OK) {
        release_gpu();
        return;
    }
    GfxDrawTypeDesc draw_desc = GFX_DRAW_TYPE_DESC_INIT;
    draw_desc.label = "MidnaFX DOF composite";
    draw_desc.draw = on_draw;
    if (svc_gfx->register_draw_type(mod_ctx, &draw_desc, &draw_type) != MOD_OK) {
        svc_gfx->unregister_compute_type(mod_ctx, compute_type);
        compute_type = 0;
        release_gpu();
        return;
    }
    GfxStageHookDesc stage_desc = GFX_STAGE_HOOK_DESC_INIT;
    stage_desc.callback = on_stage;
    if (svc_gfx->register_stage_hook(mod_ctx, GFX_STAGE_FRAME_BEFORE_HUD, &stage_desc,
                                     &stage_hook) != MOD_OK) {
        svc_gfx->unregister_draw_type(mod_ctx, draw_type);
        svc_gfx->unregister_compute_type(mod_ctx, compute_type);
        draw_type = compute_type = 0;
        release_gpu();
        return;
    }
    ready = true;
}

void update() {
    if (!execution_logged && encoded_compute.load(std::memory_order_acquire) != 0 &&
        encoded_draw.load(std::memory_order_acquire) != 0 && svc_log) {
        svc_log->info(mod_ctx, "Depth of field blur compute and composite executed");
        execution_logged = true;
    }
}

void shutdown() {
    ready = false;
    if (svc_gfx && stage_hook)
        svc_gfx->unregister_stage_hook(mod_ctx, stage_hook);
    if (svc_gfx && draw_type)
        svc_gfx->unregister_draw_type(mod_ctx, draw_type);
    if (svc_gfx && compute_type)
        svc_gfx->unregister_compute_type(mod_ctx, compute_type);
    stage_hook = draw_type = compute_type = 0;
    release_gpu();
}
} // namespace midnafx::render::dof_blur
