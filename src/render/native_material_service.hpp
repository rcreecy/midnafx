#pragma once

#include <mods/service.hpp>

// Optional host capability. Requests run on the game thread. The host records
// copied values at native scene boundaries, without retaining mod data pointers.
#define NATIVE_MATERIAL_SERVICE_ID DUSKLIGHT_SERVICE_ID_PREFIX "native-material"
#define NATIVE_MATERIAL_SERVICE_MAJOR 1u
#define NATIVE_MATERIAL_SERVICE_MINOR 0u

struct NativeMaterialService {
    ServiceHeader header;
    // Values are 0 (native) or 1 (per-fragment RGB). Only one mod may own
    // the active scope. Disable or detach releases ownership.
    ModResult (*set_relighting)(ModContext* context, uint32_t enabled);
};

MOD_DECLARE_SERVICE(NativeMaterialService, svc_native_material, NATIVE_MATERIAL_SERVICE_ID,
                    NATIVE_MATERIAL_SERVICE_MAJOR, NATIVE_MATERIAL_SERVICE_MINOR);
