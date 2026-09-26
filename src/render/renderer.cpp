#include "renderer.hpp"
#include "config/visual.hpp"
#include "game/camera_probe.hpp"
#include "midnafx_shader.hpp"
#include "services.hpp"
#include "ui/settings.hpp"
#include <mods/svc/gfx.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

namespace midnafx::render {
namespace {
struct PipelinePair;
enum PipelineKind : std::uint32_t {
    Passthrough = 0,
    Grade = 1,
    Detail = 2,
    Debug = 3,
    DebugDetail = 4,
    PipelineCount = 5,
};
struct Payload {
    WGPUTextureView scene;
    const PipelinePair* pair;
    std::uint64_t layout_key;
    std::uint32_t kind;
    std::uint32_t uniform_offset;
    std::uint32_t uniform_size;
};
static_assert(sizeof(Payload) <= GFX_INLINE_DRAW_PAYLOAD_SIZE);

GfxDeviceInfo device = GFX_DEVICE_INFO_INIT;
struct Pipeline {
    WGPURenderPipeline pipeline = nullptr;
    WGPUBindGroupLayout bind_layout = nullptr;
};
struct PipelinePair {
    GfxRenderTargetLayout layout = GFX_RENDER_TARGET_LAYOUT_INIT;
    WGPUShaderModule shaders[2]{};
    Pipeline pipelines[PipelineCount];
};
// Pairs stay alive until the host has drained draw callbacks at shutdown.
std::vector<std::unique_ptr<PipelinePair>> pairs;
GfxStageHookHandle stage_hook = 0;
GfxDrawTypeHandle draw_type = 0;
std::atomic<std::uint32_t> state{
    0}; // 0 unavailable, 1 ready, 2 failed, 3 layout skip, 4 GPU pending, 5 init layout skip
std::atomic<std::uint32_t> width{0}, height{0};
std::atomic<std::uint64_t> submitted{0}, encoded{0}, groups{0}, builds{0};
std::atomic<std::uint64_t> disabled_samples{0}, neutral_samples{0}, snapshot_requests{0};
std::atomic<double> callback_us{0.0}, disabled_us{0.0}, layout_us{0.0}, resolve_us{0.0};
struct SampleWindow {
    static constexpr unsigned Capacity = 256;
    std::array<double, Capacity> values{};
    unsigned next = 0, count = 0;
    void add(double value) {
        values[next] = value;
        next = (next + 1) % Capacity;
        count = std::min(count + 1, Capacity);
    }
    std::pair<double, double> percentiles() const {
        if (count == 0)
            return {0.0, 0.0};
        auto sorted = values;
        std::sort(sorted.begin(), sorted.begin() + count);
        return {sorted[(count - 1) / 2], sorted[(count - 1) * 95 / 100]};
    }
};
SampleWindow active_times, disabled_times;
bool warned = false;
bool depth_probe_completed = false;
std::chrono::steady_clock::time_point next_init_retry{};
std::chrono::steady_clock::time_point next_layout_retry{};

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

const char* invalid_camera_field(const CameraInfo& camera) {
    const char* names[] = {"view_from_world", "world_from_view", "proj_from_view",
                           "view_from_proj", "proj_from_world", "world_from_proj"};
    const float* matrices[] = {camera.view_from_world, camera.world_from_view,
                               camera.proj_from_view, camera.view_from_proj,
                               camera.proj_from_world, camera.world_from_proj};
    for (unsigned matrix_index = 0; matrix_index < 6; ++matrix_index)
        for (unsigned i = 0; i < 16; ++i)
            if (!std::isfinite(matrices[matrix_index][i]))
                return names[matrix_index];
    if (!std::isfinite(camera.eye[0]) || !std::isfinite(camera.eye[1]) ||
        !std::isfinite(camera.eye[2]))
        return "eye";
    if (!std::isfinite(camera.fovy))
        return "fovy";
    if (!std::isfinite(camera.aspect))
        return "aspect";
    if (!std::isfinite(camera.near_plane))
        return "near_plane";
    if (!std::isfinite(camera.far_plane))
        return "far_plane";
    return nullptr;
}

void run_depth_probe(const GfxStageContext* stage_ctx) {
    if (!settings::atmosphere_depth_probe_enabled()) {
        depth_probe_completed = false;
        return;
    }
    if (depth_probe_completed || svc_log == nullptr)
        return;
    CameraInfo camera = CAMERA_INFO_INIT;
    if (!camera_probe::latest_camera_info(camera))
        return;
    const auto* invalid_field = invalid_camera_field(camera);
    if (invalid_field != nullptr) {
        char message[160];
        std::snprintf(message, sizeof(message),
                      "Atmosphere depth probe: invalid camera snapshot field=%s", invalid_field);
        svc_log->warn(mod_ctx, message);
        depth_probe_completed = true;
        return;
    }
    GfxResolveDesc request = GFX_RESOLVE_DESC_INIT;
    request.color = false;
    request.depth = true;
    GfxResolvedTargets snapshot = GFX_RESOLVED_TARGETS_INIT;
    const auto resolve_result = svc_gfx->resolve_pass(mod_ctx, &request, &snapshot);
    char message[512];
    std::snprintf(message, sizeof(message),
                  "Atmosphere depth probe: depth=%s size=%ux%u reversed_z=%s fovy=%.2f "
                  "aspect=%.3f near=%.3f far=%.1f eye=[%.2f,%.2f,%.2f] matrices=finite",
                  resolve_result == MOD_OK && snapshot.depth != nullptr ? "available" : "unavailable",
                  snapshot.width, snapshot.height, device.uses_reversed_z ? "yes" : "no",
                  camera.fovy, camera.aspect, camera.near_plane, camera.far_plane, camera.eye[0],
                  camera.eye[1], camera.eye[2]);
    svc_log->info(mod_ctx, message);
    depth_probe_completed = true;
}

void release_pair(PipelinePair& pair);
void create_shader(WGPUShaderModule& shader, const char* source, const char* label);
void build_pipeline(Pipeline& selected, WGPUShaderModule shader, const char* entry_point,
                    const char* label, const GfxRenderTargetLayout& layout);
bool create_pair(const GfxRenderTargetLayout& layout) {
    auto next = std::make_unique<PipelinePair>();
    next->layout = layout;
    wgpuDevicePushErrorScope(device.device, WGPUErrorFilter_OutOfMemory);
    wgpuDevicePushErrorScope(device.device, WGPUErrorFilter_Internal);
    wgpuDevicePushErrorScope(device.device, WGPUErrorFilter_Validation);
    create_shader(next->shaders[0], passthrough_shader, "MidnaFX passthrough shader");
    create_shader(next->shaders[1], grading_shader, "MidnaFX grading and detail shader");
    if (next->shaders[0] && next->shaders[1]) {
        build_pipeline(next->pipelines[Passthrough], next->shaders[0], "fs_main",
                       "MidnaFX passthrough", layout);
        build_pipeline(next->pipelines[Grade], next->shaders[1], "fs_main", "MidnaFX grading",
                       layout);
        build_pipeline(next->pipelines[Detail], next->shaders[1], "fs_detail", "MidnaFX detail",
                       layout);
        build_pipeline(next->pipelines[Debug], next->shaders[1], "fs_debug",
                       "MidnaFX visual diagnostic", layout);
        build_pipeline(next->pipelines[DebugDetail], next->shaders[1], "fs_debug_detail",
                       "MidnaFX detail diagnostic", layout);
    }
    const bool validation_ok = pop_scope(device.device, device.instance);
    const bool internal_ok = pop_scope(device.device, device.instance);
    const bool memory_ok = pop_scope(device.device, device.instance);
    bool ok = validation_ok && internal_ok && memory_ok;
    for (const auto& pipeline : next->pipelines)
        ok &= pipeline.pipeline != nullptr && pipeline.bind_layout != nullptr;
    if (!ok) {
        release_pair(*next);
        return false;
    }
    pairs.push_back(std::move(next));
    return true;
}

void draw(ModContext*, const GfxDrawContext* ctx, const void* bytes, size_t size, void*) {
    if (size != sizeof(Payload) || ctx == nullptr)
        return;
    Payload payload{};
    std::memcpy(&payload, bytes, sizeof(payload));
    if (payload.kind >= PipelineCount)
        return;
    if (payload.pair == nullptr)
        return;
    const auto& selected = payload.pair->pipelines[payload.kind];
    if (selected.pipeline == nullptr || selected.bind_layout == nullptr)
        return;
    if (payload.scene == nullptr || payload.layout_key != ctx->layout.key ||
        payload.layout_key != payload.pair->layout.key || ctx->layout.sample_count != 1)
        return;
    WGPUBindGroupEntry entries[2] = {WGPU_BIND_GROUP_ENTRY_INIT, WGPU_BIND_GROUP_ENTRY_INIT};
    entries[0].binding = 0;
    entries[0].textureView = payload.scene;
    if (payload.kind != Passthrough) {
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
    desc.entryCount = payload.kind == Passthrough ? 1 : 2;
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
    if ((current_state != 1 && current_state != 3) || stage_ctx == nullptr ||
        stage_ctx->stage != GFX_STAGE_FRAME_BEFORE_HUD)
        return;
    run_depth_probe(stage_ctx);
    const bool timing = settings::diagnostics_enabled();
    const auto start =
        timing ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    if (!settings::enabled()) {
        if (timing) {
            disabled_samples.fetch_add(1, std::memory_order_relaxed);
            const auto elapsed =
                std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start)
                    .count();
            disabled_us.store(elapsed, std::memory_order_relaxed);
            disabled_times.add(elapsed);
        }
        return;
    }
    auto prepared = settings::prepared_grade();
    const auto mode = visual::debug_mode(prepared.uniforms.debug_mode);
    const bool passthrough_test =
        settings::passthrough_test() || mode == visual::DebugMode::Passthrough;
    const bool detail_enabled = prepared.uniforms.detail_strength > 0.0f;
    if (prepared.neutral && !detail_enabled && !passthrough_test &&
        mode == visual::DebugMode::Final) {
        if (timing)
            neutral_samples.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    GfxRenderTargetLayout current = GFX_RENDER_TARGET_LAYOUT_INIT;
    const auto before_layout =
        timing ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const auto layout_result = svc_gfx->get_scene_target_layout(mod_ctx, &current);
    if (timing)
        layout_us.store(std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() -
                                                                  before_layout)
                            .count(),
                        std::memory_order_relaxed);
    if (layout_result == MOD_UNAVAILABLE)
        return;
    if (layout_result != MOD_OK) {
        state.store(2, std::memory_order_release);
        return;
    }
    if (!supported(current)) {
        state.store(3, std::memory_order_release);
        return;
    }
    const PipelinePair* pair = nullptr;
    for (const auto& candidate : pairs)
        if (candidate->layout.key == current.key) {
            pair = candidate.get();
            break;
        }
    if (pair == nullptr) {
        next_layout_retry = std::chrono::steady_clock::now();
        state.store(3, std::memory_order_release);
        return;
    }
    std::uint32_t skipped = 3;
    (void)state.compare_exchange_strong(skipped, 1, std::memory_order_acq_rel);
    if (state.load(std::memory_order_acquire) != 1)
        return;
    std::uint32_t kind = Passthrough;
    if (!passthrough_test)
        kind = mode == visual::DebugMode::Final ? (detail_enabled ? Detail : Grade)
                                                : (detail_enabled ? DebugDetail : Debug);
    prepared.uniforms.split_x =
        visual::split_boundary(current.color_attachments[0].width, settings::split_percent());
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
    const auto before_resolve =
        timing ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const auto resolve_result = svc_gfx->resolve_pass(mod_ctx, &request, &snapshot);
    if (timing)
        resolve_us.store(std::chrono::duration<double, std::micro>(
                             std::chrono::steady_clock::now() - before_resolve)
                             .count(),
                         std::memory_order_relaxed);
    if (resolve_result == MOD_UNAVAILABLE)
        return;
    if (resolve_result != MOD_OK || snapshot.color == nullptr) {
        state.store(2, std::memory_order_release);
        return;
    }
    if (snapshot.color_format != current.color_attachments[0].format ||
        snapshot.width != current.color_attachments[0].width ||
        snapshot.height != current.color_attachments[0].height) {
        state.store(3, std::memory_order_release);
        return;
    }
    if (timing)
        snapshot_requests.fetch_add(1, std::memory_order_relaxed);
    width.store(snapshot.width, std::memory_order_relaxed);
    height.store(snapshot.height, std::memory_order_relaxed);
    const Payload payload{snapshot.color,    pair, current.key, kind, uniform_range.offset,
                          uniform_range.size};
    const auto push_result = svc_gfx->push_draw(mod_ctx, draw_type, &payload, sizeof(payload));
    if (push_result == MOD_UNAVAILABLE)
        return;
    if (push_result != MOD_OK) {
        state.store(2, std::memory_order_release);
        return;
    }
    submitted.fetch_add(1, std::memory_order_relaxed);
    if (timing) {
        const auto elapsed =
            std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start)
                .count();
        callback_us.store(elapsed, std::memory_order_relaxed);
        active_times.add(elapsed);
    }
}

void release_pair(PipelinePair& pair) {
    for (auto& selected : pair.pipelines) {
        if (selected.bind_layout != nullptr) {
            wgpuBindGroupLayoutRelease(selected.bind_layout);
            selected.bind_layout = nullptr;
        }
        if (selected.pipeline != nullptr) {
            wgpuRenderPipelineRelease(selected.pipeline);
            selected.pipeline = nullptr;
        }
    }
    for (auto& shader : pair.shaders)
        if (shader != nullptr) {
            wgpuShaderModuleRelease(shader);
            shader = nullptr;
        }
}

void release_gpu() {
    for (auto& pair : pairs)
        release_pair(*pair);
    pairs.clear();
}

void create_shader(WGPUShaderModule& shader, const char* source, const char* label) {
    WGPUShaderSourceWGSL wgsl = WGPU_SHADER_SOURCE_WGSL_INIT;
    wgsl.code = {source, WGPU_STRLEN};
    WGPUShaderModuleDescriptor shader_desc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    shader_desc.nextInChain = &wgsl.chain;
    shader_desc.label = {label, WGPU_STRLEN};
    shader = wgpuDeviceCreateShaderModule(device.device, &shader_desc);
}

void build_pipeline(Pipeline& selected, WGPUShaderModule shader, const char* entry_point,
                    const char* label, const GfxRenderTargetLayout& layout) {
    if (shader == nullptr)
        return;
    WGPUColorTargetState colors[GFX_MAX_COLOR_ATTACHMENTS];
    const auto count =
        gfx_init_color_target_states(&layout, colors, nullptr, WGPUColorWriteMask_All);
    WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
    fragment.module = shader;
    fragment.entryPoint = {entry_point, WGPU_STRLEN};
    fragment.targetCount = count;
    fragment.targets = colors;
    WGPUDepthStencilState depth = WGPU_DEPTH_STENCIL_STATE_INIT;
    depth.format = layout.depth_stencil_format;
    depth.depthWriteEnabled = WGPUOptionalBool_False;
    depth.depthCompare = WGPUCompareFunction_Always;
    WGPURenderPipelineDescriptor desc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    desc.label = {label, WGPU_STRLEN};
    desc.vertex.module = shader;
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
    reset_timing_samples();
    warned = false;
    depth_probe_completed = false;
    next_init_retry = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
    next_layout_retry = std::chrono::steady_clock::now();
    if (svc_gfx == nullptr)
        return;
    if (svc_gfx->get_device_info(mod_ctx, &device) != MOD_OK || device.device == nullptr ||
        device.instance == nullptr) {
        state.store(4);
        return;
    }
    GfxRenderTargetLayout layout = GFX_RENDER_TARGET_LAYOUT_INIT;
    if (svc_gfx->get_scene_target_layout(mod_ctx, &layout) != MOD_OK) {
        state.store(4);
        return;
    }
    if (!supported(layout)) {
        state.store(5);
        return;
    }
    if (!create_pair(layout)) {
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
    if (current_state == 3 && settings::enabled() && svc_gfx != nullptr) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= next_layout_retry) {
            next_layout_retry = now + std::chrono::milliseconds(250);
            GfxRenderTargetLayout current = GFX_RENDER_TARGET_LAYOUT_INIT;
            if (svc_gfx->get_scene_target_layout(mod_ctx, &current) == MOD_OK &&
                supported(current)) {
                bool found = false;
                for (const auto& pair : pairs)
                    found |= pair->layout.key == current.key;
                if (found || create_pair(current))
                    state.store(1, std::memory_order_release);
                else
                    state.store(2, std::memory_order_release);
            }
        }
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

void reset_timing_samples() {
    active_times = {};
    disabled_times = {};
    callback_us.store(0.0, std::memory_order_relaxed);
    disabled_us.store(0.0, std::memory_order_relaxed);
    layout_us.store(0.0, std::memory_order_relaxed);
    resolve_us.store(0.0, std::memory_order_relaxed);
    disabled_samples.store(0, std::memory_order_relaxed);
    neutral_samples.store(0, std::memory_order_relaxed);
    snapshot_requests.store(0, std::memory_order_relaxed);
}

std::uint64_t timing_sample_count() {
    return std::max({disabled_samples.load(std::memory_order_relaxed),
                     neutral_samples.load(std::memory_order_relaxed),
                     snapshot_requests.load(std::memory_order_relaxed)});
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
    const bool timing = settings::diagnostics_enabled();
    const auto active_percentiles =
        timing ? active_times.percentiles() : std::pair<double, double>{};
    const auto disabled_percentiles =
        timing ? disabled_times.percentiles() : std::pair<double, double>{};
    return {status_text,
            width.load(),
            height.load(),
            submitted.load(),
            encoded.load(),
            groups.load(),
            builds.load(),
            disabled_samples.load(),
            neutral_samples.load(),
            snapshot_requests.load(),
            callback_us.load(),
            disabled_us.load(),
            layout_us.load(),
            resolve_us.load(),
            active_percentiles.first,
            active_percentiles.second,
            disabled_percentiles.first,
            disabled_percentiles.second,
            timing,
            "Dawn backend not exposed by GfxService"};
}
} // namespace midnafx::render
