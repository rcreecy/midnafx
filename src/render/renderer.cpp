#include "renderer.hpp"
#include "midnafx_shader.hpp"
#include "services.hpp"
#include "ui/settings.hpp"
#include <mods/svc/gfx.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

namespace midnafx::render {
namespace {
struct Payload {
    WGPUTextureView scene;
    std::uint64_t layout_key;
    std::uint32_t kind;
    std::uint32_t uniform_offset;
    std::uint32_t uniform_size;
};
static_assert(sizeof(Payload) <= GFX_INLINE_DRAW_PAYLOAD_SIZE);

GfxDeviceInfo device = GFX_DEVICE_INFO_INIT;
GfxRenderTargetLayout layout = GFX_RENDER_TARGET_LAYOUT_INIT;
struct Pipeline {
    WGPUShaderModule shader = nullptr;
    WGPURenderPipeline pipeline = nullptr;
    WGPUBindGroupLayout bind_layout = nullptr;
};
Pipeline pipelines[2]; // 0: M1 passthrough; 1: fused M2 grading.
GfxStageHookHandle stage_hook = 0;
GfxDrawTypeHandle draw_type = 0;
std::atomic<std::uint32_t> state{
    0}; // 0 unavailable, 1 ready, 2 failed, 3 layout skip, 4 GPU pending, 5 init layout skip
std::atomic<std::uint32_t> width{0}, height{0};
std::atomic<std::uint64_t> submitted{0}, encoded{0}, groups{0}, builds{0};
std::atomic<double> callback_us{0.0};
bool warned = false;
std::chrono::steady_clock::time_point next_init_retry{};

struct ScopeResult {
    bool complete = false;
    bool ok = false;
};

void scope_callback(WGPUPopErrorScopeStatus status, WGPUErrorType type, WGPUStringView,
                    void* user_data, void*) {
    auto& result = *static_cast<ScopeResult*>(user_data);
    result.ok = status == WGPUPopErrorScopeStatus_Success && type == WGPUErrorType_NoError;
    result.complete = true;
}

bool pop_scope(WGPUDevice gpu, WGPUInstance instance) {
    ScopeResult result;
    WGPUPopErrorScopeCallbackInfo callback = WGPU_POP_ERROR_SCOPE_CALLBACK_INFO_INIT;
    callback.mode = WGPUCallbackMode_WaitAnyOnly;
    callback.callback = scope_callback;
    callback.userdata1 = &result;
    WGPUFutureWaitInfo wait = WGPU_FUTURE_WAIT_INFO_INIT;
    wait.future = wgpuDevicePopErrorScope(gpu, callback);
    // Callback storage is on this stack: always drain the future before returning.
    while (!wait.completed) {
        const auto status = wgpuInstanceWaitAny(instance, 1, &wait, 0);
        if (status != WGPUWaitStatus_Success && status != WGPUWaitStatus_TimedOut) {
            // WaitAnyOnly callbacks fire only when this future is explicitly waited on.
            // An invalid wait can be abandoned without leaving a callback into this mod.
            return false;
        }
        if (!wait.completed)
            std::this_thread::yield();
    }
    return result.complete && result.ok;
}

bool supported(const GfxRenderTargetLayout& target) {
    if (target.color_attachment_count < 1 || target.sample_count != 1 ||
        target.color_attachments[0].semantic != GFX_ATTACHMENT_SCENE_COLOR)
        return false;
    auto format = target.color_attachments[0].format;
    return format == WGPUTextureFormat_RGBA8Unorm || format == WGPUTextureFormat_BGRA8Unorm;
}

void draw(ModContext*, const GfxDrawContext* ctx, const void* bytes, size_t size, void*) {
    if (size != sizeof(Payload) || ctx == nullptr)
        return;
    Payload payload{};
    std::memcpy(&payload, bytes, sizeof(payload));
    if (payload.kind > 1)
        return;
    const auto& selected = pipelines[payload.kind];
    if (selected.pipeline == nullptr || selected.bind_layout == nullptr)
        return;
    if (payload.scene == nullptr || payload.layout_key != ctx->layout.key ||
        payload.layout_key != layout.key || ctx->layout.sample_count != 1)
        return;
    WGPUBindGroupEntry entries[2] = {WGPU_BIND_GROUP_ENTRY_INIT, WGPU_BIND_GROUP_ENTRY_INIT};
    entries[0].binding = 0;
    entries[0].textureView = payload.scene;
    if (payload.kind == 1) {
        if (payload.uniform_size != sizeof(grade::Uniforms))
            return;
        entries[1].binding = 1;
        entries[1].buffer = ctx->uniform_buffer;
        entries[1].offset = payload.uniform_offset;
        entries[1].size = payload.uniform_size;
    }
    WGPUBindGroupDescriptor desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    desc.label = {"MidnaFX scene", WGPU_STRLEN};
    desc.layout = selected.bind_layout;
    desc.entryCount = payload.kind == 1 ? 2 : 1;
    desc.entries = entries;
    auto group = wgpuDeviceCreateBindGroup(ctx->device, &desc);
    if (group == nullptr) {
        state.store(2, std::memory_order_release);
        return;
    }
    groups.fetch_add(1, std::memory_order_relaxed);
    const auto target_width = ctx->layout.color_attachments[0].width;
    const auto target_height = ctx->layout.color_attachments[0].height;
    wgpuRenderPassEncoderSetViewport(ctx->pass, 0, 0, static_cast<float>(target_width),
                                     static_cast<float>(target_height), 0, 1);
    wgpuRenderPassEncoderSetScissorRect(ctx->pass, 0, 0, target_width, target_height);
    wgpuRenderPassEncoderSetPipeline(ctx->pass, selected.pipeline);
    wgpuRenderPassEncoderSetBindGroup(ctx->pass, 0, group, 0, nullptr);
    wgpuRenderPassEncoderDraw(ctx->pass, 3, 1, 0, 0);
    encoded.fetch_add(1, std::memory_order_relaxed);
    wgpuBindGroupRelease(group);
}

void stage(ModContext*, const GfxStageContext* stage_ctx, void*) {
    const auto current_state = state.load(std::memory_order_acquire);
    if ((current_state != 1 && current_state != 3) || !settings::enabled() ||
        stage_ctx == nullptr || stage_ctx->stage != GFX_STAGE_FRAME_BEFORE_HUD)
        return;
    const auto prepared = settings::prepared_grade();
    const bool passthrough_test = settings::passthrough_test();
    if (prepared.neutral && !passthrough_test)
        return;
    const bool timing = settings::diagnostics_enabled();
    const auto start =
        timing ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    GfxRenderTargetLayout current = GFX_RENDER_TARGET_LAYOUT_INIT;
    const auto layout_result = svc_gfx->get_scene_target_layout(mod_ctx, &current);
    if (layout_result == MOD_UNAVAILABLE)
        return;
    if (layout_result != MOD_OK) {
        state.store(2, std::memory_order_release);
        return;
    }
    if (!supported(current) || current.key != layout.key) {
        state.store(3, std::memory_order_release);
        return;
    }
    std::uint32_t skipped = 3;
    (void)state.compare_exchange_strong(skipped, 1, std::memory_order_acq_rel);
    if (state.load(std::memory_order_acquire) != 1)
        return;
    GfxRange uniform_range{0, 0};
    if (!passthrough_test) {
        const auto uniform_result = svc_gfx->push_uniform(
            mod_ctx, &prepared.uniforms, sizeof(prepared.uniforms), &uniform_range);
        if (uniform_result == MOD_UNAVAILABLE)
            return;
        if (uniform_result != MOD_OK) {
            state.store(2, std::memory_order_release);
            return;
        }
    }
    GfxResolveDesc request = GFX_RESOLVE_DESC_INIT;
    request.color = true;
    request.depth = false;
    GfxResolvedTargets snapshot = GFX_RESOLVED_TARGETS_INIT;
    const auto resolve_result = svc_gfx->resolve_pass(mod_ctx, &request, &snapshot);
    if (resolve_result == MOD_UNAVAILABLE)
        return;
    if (resolve_result != MOD_OK || snapshot.color == nullptr) {
        state.store(2, std::memory_order_release);
        return;
    }
    if (snapshot.color_format != layout.color_attachments[0].format ||
        snapshot.width != current.color_attachments[0].width ||
        snapshot.height != current.color_attachments[0].height) {
        state.store(3, std::memory_order_release);
        return;
    }
    width.store(snapshot.width, std::memory_order_relaxed);
    height.store(snapshot.height, std::memory_order_relaxed);
    const Payload payload{snapshot.color, current.key, passthrough_test ? 0u : 1u,
                          uniform_range.offset, uniform_range.size};
    const auto push_result = svc_gfx->push_draw(mod_ctx, draw_type, &payload, sizeof(payload));
    if (push_result == MOD_UNAVAILABLE)
        return;
    if (push_result != MOD_OK) {
        state.store(2, std::memory_order_release);
        return;
    }
    submitted.fetch_add(1, std::memory_order_relaxed);
    if (timing) {
        callback_us.store(
            std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start)
                .count(),
            std::memory_order_relaxed);
    }
}

void release_gpu() {
    for (auto& selected : pipelines) {
        if (selected.bind_layout != nullptr) {
            wgpuBindGroupLayoutRelease(selected.bind_layout);
            selected.bind_layout = nullptr;
        }
        if (selected.pipeline != nullptr) {
            wgpuRenderPipelineRelease(selected.pipeline);
            selected.pipeline = nullptr;
        }
        if (selected.shader != nullptr) {
            wgpuShaderModuleRelease(selected.shader);
            selected.shader = nullptr;
        }
    }
}

void build_pipeline(Pipeline& selected, const char* source, const char* label) {
    WGPUShaderSourceWGSL wgsl = WGPU_SHADER_SOURCE_WGSL_INIT;
    wgsl.code = {source, WGPU_STRLEN};
    WGPUShaderModuleDescriptor shader_desc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    shader_desc.nextInChain = &wgsl.chain;
    shader_desc.label = {label, WGPU_STRLEN};
    selected.shader = wgpuDeviceCreateShaderModule(device.device, &shader_desc);
    if (selected.shader == nullptr)
        return;
    WGPUColorTargetState colors[GFX_MAX_COLOR_ATTACHMENTS];
    const auto count =
        gfx_init_color_target_states(&layout, colors, nullptr, WGPUColorWriteMask_All);
    WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
    fragment.module = selected.shader;
    fragment.entryPoint = {"fs_main", WGPU_STRLEN};
    fragment.targetCount = count;
    fragment.targets = colors;
    WGPUDepthStencilState depth = WGPU_DEPTH_STENCIL_STATE_INIT;
    depth.format = layout.depth_stencil_format;
    depth.depthWriteEnabled = WGPUOptionalBool_False;
    depth.depthCompare = WGPUCompareFunction_Always;
    WGPURenderPipelineDescriptor desc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    desc.label = {label, WGPU_STRLEN};
    desc.vertex.module = selected.shader;
    desc.vertex.entryPoint = {"vs_main", WGPU_STRLEN};
    desc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    desc.depthStencil =
        layout.depth_stencil_format == WGPUTextureFormat_Undefined ? nullptr : &depth;
    desc.multisample.count = layout.sample_count;
    desc.fragment = &fragment;
    selected.pipeline = wgpuDeviceCreateRenderPipeline(device.device, &desc);
    builds.fetch_add(1);
    if (selected.pipeline != nullptr)
        selected.bind_layout = wgpuRenderPipelineGetBindGroupLayout(selected.pipeline, 0);
}
} // namespace

