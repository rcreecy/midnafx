#include "bloom.hpp"
#include "gpu_validation.hpp"
#include "midnafx_shader.hpp"
#include "services.hpp"
#include <d/d_com_inf_game.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace midnafx::render::bloom {
namespace {
struct Controls { float strength, blur_scale; float padding[2]{}; };
static_assert(sizeof(Controls) == 16);
struct Targets {
    std::uint32_t width = 0, height = 0;
    WGPUTexture textures[2]{};
    WGPUTextureView views[2]{};
    std::atomic<unsigned> pending{0};
    ~Targets() {
        for (auto value : views) if (value) wgpuTextureViewRelease(value);
        for (auto value : textures) if (value) wgpuTextureRelease(value);
    }
};
struct Payload {
    WGPUTextureView scene;
    Targets* targets;
    std::uint32_t offset, size;
};
static_assert(sizeof(Payload) <= GFX_INLINE_DRAW_PAYLOAD_SIZE);
struct Pending {
    Targets* targets;
    ~Pending() { if (targets) targets->pending.fetch_sub(1); }
};
GfxDeviceInfo device = GFX_DEVICE_INFO_INIT;
GfxComputeTypeHandle compute_type = 0;
WGPUComputePipeline pipelines[3]{};
WGPUBindGroupLayout layouts[3]{};
WGPUTexture neutral_texture = nullptr;
WGPUTextureView neutral_view = nullptr;
WGPUSampler linear_sampler = nullptr;
std::vector<std::unique_ptr<Targets>> target_sets;
Targets* current = nullptr;
std::atomic<bool> failed{false};
std::atomic<std::uint64_t> encoded{0};
bool warned = false;

void collect() {
    std::erase_if(target_sets, [](const auto& target) {
        return target.get() != current && target->pending.load() == 0;
    });
}
Targets* ensure_targets(std::uint32_t width, std::uint32_t height) {
    width = (width + 3) / 4; height = (height + 3) / 4;
    if (current && current->width == width && current->height == height) return current;
    current = nullptr;
    collect();
    auto next = std::make_unique<Targets>();
    next->width = width; next->height = height;
    gpu::ValidationScopes scopes(device.device, device.instance);
    for (unsigned i = 0; i < 2; ++i) {
        WGPUTextureDescriptor desc = WGPU_TEXTURE_DESCRIPTOR_INIT;
        desc.label = {"MidnaFX quarter-resolution bloom", WGPU_STRLEN};
        desc.size = {width, height, 1}; desc.format = WGPUTextureFormat_RGBA16Float;
        desc.usage = WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding;
        next->textures[i] = wgpuDeviceCreateTexture(device.device, &desc);
        if (next->textures[i]) next->views[i] = wgpuTextureCreateView(next->textures[i], nullptr);
    }
    const bool valid = scopes.finish();
    if (!valid || !next->views[0] || !next->views[1]) { failed.store(true); return nullptr; }
    current = next.get();
    target_sets.push_back(std::move(next));
    return current;
}

void compute(ModContext*, const GfxComputeContext* context, const void* bytes, size_t size, void*) {
    if (!context || size != sizeof(Payload)) return;
    Payload payload{};
    std::memcpy(&payload, bytes, sizeof(payload));
    Pending pending{payload.targets};
    if (!payload.targets || !payload.scene || failed.load()) return;
    auto& target = *payload.targets;
    WGPUBindGroup groups[3]{};
    for (unsigned i = 0; i < 3; ++i) {
        WGPUBindGroupEntry entries[4];
        for (auto& entry : entries) entry = WGPU_BIND_GROUP_ENTRY_INIT;
        unsigned count = 0;
        entries[count].binding = i == 0 ? 0 : 1;
        entries[count++].textureView = i == 0 ? payload.scene : target.views[i == 1 ? 0 : 1];
        entries[count].binding = 2; entries[count++].textureView = target.views[i == 1 ? 1 : 0];
        if (i > 0) { entries[count].binding = 3; entries[count++].sampler = linear_sampler; }
        entries[count].binding = 4; entries[count].buffer = context->uniform_buffer;
        entries[count].offset = payload.offset; entries[count++].size = payload.size;
        WGPUBindGroupDescriptor desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
        desc.layout = layouts[i]; desc.entryCount = count; desc.entries = entries;
        groups[i] = wgpuDeviceCreateBindGroup(context->device, &desc);
    }
    if (!groups[0] || !groups[1] || !groups[2]) {
        for (auto group : groups) if (group) wgpuBindGroupRelease(group);
        failed.store(true); return;
    }
    WGPUComputePassDescriptor desc = WGPU_COMPUTE_PASS_DESCRIPTOR_INIT;
    desc.label = {"MidnaFX scene bloom", WGPU_STRLEN};
    auto pass = wgpuCommandEncoderBeginComputePass(context->encoder, &desc);
    for (unsigned i = 0; i < 3; ++i) {
        wgpuComputePassEncoderSetPipeline(pass, pipelines[i]);
        wgpuComputePassEncoderSetBindGroup(pass, 0, groups[i], 0, nullptr);
        wgpuComputePassEncoderDispatchWorkgroups(pass, (target.width + 7) / 8, (target.height + 7) / 8, 1);
    }
    wgpuComputePassEncoderEnd(pass);
    wgpuComputePassEncoderRelease(pass);
    for (auto group : groups) wgpuBindGroupRelease(group);
    encoded.fetch_add(1);
}
} // namespace

