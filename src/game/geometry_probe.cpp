#include "geometry_probe.hpp"

#include "services.hpp"
#include "ui/settings.hpp"

#include <JSystem/J3DGraphAnimator/J3DModelData.h>
#include <JSystem/JKernel/JKRArchive.h>
#include <d/d_resorce.h>
#include <mods/svc/hook.hpp>

#include <cstdio>

namespace midnafx::geometry_probe {
namespace {
DEFINE_HOOK(&dRes_info_c::loadResource, ResourceLoad);
bool registered = false;

bool is_model_node(u32 type) {
    return type == 'BMDR' || type == 'BMDV' || type == 'BMDE' || type == 'BMWR' || type == 'BMWE' ||
           type == 'BMDP' || type == 'BMDG' || type == 'BMDA';
}

void describe_model(const dRes_info_c& info, u32 type, u32 file_index) {
    JKRArchive* archive = info.mArchive;
    if (!archive || !info.mRes || file_index >= static_cast<u32>(archive->countFile()))
        return;
    auto* entry = archive->findIdxResource(file_index);
    auto* model = static_cast<J3DModelData*>(info.mRes[file_index]);
    if (!entry || !model)
        return;

    const char* file_name = archive->mStringTable + (entry->type_flags_and_name_offset & 0xFFFFFF);
    const J3DVertexData& vertex = model->getVertexData();
    int normal_type = -1;
    int normal_count = -1;
    if (auto* formats = vertex.getVtxAttrFmtList()) {
        for (unsigned i = 0; i < 32 && formats[i].attr != GX_VA_NULL; ++i) {
            if (formats[i].attr == GX_VA_NRM) {
                normal_type = static_cast<int>(formats[i].type);
                normal_count = static_cast<int>(formats[i].cnt);
                break;
            }
        }
    }
    char message[320];
    std::snprintf(message, sizeof(message),
                  "Geometry catalog: %.10s.arc/%s tag=%c%c%c%c pos=%u nrm=%u "
                  "nrmType=%d nrmComponents=%d nrmStride=%u NBT=%s envelopes=%u "
                  "shapes=%u materials=%u",
                  info.mArchiveName, file_name, static_cast<char>(type >> 24),
                  static_cast<char>(type >> 16), static_cast<char>(type >> 8),
                  static_cast<char>(type), model->getVtxNum(), model->getNrmNum(), normal_type,
                  normal_count, vertex.getVtxArrStride(GX_VA_NRM),
                  vertex.getVtxNBTArray() ? "yes" : "no", model->getWEvlpMtxNum(),
                  model->getShapeNum(), model->getMaterialNum());
    svc_log->info(mod_ctx, message);
}

void on_resource_loaded(ModContext*, void* args, void* retval, void*) {
    if (!settings::geometry_diagnostics_enabled() || !args || !retval ||
        *static_cast<int*>(retval) < 0)
        return;
    auto* info = mods::arg<dRes_info_c*>(args, 0);
    if (!info || !info->mArchive || !info->mRes)
        return;
    auto* archive = info->mArchive;
    auto* node = archive->mNodes;
    const u32 count = static_cast<u32>(archive->countFile());
    for (int i = 0; i < archive->countDirectory(); ++i, ++node) {
        if (!is_model_node(node->type))
            continue;
        const u32 first = node->first_file_index;
        for (u32 j = 0; j < node->num_entries && first + j < count; ++j) {
            const u32 file_index = first + j;
            if (archive->isFileEntry(file_index))
                describe_model(*info, node->type, file_index);
        }
    }
}
} // namespace

void initialize() {
    registered = false;
    if (!svc_hook)
        return;
    if (mods::hook::add_post<ResourceLoad>(on_resource_loaded) == MOD_OK)
        registered = true;
    else
        (void)mods::hook::uninstall<ResourceLoad>();
}

void shutdown() {
    if (registered)
        (void)mods::hook::uninstall<ResourceLoad>();
    registered = false;
}
} // namespace midnafx::geometry_probe
