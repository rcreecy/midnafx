#include "material_relighting.hpp"
#include "native_material_service.hpp"
#include "services.hpp"
#include "ui/settings.hpp"

namespace midnafx::render::material_relighting {
namespace {
bool requested = false;
bool reported_conflict = false;
}

bool available() noexcept {
    return SERVICE_HAS(svc_native_material, NativeMaterialService, set_relighting) &&
           svc_native_material->set_relighting;
}

bool surface_relief_available() noexcept {
    return available() && svc_native_material->header.minor_version >= 1 &&
           SERVICE_HAS(svc_native_material, NativeMaterialService, set_surface_relief) &&
           svc_native_material->set_surface_relief;
}
bool surface_profiles_available() noexcept {
    return available() && svc_native_material->header.minor_version >= 2 &&
           SERVICE_HAS(svc_native_material, NativeMaterialService, set_surface_profile) &&
           svc_native_material->set_surface_profile;
}
bool set_surface_profile(unsigned profile) noexcept {
    return surface_profiles_available() && svc_native_material->set_surface_profile(mod_ctx, profile) == MOD_OK;
}

void update() {
    if (!available()) return;
    const bool enable = settings::material_relighting_enabled();
    if (enable == requested) return;
    const auto result = svc_native_material->set_relighting(mod_ctx, enable ? 1u : 0u);
    if (result != MOD_OK) {
        if (!reported_conflict) {
            reported_conflict = true;
            svc_log->warn(mod_ctx, "Material relighting: host rejected scene request");
        }
        return;
    }
    if (enable && surface_relief_available()) {
        const auto relief = svc_native_material->set_surface_relief(mod_ctx, 1u);
        svc_log->info(mod_ctx, relief == MOD_OK ? "Material relief: opaque scenery texture response enabled"
                                             : "Material relief: unavailable; per-fragment lighting retained");
    }
    requested = enable;
    reported_conflict = false;
    svc_log->info(mod_ctx, enable ? "Material relighting: native scene request enabled"
                                  : "Material relighting: native scene request disabled");
}

void shutdown() {
    if (requested && available()) svc_native_material->set_relighting(mod_ctx, 0u);
    requested = reported_conflict = false;
}
}
