#pragma once
#include <mods/svc/gfx.h>
namespace midnafx::render::ambient_occlusion {
void initialize();
// Idempotent between SCENE_BEGIN callbacks; water capture calls this first.
void apply(const GfxStageContext* context);
void shutdown();
}