void initialize() {
    state.store(0);
    warned = false;
    next_init_retry = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
    if (svc_gfx == nullptr)
        return;
    if (svc_gfx->get_device_info(mod_ctx, &device) != MOD_OK || device.device == nullptr ||
        device.instance == nullptr) {
        state.store(4);
        return;
    }
    if (svc_gfx->get_scene_target_layout(mod_ctx, &layout) != MOD_OK) {
        state.store(4);
        return;
    }
    if (!supported(layout)) {
        state.store(5);
        return;
    }
    wgpuDevicePushErrorScope(device.device, WGPUErrorFilter_OutOfMemory);
    wgpuDevicePushErrorScope(device.device, WGPUErrorFilter_Internal);
    wgpuDevicePushErrorScope(device.device, WGPUErrorFilter_Validation);
    build_pipeline(pipelines[0], passthrough_shader, "MidnaFX passthrough");
    build_pipeline(pipelines[1], grading_shader, "MidnaFX fused grading");
    const bool validation_ok = pop_scope(device.device, device.instance);
    const bool internal_ok = pop_scope(device.device, device.instance);
    const bool memory_ok = pop_scope(device.device, device.instance);
    if (!validation_ok || !internal_ok || !memory_ok || pipelines[0].pipeline == nullptr ||
        pipelines[0].bind_layout == nullptr || pipelines[1].pipeline == nullptr ||
        pipelines[1].bind_layout == nullptr) {
        state.store(2);
        return;
    }
    GfxDrawTypeDesc draw_desc = GFX_DRAW_TYPE_DESC_INIT;
    draw_desc.label = "MidnaFX fullscreen grading";
    draw_desc.draw = draw;
    if (svc_gfx->register_draw_type(mod_ctx, &draw_desc, &draw_type) != MOD_OK) {
        state.store(2);
        return;
    }
    GfxStageHookDesc hook_desc = GFX_STAGE_HOOK_DESC_INIT;
    hook_desc.callback = stage;
    if (svc_gfx->register_stage_hook(mod_ctx, GFX_STAGE_FRAME_BEFORE_HUD, &hook_desc,
                                     &stage_hook) != MOD_OK) {
        state.store(2);
        return;
    }
    state.store(1, std::memory_order_release);
}

