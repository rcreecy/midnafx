#pragma once
#include <mods/svc/gfx.h>

namespace midnafx::render::bloom {
struct Frame { WGPUTextureView texture = nullptr; void* owner = nullptr; };
bool initialize(const GfxDeviceInfo& device);
void shutdown();
// A prepared frame owns one queued-draw reference; release after encoding or
// failed draw submission. Compute and draw share the existing scene snapshot.
Frame prepare(const GfxResolvedTargets& scene, float strength);
void release(Frame frame);
WGPUTextureView view(Frame frame);
WGPUSampler sampler();
} // namespace midnafx::render::bloom
