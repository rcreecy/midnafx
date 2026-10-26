#pragma once
#include <cstdint>
#include <mods/svc/gfx.h>

namespace midnafx::render::adaptive_exposure {
bool initialize(const GfxDeviceInfo& device);
void shutdown();
void reset();
// Uses the renderer's existing color snapshot; queues compute before its draw.
WGPUTextureView prepare(const GfxResolvedTargets& scene, std::uint64_t layout_key, bool active);
WGPUTextureView neutral();
} // namespace midnafx::render::adaptive_exposure