void update() {
    const auto current_state = state.load(std::memory_order_acquire);
    if ((current_state == 4 || current_state == 5) && settings::enabled()) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= next_init_retry)
            initialize();
    }
    if (!warned && state.load(std::memory_order_acquire) == 2 && svc_log != nullptr) {
        svc_log->error(mod_ctx, "MidnaFX graphics path failed and was disabled");
        warned = true;
    }
}

void shutdown() {
    state.store(0);
    if (svc_gfx != nullptr && stage_hook != 0)
        svc_gfx->unregister_stage_hook(mod_ctx, stage_hook);
    if (svc_gfx != nullptr && draw_type != 0)
        svc_gfx->unregister_draw_type(mod_ctx, draw_type);
    stage_hook = 0;
    draw_type = 0;
    release_gpu();
}

Diagnostics diagnostics() {
    const char* status_text = "Graphics service unavailable";
    switch (state.load(std::memory_order_acquire)) {
    case 1:
        status_text = "Pre-HUD grading ready";
        break;
    case 2:
        status_text = "Graphics operation failed; bypassed";
        break;
    case 3:
        status_text = "Unsupported scene layout or MSAA; bypassed";
        break;
    case 4:
        status_text = "GPU unavailable; waiting to initialize";
        break;
    case 5:
        status_text = "Unsupported initial scene layout or MSAA; waiting";
        break;
    }
    return {status_text,
            width.load(),
            height.load(),
            submitted.load(),
            encoded.load(),
            groups.load(),
            builds.load(),
            callback_us.load(),
            settings::diagnostics_enabled(),
            "Dawn backend not exposed by GfxService"};
}
} // namespace midnafx::render
