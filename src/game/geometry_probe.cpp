#include "geometry_probe.hpp"

#include "services.hpp"
#include "topology.hpp"
#include "ui/settings.hpp"

#include <JSystem/J3DGraphAnimator/J3DModelData.h>
#include <JSystem/J3DGraphBase/J3DMaterial.h>
#include <JSystem/J3DGraphBase/J3DShape.h>
#include <JSystem/JKernel/JKRArchive.h>
#include <d/d_resorce.h>
#include <mods/svc/hook.hpp>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <vector>

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
    for (u32 i = 0; i < count * 3; ++i)
        normals[i] = -normals[i];
    svc_log->info(mod_ctx,
                  "Geometry mutation test: L_mbox_00.arc/l_metabox_00.bmd normals negated");
}

bool is_model_node(u32 type) {
    return type == 'BMDR' || type == 'BMDV' || type == 'BMDE' || type == 'BMWR' || type == 'BMWE' ||
           type == 'BMDP' || type == 'BMDG' || type == 'BMDA';
}

bool is_topology_target(const dRes_info_c& info, const char* name) {
    return (std::strcmp(info.mArchiveName, "L_mbox_00") == 0 &&
            std::strcmp(name, "l_metabox_00.bmd") == 0) ||
           (std::strcmp(info.mArchiveName, "R03_00") == 0 && std::strcmp(name, "model.bmd") == 0) ||
           (std::strcmp(info.mArchiveName, "Bmdl") == 0 && std::strcmp(name, "bl.bmd") == 0);
}

void analyze_topology(const dRes_info_c& info, const char* name, J3DModelData& model) {
    if (!settings::topology_diagnostics_enabled() || !is_topology_target(info, name))
        return;
    const auto begin = std::chrono::steady_clock::now();
    const J3DVertexData& vertex = model.getVertexData();
    const u32 pos_count = vertex.getVtxArrNum(GX_VA_POS);
    const u32 normal_count = vertex.getNrmNum();
    const u32 stride = vertex.getVtxArrStride(GX_VA_POS);
    const int pos_type = vertex.getVtxPosType();
    topology::Result result;
    if (!vertex.getVtxPosArray() || pos_count == 0 || pos_count > 65536 ||
        !((pos_type == GX_F32 && stride == 12) || (pos_type == GX_S16 && stride == 6))) {
        result.error = "unsupported position array";
    } else {
        std::vector<topology::Vec3> positions(pos_count);
        const auto* source = static_cast<const std::byte*>(vertex.getVtxPosArray());
        const float scale = std::ldexp(1.0f, -static_cast<int>(vertex.getVtxPosFrac()));
        for (u32 i = 0; i < pos_count; ++i) {
            float values[3];
            for (unsigned c = 0; c < 3; ++c) {
                if (pos_type == GX_F32) {
                    std::memcpy(&values[c], source + i * stride + c * 4, 4);
                } else {
                    std::int16_t value;
                    std::memcpy(&value, source + i * stride + c * 2, 2);
                    values[c] = value * scale;
                }
            }
            positions[i] = {values[0], values[1], values[2]};
        }
        std::vector<topology::Format> formats;
        if (const auto* raw = vertex.getVtxAttrFmtList()) {
            for (unsigned i = 0; i < 32 && raw[i].attr != GX_VA_NULL; ++i)
                formats.push_back({static_cast<std::uint8_t>(raw[i].attr),
                                   static_cast<std::uint8_t>(raw[i].cnt),
                                   static_cast<std::uint8_t>(raw[i].type)});
        }
        const u16 shape_count = model.getShapeNum();
        std::vector<std::vector<topology::Attribute>> attrs(shape_count);
        std::vector<std::vector<topology::Group>> groups(shape_count);
        std::vector<topology::Shape> shapes(shape_count);
        for (u16 si = 0; si < shape_count; ++si) {
            auto* shape = model.getShapeNodePointer(si);
            if (!shape || !shape->getVtxDesc()) {
                result.error = "missing shape descriptor";
                break;
            }
            const auto* raw = shape->getVtxDesc();
            for (unsigned i = 0; i < 32 && raw[i].attr != GX_VA_NULL; ++i)
                attrs[si].push_back({static_cast<std::uint8_t>(raw[i].attr),
                                     static_cast<std::uint8_t>(raw[i].type)});
            if (attrs[si].size() == 32) {
                result.error = "unterminated shape descriptor";
                break;
            }
            for (u16 gi = 0; gi < shape->getMtxGroupNum(); ++gi) {
                auto* draw = shape->getShapeDraw(gi);
                if (!draw) {
                    result.error = "missing shape draw";
                    break;
                }
                groups[si].push_back({draw->getDisplayList(), draw->getDisplayListSize()});
            }
            if (result.error)
                break;
            shapes[si] = {attrs[si], groups[si],
                          shape->getMaterial() ? shape->getMaterial()->getIndex()
                                               : static_cast<u16>(0xffff)};
        }
        if (!result.error)
            result = topology::decode(shapes, formats, positions, normal_count);
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                             std::chrono::steady_clock::now() - begin)
                             .count();
    char report[650];
    std::snprintf(report, sizeof(report),
                  "Topology JSON: {\"archive\":\"%s\",\"file\":\"%s\",\"positions\":%u,"
                  "\"normals\":%u,\"shapes\":%u,\"envelopes\":%u,\"primitives\":%u,"
                  "\"strips\":%u,\"fans\":%u,\"indexed\":%u,\"triangles\":%zu,"
                  "\"degenerate\":%u,\"uniquePositions\":%u,\"uniqueNormals\":%u,"
                  "\"positionNormalSplits\":%u,\"cornerHash\":\"%016llx\","
                  "\"decodeUs\":%lld,\"error\":\"%s\"}",
                  info.mArchiveName, name, pos_count, normal_count, model.getShapeNum(),
                  model.getWEvlpMtxNum(), result.primitive_count, result.strip_count,
                  result.fan_count, result.indexed_count, result.triangles.size(),
                  result.degenerate_count, result.unique_positions, result.unique_normals,
                  result.position_normal_splits,
                  static_cast<unsigned long long>(result.corner_hash),
                  static_cast<long long>(elapsed), result.error ? result.error : "");
    svc_log->info(mod_ctx, report);
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
    analyze_topology(info, file_name, *model);
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
    if ((!settings::geometry_diagnostics_enabled() && !settings::geometry_mutation_test_enabled() &&
         !settings::topology_diagnostics_enabled()) ||
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
