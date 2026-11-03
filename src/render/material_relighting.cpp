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
    return svc_native_material &&
           svc_native_material->header.struct_size >= sizeof(NativeMaterialService) &&
           svc_native_material->set_relighting;
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
