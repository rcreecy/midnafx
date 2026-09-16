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
};
static_assert(sizeof(Payload) <= GFX_INLINE_DRAW_PAYLOAD_SIZE);

GfxDeviceInfo device = GFX_DEVICE_INFO_INIT;
GfxRenderTargetLayout layout = GFX_RENDER_TARGET_LAYOUT_INIT;
WGPUShaderModule shader = nullptr;
WGPURenderPipeline pipeline = nullptr;
WGPUBindGroupLayout bind_layout = nullptr;
GfxStageHookHandle stage_hook = 0;
GfxDrawTypeHandle draw_type = 0;
std::atomic<std::uint32_t> state{0}; // 0 unavailable, 1 ready, 2 failed, 3 unsupported
std::atomic<std::uint32_t> width{0}, height{0};
std::atomic<std::uint64_t> submitted{0}, encoded{0}, groups{0}, builds{0};
std::atomic<double> callback_us{0.0};
bool warned = false;

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
            // A wait error cannot cancel the callback. Keep its storage live and retry.
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } else if (!wait.completed) {
            std::this_thread::yield();
        }
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
    if (size != sizeof(Payload) || ctx == nullptr || pipeline == nullptr || bind_layout == nullptr)
        return;
    Payload payload{};
    std::memcpy(&payload, bytes, sizeof(payload));
    if (payload.scene == nullptr || payload.layout_key != ctx->layout.key ||
        payload.layout_key != layout.key || ctx->layout.sample_count != 1)
        return;
    WGPUBindGroupEntry entry = WGPU_BIND_GROUP_ENTRY_INIT;
    entry.binding = 0;
    entry.textureView = payload.scene;
    WGPUBindGroupDescriptor desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    desc.label = {"MidnaFX M1 scene", WGPU_STRLEN};
    desc.layout = bind_layout;
    desc.entryCount = 1;
    desc.entries = &entry;
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
    wgpuRenderPassEncoderSetPipeline(ctx->pass, pipeline);
    wgpuRenderPassEncoderSetBindGroup(ctx->pass, 0, group, 0, nullptr);
    wgpuRenderPassEncoderDraw(ctx->pass, 3, 1, 0, 0);
    encoded.fetch_add(1, std::memory_order_relaxed);
    wgpuBindGroupRelease(group);
}

void stage(ModContext*, const GfxStageContext* stage_ctx, void*) {
    if (state.load(std::memory_order_acquire) != 1 || !settings::enabled() ||
        stage_ctx == nullptr || stage_ctx->stage != GFX_STAGE_FRAME_BEFORE_HUD)
        return;
    const bool timing = settings::diagnostics_enabled();
    const auto start =
        timing ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    GfxRenderTargetLayout current = GFX_RENDER_TARGET_LAYOUT_INIT;
    if (svc_gfx->get_scene_target_layout(mod_ctx, &current) != MOD_OK || !supported(current) ||
        current.key != layout.key) {
        state.store(3, std::memory_order_release);
        return;
    }
    GfxResolveDesc request = GFX_RESOLVE_DESC_INIT;
    request.color = true;
    request.depth = false;
    GfxResolvedTargets snapshot = GFX_RESOLVED_TARGETS_INIT;
    if (svc_gfx->resolve_pass(mod_ctx, &request, &snapshot) != MOD_OK ||
        snapshot.color == nullptr || snapshot.color_format != layout.color_attachments[0].format ||
        snapshot.width != current.color_attachments[0].width ||
        snapshot.height != current.color_attachments[0].height) {
        state.store(2, std::memory_order_release);
        return;
    }
    width.store(snapshot.width, std::memory_order_relaxed);
    height.store(snapshot.height, std::memory_order_relaxed);
    const Payload payload{snapshot.color, current.key};
    if (svc_gfx->push_draw(mod_ctx, draw_type, &payload, sizeof(payload)) != MOD_OK) {
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
    if (bind_layout != nullptr) {
        wgpuBindGroupLayoutRelease(bind_layout);
        bind_layout = nullptr;
    }
    if (pipeline != nullptr) {
        wgpuRenderPipelineRelease(pipeline);
        pipeline = nullptr;
    }
    if (shader != nullptr) {
        wgpuShaderModuleRelease(shader);
        shader = nullptr;
    }
}
} // namespace

void initialize() {
    state.store(0);
    warned = false;
    if (svc_gfx == nullptr || svc_gfx->get_device_info(mod_ctx, &device) != MOD_OK ||
        svc_gfx->get_scene_target_layout(mod_ctx, &layout) != MOD_OK)
        return;
    if (!supported(layout)) {
        state.store(3);
        return;
    }
    wgpuDevicePushErrorScope(device.device, WGPUErrorFilter_OutOfMemory);
    wgpuDevicePushErrorScope(device.device, WGPUErrorFilter_Internal);
    wgpuDevicePushErrorScope(device.device, WGPUErrorFilter_Validation);
    WGPUShaderSourceWGSL wgsl = WGPU_SHADER_SOURCE_WGSL_INIT;
    wgsl.code = {passthrough_shader, WGPU_STRLEN};
    WGPUShaderModuleDescriptor shader_desc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    shader_desc.nextInChain = &wgsl.chain;
    shader_desc.label = {"MidnaFX M1 passthrough", WGPU_STRLEN};
    shader = wgpuDeviceCreateShaderModule(device.device, &shader_desc);
    WGPUColorTargetState colors[GFX_MAX_COLOR_ATTACHMENTS];
    const auto count =
        gfx_init_color_target_states(&layout, colors, nullptr, WGPUColorWriteMask_All);
    WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
    fragment.module = shader;
    fragment.entryPoint = {"fs_main", WGPU_STRLEN};
    fragment.targetCount = count;
    fragment.targets = colors;
    WGPUDepthStencilState depth = WGPU_DEPTH_STENCIL_STATE_INIT;
    depth.format = layout.depth_stencil_format;
    depth.depthWriteEnabled = WGPUOptionalBool_False;
    depth.depthCompare = WGPUCompareFunction_Always;
    WGPURenderPipelineDescriptor desc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    desc.label = {"MidnaFX M1 passthrough", WGPU_STRLEN};
    desc.vertex.module = shader;
    desc.vertex.entryPoint = {"vs_main", WGPU_STRLEN};
    desc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    desc.depthStencil =
        layout.depth_stencil_format == WGPUTextureFormat_Undefined ? nullptr : &depth;
    desc.multisample.count = layout.sample_count;
    desc.fragment = &fragment;
    pipeline = wgpuDeviceCreateRenderPipeline(device.device, &desc);
    builds.fetch_add(1);
    if (pipeline != nullptr)
        bind_layout = wgpuRenderPipelineGetBindGroupLayout(pipeline, 0);
    const bool validation_ok = pop_scope(device.device, device.instance);
    const bool internal_ok = pop_scope(device.device, device.instance);
    const bool memory_ok = pop_scope(device.device, device.instance);
    if (!validation_ok || !internal_ok || !memory_ok || shader == nullptr || pipeline == nullptr ||
        bind_layout == nullptr) {
        state.store(2);
        return;
    }
    GfxDrawTypeDesc draw_desc = GFX_DRAW_TYPE_DESC_INIT;
    draw_desc.label = "MidnaFX M1 passthrough";
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
        status_text = "Pre-HUD passthrough ready";
        break;
    case 2:
        status_text = "Graphics operation failed; bypassed";
        break;
    case 3:
        status_text = "Unsupported scene layout or MSAA; bypassed";
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
