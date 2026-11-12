#pragma once
#include <webgpu/webgpu.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <vector>
#include <algorithm>

namespace midnafx::render::immutable_instances {
struct Resource {
    WGPUBuffer buffer = nullptr;
    std::size_t bytes = 0;
    std::atomic<unsigned> pending{0};
    ~Resource() { if (buffer) wgpuBufferRelease(buffer); }
};
inline void pin(Resource* value) { if (value) value->pending.fetch_add(1); }
inline void release(Resource* value) { if (value) value->pending.fetch_sub(1); }
struct Pending {
    Resource* value;
    ~Pending() { release(value); }
};
// Producer owns this pool; render callbacks only read pinned resources and
// decrement pending. Encoded WebGPU commands retain their own buffer references.
class Pool {
    std::vector<std::unique_ptr<Resource>> resources;
    Resource* current = nullptr;
    unsigned stable = 0;
    bool failed = false;
    WGPUBufferUsage extra_usage;
public:
    static constexpr unsigned WarmFrames = 8, MaxSets = 4;
    static constexpr std::size_t MaxBytes = 8 * 1024 * 1024;
    explicit Pool(WGPUBufferUsage extra = WGPUBufferUsage_None) : extra_usage(extra) {}
    void collect() {
        std::erase_if(resources, [&](const auto& resource) {
            return resource.get() != current && resource->pending.load() == 0;
        });
    }
    void invalidate() { current = nullptr; stable = 0; failed = false; collect(); }
    Resource* prepare(WGPUDevice device, std::span<const std::byte> bytes, bool unchanged) {
        if (!unchanged || bytes.empty()) { invalidate(); return nullptr; }
        if (current && current->bytes != bytes.size()) invalidate();
        collect();
        if (current) return current;
        if (stable < WarmFrames) ++stable;
        if (stable < WarmFrames || failed || !device || resources.size() >= MaxSets ||
            bytes.size() > MaxBytes || bytes.size() % 4) return nullptr;
        auto next = std::make_unique<Resource>();
        WGPUBufferDescriptor desc = WGPU_BUFFER_DESCRIPTOR_INIT;
        desc.label = {"MidnaFX unchanged ground instances", WGPU_STRLEN};
        desc.size = bytes.size(); desc.usage = WGPUBufferUsage_Vertex | extra_usage;
        desc.mappedAtCreation = true;
        next->buffer = wgpuDeviceCreateBuffer(device, &desc);
        void* mapped = next->buffer ? wgpuBufferGetMappedRange(next->buffer, 0, bytes.size()) : nullptr;
        if (!mapped) { failed = true; return nullptr; }
        std::memcpy(mapped, bytes.data(), bytes.size());
        wgpuBufferUnmap(next->buffer);
        next->bytes = bytes.size();
        current = next.get(); resources.push_back(std::move(next));
        return current;
    }
    std::size_t bytes() const {
        std::size_t result = 0;
        for (const auto& resource : resources) result += resource->bytes;
        return result;
    }
    std::size_t sets() const { return resources.size(); }
    // Only after the host has drained/cancelled all callbacks at mod shutdown.
    // Release references, never Destroy buffers still held by encoded commands.
    void clear_after_drain() { current = nullptr; resources.clear(); stable = 0; failed = false; }
};
} // namespace midnafx::render::immutable_instances
