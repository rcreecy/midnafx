#include "geometry_probe.hpp"

#include "services.hpp"
#include "ui/settings.hpp"

#include <JSystem/J3DGraphAnimator/J3DModelData.h>
#include <JSystem/JKernel/JKRArchive.h>
#include <d/d_resorce.h>
#include <mods/svc/hook.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>

namespace midnafx::geometry_probe {
namespace {
DEFINE_HOOK(&dRes_info_c::loadResource, ResourceLoad);
DEFINE_HOOK(&dRes_info_c::deleteArchiveRes, ResourceDelete);
bool load_registered = false;
bool delete_registered = false;

struct MutationBackup {
    dRes_info_c* owner = nullptr;
    void* normals = nullptr;
    u32 bytes = 0;
    std::unique_ptr<std::byte[]> original;
};
MutationBackup mutation;

void restore_active_mutation() {
    if (!mutation.owner)
        return;
    std::memcpy(mutation.normals, mutation.original.get(), mutation.bytes);
    mutation = {};
    svc_log->info(mod_ctx, "Geometry mutation test: original normals restored");
}

void maybe_mutate(dRes_info_c& info, const char* file_name, J3DModelData& model) {
    // One exact game-owned BMD, verified offline from the USA GZ2E01 disc.
    if (!delete_registered || !settings::geometry_mutation_test_enabled() || mutation.owner ||
        std::strcmp(info.mArchiveName, "L_mbox_00") != 0 ||
        std::strcmp(file_name, "l_metabox_00.bmd") != 0)
        return;
    J3DVertexData& vertex = model.getVertexData();
    const auto* formats = vertex.getVtxAttrFmtList();
    bool xyz_f32 = false;
    if (formats) {
        for (unsigned i = 0; i < 32 && formats[i].attr != GX_VA_NULL; ++i) {
            if (formats[i].attr == GX_VA_NRM) {
                xyz_f32 = formats[i].cnt == GX_NRM_XYZ && formats[i].type == GX_F32;
                break;
            }
        }
    }
    const u32 count = vertex.getNrmNum();
    if (!xyz_f32 || vertex.getVtxNBTArray() || model.getWEvlpMtxNum() != 0 ||
        vertex.getVtxArrStride(GX_VA_NRM) != 12 || count == 0 || count > 100000)
        return;
    const u32 bytes = count * 12;
    const auto base = reinterpret_cast<std::uintptr_t>(model.getRawData());
    const auto address = reinterpret_cast<std::uintptr_t>(vertex.getVtxNrmArray());
    const u32 resource_size = info.mArchive->getExpandedResSize(model.getRawData());
    if (resource_size == static_cast<u32>(-1) || address < base || address % alignof(float) != 0 ||
        address - base > resource_size || bytes > resource_size - (address - base))
        return;
    auto* normals = static_cast<float*>(vertex.getVtxNrmArray());
    for (u32 i = 0; i < count * 3; ++i)
        if (!std::isfinite(normals[i]))
            return;
    std::unique_ptr<std::byte[]> original{new (std::nothrow) std::byte[bytes]};
    if (!original)
        return;
    std::memcpy(original.get(), normals, bytes);
    mutation.owner = &info;
    mutation.normals = normals;
    mutation.bytes = bytes;
    mutation.original = std::move(original);
    for (u32 i = 0; i < count; ++i) {
        normals[3 * i] = 0.0f;
        normals[3 * i + 1] = 1.0f;
        normals[3 * i + 2] = 0.0f;
    }
    svc_log->info(mod_ctx,
                  "Geometry mutation test: L_mbox_00.arc/l_metabox_00.bmd normals forced upward");
}

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
    maybe_mutate(const_cast<dRes_info_c&>(info), file_name, *model);
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
    if (settings::geometry_diagnostics_enabled())
        svc_log->info(mod_ctx, message);
}

void on_resource_loaded(ModContext*, void* args, void* retval, void*) {
    if ((!settings::geometry_diagnostics_enabled() &&
         !settings::geometry_mutation_test_enabled()) ||
        !args || !retval || *static_cast<int*>(retval) < 0)
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

HookAction on_resource_delete(ModContext*, void* args, void*, void*) {
    if (args && mods::arg<dRes_info_c*>(args, 0) == mutation.owner)
        restore_active_mutation();
    return HOOK_CONTINUE;
}
} // namespace

void initialize() {
    load_registered = false;
    delete_registered = false;
    mutation = {};
    if (!svc_hook)
        return;
    if (mods::hook::add_pre<ResourceDelete>(on_resource_delete) == MOD_OK)
        delete_registered = true;
    else
        (void)mods::hook::uninstall<ResourceDelete>();
    if (mods::hook::add_post<ResourceLoad>(on_resource_loaded) == MOD_OK)
        load_registered = true;
    else
        (void)mods::hook::uninstall<ResourceLoad>();
}

void shutdown() {
    if (load_registered)
        (void)mods::hook::uninstall<ResourceLoad>();
    restore_active_mutation();
    if (delete_registered)
        (void)mods::hook::uninstall<ResourceDelete>();
    load_registered = delete_registered = false;
}

void restore_mutation() { restore_active_mutation(); }
} // namespace midnafx::geometry_probe