bool initialize(const GfxDeviceInfo& info) {
    if (neutral_view) return true;
    device = info; failed.store(false); encoded.store(0); warned = false;
    gpu::ValidationScopes neutral_scopes(device.device, device.instance);
    WGPUTextureDescriptor desc = WGPU_TEXTURE_DESCRIPTOR_INIT;
    desc.size = {1,1,1}; desc.format = WGPUTextureFormat_RGBA16Float;
    desc.usage = WGPUTextureUsage_TextureBinding;
    neutral_texture = wgpuDeviceCreateTexture(device.device, &desc);
    if (neutral_texture) neutral_view = wgpuTextureCreateView(neutral_texture, nullptr);
    WGPUSamplerDescriptor sampler_desc = WGPU_SAMPLER_DESCRIPTOR_INIT;
    sampler_desc.minFilter = WGPUFilterMode_Linear; sampler_desc.magFilter = WGPUFilterMode_Linear;
    sampler_desc.addressModeU = WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeV = WGPUAddressMode_ClampToEdge;
    linear_sampler = wgpuDeviceCreateSampler(device.device, &sampler_desc);
    const bool neutral_valid = neutral_scopes.finish();
    if (!neutral_valid || !neutral_view || !linear_sampler) { shutdown(); return false; }
    gpu::ValidationScopes compute_scopes(device.device, device.instance);
    WGPUShaderSourceWGSL source = WGPU_SHADER_SOURCE_WGSL_INIT;
    source.code = {bloom_compute_shader, WGPU_STRLEN};
    WGPUShaderModuleDescriptor shader_desc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    shader_desc.nextInChain = &source.chain;
    auto shader = wgpuDeviceCreateShaderModule(device.device, &shader_desc);
    constexpr const char* entries[]{"cs_extract", "cs_blur_horizontal", "cs_blur_vertical"};
    if (shader) for (unsigned i = 0; i < 3; ++i) {
        WGPUComputePipelineDescriptor pipeline_desc = WGPU_COMPUTE_PIPELINE_DESCRIPTOR_INIT;
        pipeline_desc.compute.module = shader; pipeline_desc.compute.entryPoint = {entries[i], WGPU_STRLEN};
        pipelines[i] = wgpuDeviceCreateComputePipeline(device.device, &pipeline_desc);
        if (pipelines[i]) layouts[i] = wgpuComputePipelineGetBindGroupLayout(pipelines[i], 0);
    }
    if (shader) wgpuShaderModuleRelease(shader);
    const bool compute_valid = compute_scopes.finish();
    GfxComputeTypeDesc compute_desc = GFX_COMPUTE_TYPE_DESC_INIT;
    compute_desc.label = "MidnaFX scene bloom"; compute_desc.callback = compute;
    if (!compute_valid || !layouts[0] || !layouts[1] || !layouts[2] ||
        svc_gfx->register_compute_type(mod_ctx, &compute_desc, &compute_type) != MOD_OK)
        failed.store(true);
    return true;
}

Frame prepare(const GfxResolvedTargets& scene, float strength) {
    collect();
    if (strength <= 0 || !scene.color || !scene.width || !scene.height || failed.load() || !compute_type ||
        !dComIfGp_getPlayer(0) || dComIfGp_isPauseFlag()) {
        if (strength > 0 && failed.load() && !warned && svc_log) {
            svc_log->warn(mod_ctx, "Scene bloom unavailable; original scene lighting retained");
            warned = true;
        }
        return {neutral_view, nullptr};
    }
    auto* target = ensure_targets(scene.width, scene.height);
    if (!target) return {neutral_view, nullptr};
    const Controls controls{std::clamp(strength, 0.0f, 0.5f), std::max(float(scene.height) / 1080.0f, 0.25f)};
    GfxRange range{};
    if (svc_gfx->push_uniform(mod_ctx, &controls, sizeof(controls), &range) != MOD_OK)
        return {neutral_view, nullptr};
    const Payload payload{scene.color, target, range.offset, range.size};
    target->pending.fetch_add(1);
    if (svc_gfx->push_compute(mod_ctx, compute_type, &payload, sizeof(payload)) != MOD_OK) {
        target->pending.fetch_sub(1); return {neutral_view, nullptr};
    }
    target->pending.fetch_add(1);
    return {target->views[0], target};
}
void release(Frame frame) {
    if (frame.owner) static_cast<Targets*>(frame.owner)->pending.fetch_sub(1);
}
WGPUTextureView view(Frame frame) { return failed.load() ? neutral_view : frame.texture; }
WGPUSampler sampler() { return linear_sampler; }

void shutdown() {
    if (svc_log && encoded.load()) {
        char message[160];
        std::snprintf(message, sizeof(message), "Scene bloom shutdown: compute=%llu target_sets=%zu",
                      static_cast<unsigned long long>(encoded.load()), target_sets.size());
        svc_log->info(mod_ctx, message);
    }
    if (svc_gfx && compute_type) svc_gfx->unregister_compute_type(mod_ctx, compute_type);
    compute_type = 0;
    target_sets.clear(); current = nullptr;
    for (auto& value : layouts) if (value) { wgpuBindGroupLayoutRelease(value); value = nullptr; }
    for (auto& value : pipelines) if (value) { wgpuComputePipelineRelease(value); value = nullptr; }
    if (linear_sampler) { wgpuSamplerRelease(linear_sampler); linear_sampler = nullptr; }
    if (neutral_view) { wgpuTextureViewRelease(neutral_view); neutral_view = nullptr; }
    if (neutral_texture) { wgpuTextureRelease(neutral_texture); neutral_texture = nullptr; }
}
} // namespace midnafx::render::bloom
