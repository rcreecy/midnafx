#pragma once

#include <mods/service.hpp>

// Optional host capability. Requests run on the game thread. The host records
// copied values at native scene boundaries, without retaining mod data pointers.
#define NATIVE_MATERIAL_SERVICE_ID DUSKLIGHT_SERVICE_ID_PREFIX "native-material"
#define NATIVE_MATERIAL_SERVICE_MAJOR 1u
#define NATIVE_MATERIAL_SERVICE_MINOR 2u

struct NativeMaterialService {
    ServiceHeader header;
    // Values are 0 (native) or 1 (per-fragment RGB). Only one mod may own
    // the active scope. Disable or detach releases ownership.
    ModResult (*set_relighting)(ModContext* context, uint32_t enabled);
    // Since 1.1: bounded texture-derived relief during opaque scenery only.
    // Requires ownership through set_relighting(1); disable/detach clears it.
    ModResult (*set_surface_relief)(ModContext* context, uint32_t enabled);
    // Since 1.2: copied per-draw finish. 0 fallback, 1 rock, 2 wood,
    // 3 cloth, 4 metal, 5 wet surface, 6 leather. Restore 0 after the draw.
    ModResult (*set_surface_profile)(ModContext* context, uint32_t profile);
};

MOD_DECLARE_SERVICE(NativeMaterialService, svc_native_material, NATIVE_MATERIAL_SERVICE_ID,
                    NATIVE_MATERIAL_SERVICE_MAJOR, NATIVE_MATERIAL_SERVICE_MINOR);
