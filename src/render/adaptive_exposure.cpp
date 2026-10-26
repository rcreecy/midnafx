#include "adaptive_exposure.hpp"
#include "midnafx_shader.hpp"
#include "services.hpp"
#include <d/d_com_inf_game.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>
#include <utility>

namespace midnafx::render::adaptive_exposure {
namespace {
struct Controls { float seconds; std::uint32_t reset; float padding[2]{}; };
static_assert(sizeof(Controls) == 16);
struct Payload {
    WGPUTextureView scene, previous, output;
    std::uint32_t offset, size;
};
static_assert(sizeof(Payload) <= GFX_INLINE_DRAW_PAYLOAD_SIZE);
GfxDeviceInfo device = GFX_DEVICE_INFO_INIT;
WGPUTexture textures[3]{};
WGPUTextureView views[3]{};
WGPUComputePipeline pipeline = nullptr;
WGPUBindGroupLayout layout = nullptr;
GfxComputeTypeHandle compute_type = 0;
std::atomic<bool> failed{false};
std::atomic<std::uint64_t> encoded{0};
unsigned current = 0;
bool initialized = false;
bool warned = false;
std::uint64_t last_layout = 0;
std::array<char, 8> last_stage{};
int last_room = -1;
std::chrono::steady_clock::time_point last_time{};

void begin_scopes() {
    wgpuDevicePushErrorScope(device.device, WGPUErrorFilter_OutOfMemory);
    wgpuDevicePushErrorScope(device.device, WGPUErrorFilter_Internal);
    wgpuDevicePushErrorScope(device.device, WGPUErrorFilter_Validation);
}
bool end_scopes() {
    bool valid = true;
    for (unsigned i = 0; i < 3; ++i) {
        bool ok = false;
        WGPUPopErrorScopeCallbackInfo callback = WGPU_POP_ERROR_SCOPE_CALLBACK_INFO_INIT;
        callback.mode = WGPUCallbackMode_WaitAnyOnly;
        callback.userdata1 = &ok;
        callback.callback = [](WGPUPopErrorScopeStatus status, WGPUErrorType type,
                               WGPUStringView, void* user, void*) {
            *static_cast<bool*>(user) = status == WGPUPopErrorScopeStatus_Success &&
                                       type == WGPUErrorType_NoError;
        };
        WGPUFutureWaitInfo wait = WGPU_FUTURE_WAIT_INFO_INIT;
        wait.future = wgpuDevicePopErrorScope(device.device, callback);
        while (!wait.completed) {
            const auto status = wgpuInstanceWaitAny(device.instance, 1, &wait, 0);
            if (status != WGPUWaitStatus_Success && status != WGPUWaitStatus_TimedOut) break;
            if (!wait.completed) std::this_thread::yield();
        }
        valid &= wait.completed && ok;
    }
    return valid;
}

void compute(ModContext*, const GfxComputeContext* ctx, const void* bytes, size_t size, void*) {
    if (!ctx || size != sizeof(Payload) || !pipeline || failed.load()) return;
    Payload payload{};
    std::memcpy(&payload, bytes, sizeof(payload));
    WGPUBindGroupEntry entries[4];
    for (auto& entry : entries) entry = WGPU_BIND_GROUP_ENTRY_INIT;
    entries[0].binding = 0; entries[0].textureView = payload.scene;
    entries[1].binding = 1; entries[1].textureView = payload.previous;
    entries[2].binding = 2; entries[2].textureView = payload.output;
    entries[3].binding = 3; entries[3].buffer = ctx->uniform_buffer;
    entries[3].offset = payload.offset; entries[3].size = payload.size;
    WGPUBindGroupDescriptor desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    desc.layout = layout; desc.entryCount = 4; desc.entries = entries;
    auto group = wgpuDeviceCreateBindGroup(ctx->device, &desc);
    if (!group) { failed.store(true); return; }
    WGPUComputePassDescriptor pass_desc = WGPU_COMPUTE_PASS_DESCRIPTOR_INIT;
    pass_desc.label = {"MidnaFX adaptive exposure meter", WGPU_STRLEN};
    auto pass = wgpuCommandEncoderBeginComputePass(ctx->encoder, &pass_desc);
    wgpuComputePassEncoderSetPipeline(pass, pipeline);
    wgpuComputePassEncoderSetBindGroup(pass, 0, group, 0, nullptr);
    wgpuComputePassEncoderDispatchWorkgroups(pass, 1, 1, 1);
    wgpuComputePassEncoderEnd(pass);
    wgpuComputePassEncoderRelease(pass);
    wgpuBindGroupRelease(group);
    encoded.fetch_add(1);
}
} // namespace

void reset() { initialized = false; last_time = {}; }
WGPUTextureView neutral() { return views[2]; }

bool initialize(const GfxDeviceInfo& info) {
    if (views[2]) return true;
    device = info;
    failed.store(false); encoded.store(0); warned = false; current = 0;
    reset(); last_layout = 0; last_stage = {}; last_room = -1;
    begin_scopes();
    bool textures_ok = true;
    for (unsigned i = 0; i < 3; ++i) {
        WGPUTextureDescriptor desc = WGPU_TEXTURE_DESCRIPTOR_INIT;
        desc.label = {"MidnaFX exposure history", WGPU_STRLEN};
        desc.size = {1, 1, 1}; desc.format = WGPUTextureFormat_RGBA16Float;
        desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_StorageBinding;
        // New WebGPU textures are zero-initialized: zero stops is exactly neutral.
        textures[i] = wgpuDeviceCreateTexture(device.device, &desc);
        if (textures[i]) views[i] = wgpuTextureCreateView(textures[i], nullptr);
        if (!views[i]) { textures_ok = false; break; }
    }
    const bool textures_valid = end_scopes();
    if (!textures_ok || !textures_valid) { shutdown(); return false; }
    begin_scopes();
    WGPUShaderSourceWGSL source = WGPU_SHADER_SOURCE_WGSL_INIT;
    source.code = {adaptive_exposure_shader, WGPU_STRLEN};
    WGPUShaderModuleDescriptor shader_desc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    shader_desc.nextInChain = &source.chain;
    auto shader = wgpuDeviceCreateShaderModule(device.device, &shader_desc);
    if (shader) {
        WGPUComputePipelineDescriptor desc = WGPU_COMPUTE_PIPELINE_DESCRIPTOR_INIT;
        desc.compute.module = shader; desc.compute.entryPoint = {"meter", WGPU_STRLEN};
        pipeline = wgpuDeviceCreateComputePipeline(device.device, &desc);
        wgpuShaderModuleRelease(shader);
    }
    if (pipeline) layout = wgpuComputePipelineGetBindGroupLayout(pipeline, 0);
    const bool compute_valid = end_scopes();
    GfxComputeTypeDesc desc = GFX_COMPUTE_TYPE_DESC_INIT;
    desc.label = "MidnaFX adaptive exposure"; desc.callback = compute;
    if (!compute_valid || !layout ||
        svc_gfx->register_compute_type(mod_ctx, &desc, &compute_type) != MOD_OK)
        failed.store(true);
    return true; // Neutral texture remains usable if metering is unavailable.
}

WGPUTextureView prepare(const GfxResolvedTargets& scene, std::uint64_t layout_key, bool active) {
    if (!active || !scene.color || !scene.width || !scene.height || failed.load() || !compute_type ||
        !dComIfGp_getPlayer(0) || dComIfGp_isPauseFlag()) {
        reset();
        if (active && failed.load() && !warned && svc_log) {
            svc_log->warn(mod_ctx, "Adaptive exposure unavailable; original exposure retained");
            warned = true;
        }
        return neutral();
    }
    const auto now = std::chrono::steady_clock::now();
    std::array<char, 8> stage{};
    const auto* name = dComIfGp_getStartStageName();
    if (name) std::memcpy(stage.data(), name, stage.size());
    const int room = dComIfGp_roomControl_getStayNo();
    float elapsed = std::chrono::duration<float>(now - last_time).count();
    const bool restart = !initialized || last_layout != layout_key || stage != last_stage ||
                         room != last_room || elapsed > 0.5f;
    const Controls controls{restart ? 0.0f : std::clamp(elapsed, 0.0f, 0.1f), restart ? 1u : 0u};
    GfxRange range{};
    if (svc_gfx->push_uniform(mod_ctx, &controls, sizeof(controls), &range) != MOD_OK) {
        reset(); return neutral();
    }
    const unsigned output = 1 - current;
    const Payload payload{scene.color, views[current], views[output], range.offset, range.size};
    if (svc_gfx->push_compute(mod_ctx, compute_type, &payload, sizeof(payload)) != MOD_OK) {
        reset(); return neutral();
    }
    current = output; initialized = true; last_time = now;
    last_layout = layout_key; last_stage = stage; last_room = room;
    return views[current];
}

void shutdown() {
    if (svc_log && encoded.load()) {
        char message[160];
        std::snprintf(message, sizeof(message), "Adaptive exposure shutdown: encoded=%llu samples_per_frame=256",
                      static_cast<unsigned long long>(encoded.load()));
        svc_log->info(mod_ctx, message);
    }
    if (compute_type && svc_gfx) svc_gfx->unregister_compute_type(mod_ctx, compute_type);
    compute_type = 0;
    if (layout) wgpuBindGroupLayoutRelease(std::exchange(layout, nullptr));
    if (pipeline) wgpuComputePipelineRelease(std::exchange(pipeline, nullptr));
    for (auto& view : views) if (view) wgpuTextureViewRelease(std::exchange(view, nullptr));
    for (auto& texture : textures) if (texture) wgpuTextureRelease(std::exchange(texture, nullptr));
    reset();
}
} // namespace midnafx::render::adaptive_exposure
