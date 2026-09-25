#include "geometry_probe.hpp"

#include "bmd_rebuild.hpp"
#include "services.hpp"
#include "smoothing.hpp"
#include "topology.hpp"
#include "ui/settings.hpp"

#include <JSystem/J3DGraphAnimator/J3DModelData.h>
#include <JSystem/J3DGraphBase/J3DMaterial.h>
#include <JSystem/J3DGraphBase/J3DShape.h>
#include <JSystem/J3DGraphLoader/J3DModelLoader.h>
#include <JSystem/JKernel/JKRArchive.h>
#include <JSystem/JKernel/JKRHeap.h>
#include <JSystem/JKernel/JKRSolidHeap.h>
#include <d/d_resorce.h>
#include <m_Do/m_Do_ext.h>
#include <mods/svc/hook.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <vector>

namespace midnafx::geometry_probe {
namespace {
DEFINE_HOOK(&dRes_info_c::loadResource, ResourceLoad);
DEFINE_HOOK(&dRes_info_c::deleteArchiveRes, ResourceDelete);
DEFINE_HOOK(&mDoExt_J3DModel__create, ModelCreate);
DEFINE_HOOK(&J3DModelLoaderDataBase::load, ModelLoad);
bool load_registered = false;
bool delete_registered = false;
bool create_registered = false;
bool model_load_registered = false;

struct Replacement {
    enum class Kind { Pot, LinkBody, Nest, Pumpkin } kind = Kind::Pot;
    dRes_info_c* owner = nullptr;
    const void* source = nullptr;
    void* data = nullptr;
    u32 size = 0;
};
std::vector<Replacement> replacements;
thread_local std::vector<dRes_info_c*> loading_owners;

std::uint64_t fnv1a(const std::byte* data, std::size_t size) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= std::to_integer<std::uint8_t>(data[i]);
        hash *= 1099511628211ULL;
    }
    return hash;
}

const Replacement* replacement_for(const void* data) {
    const auto found = std::find_if(replacements.begin(), replacements.end(),
                                    [data](const Replacement& item) {
                                        return item.data == data;
                                    });
    return found == replacements.end() ? nullptr : &*found;
}

HookAction use_rebuilt_model(ModContext*, void* args, void*, void*) {
    if (!args)
        return HOOK_CONTINUE;
    const void* source = mods::arg<const void*>(args, 0);
    auto found = std::find_if(replacements.begin(), replacements.end(),
                              [source](const Replacement& item) {
                                  return item.source == source;
                              });
    if (found == replacements.end() && delete_registered &&
        settings::geometry_smoothing_enabled() && !loading_owners.empty() &&
        source && std::memcmp(source, "J3D2bmd3", 8) == 0) {
        auto* owner = loading_owners.back();
        const auto* bytes = static_cast<const std::byte*>(source);
        const u32 declared_size =
            (static_cast<u32>(std::to_integer<u8>(bytes[8])) << 24) |
            (static_cast<u32>(std::to_integer<u8>(bytes[9])) << 16) |
            (static_cast<u32>(std::to_integer<u8>(bytes[10])) << 8) |
            static_cast<u32>(std::to_integer<u8>(bytes[11]));
        JKRHeap* allocation_heap = JKRHeap::getCurrentHeap();
        if (owner && allocation_heap && owner->mArchiveName &&
            std::strcmp(owner->mArchiveName, "pumpkin") == 0 &&
            declared_size == 17824 && fnv1a(bytes, declared_size) == 0x9b2ddb5ecbd95421ULL) {
            const auto begin = std::chrono::steady_clock::now();
            auto rebuilt = bmd_rebuild::split_normals({bytes, declared_size});
            if (rebuilt.ok() && rebuilt.bytes.size() <= std::numeric_limits<u32>::max()) {
                // Packed child archives have no mDataHeap. They load synchronously
                // into the parent's current solid heap, alongside their J3D data.
                void* owned = JKRHeap::alloc(static_cast<u32>(rebuilt.bytes.size()), 0x20,
                                             allocation_heap);
                if (owned) {
                    std::memcpy(owned, rebuilt.bytes.data(), rebuilt.bytes.size());
                    replacements.push_back({Replacement::Kind::Pumpkin, owner, source, owned,
                                            static_cast<u32>(rebuilt.bytes.size())});
                    found = std::prev(replacements.end());
                    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                                             std::chrono::steady_clock::now() - begin)
                                             .count();
                    char message[240];
                    std::snprintf(message, sizeof(message),
                                  "Geometry rebuild: pumpkin.arc/pumpkin.bmd bytes=%u "
                                  "normals=%u/%u triangles=%u us=%lld",
                                  static_cast<unsigned>(rebuilt.bytes.size()),
                                  rebuilt.evidence.written_normals,
                                  rebuilt.evidence.rebuilt_normal_capacity,
                                  rebuilt.evidence.triangles,
                                  static_cast<long long>(elapsed));
                    svc_log->info(mod_ctx, message);
                } else
                    svc_log->info(mod_ctx, "Geometry rebuild: pumpkin allocation failed");
            } else {
                char message[240];
                std::snprintf(message, sizeof(message),
                              "Geometry rebuild: pumpkin lazy rebuild rejected: %.170s",
                              rebuilt.error.c_str());
                svc_log->info(mod_ctx, message);
            }
        }
    }
    if (found != replacements.end())
        mods::arg_ref<const void*>(args, 0) = found->data;
    return HOOK_CONTINUE;
}

HookAction prepare_rebuilt_model(ModContext*, void* args, void*, void*) {
    if (!args || !model_load_registered || !delete_registered)
        return HOOK_CONTINUE;
    auto* info = mods::arg<dRes_info_c*>(args, 0);
    if (info)
        loading_owners.push_back(info);
    if (!info || !info->mArchive || !info->mDataHeap || !info->mArchiveName)
        return HOOK_CONTINUE;
    struct Target {
        Replacement::Kind kind;
        const char* file;
        u32 size;
        std::uint64_t hash;
        const char* label;
    };
    std::optional<Target> target;
    if (settings::geometry_skinned_smoothing_enabled() &&
        std::strcmp(info->mArchiveName, "Kmdl") == 0)
        target = Target{Replacement::Kind::LinkBody, "al.bmd", 140448,
                        0xc00c6d9abd5d79bcULL,
                        "Kmdl.arc/al.bmd"};
    else if (settings::geometry_smoothing_enabled() &&
             std::strcmp(info->mArchiveName, "OBJ_GM") == 0)
        target = Target{Replacement::Kind::Pot, "k_kumo_tubo01.bmd", 12896,
                        0xa9efd2ace652b900ULL, "OBJ_GM.arc/k_kumo_tubo01.bmd"};
    else if (settings::geometry_smoothing_enabled() &&
             std::strcmp(info->mArchiveName, "E_nest") == 0)
        target = Target{Replacement::Kind::Nest, "o_hachinosu_01.bmd", 12576,
                        0xde8546d0a1fd387dULL, "E_nest.arc/o_hachinosu_01.bmd"};
    if (!target)
        return HOOK_CONTINUE;
    if (JKRHeap::getCurrentHeap() != info->mDataHeap)
        return HOOK_CONTINUE;
    if (std::any_of(replacements.begin(), replacements.end(),
                    [info, &target](const Replacement& item) {
                        return item.owner == info && item.kind == target->kind;
                    }))
        return HOOK_CONTINUE;
    const u32 count = static_cast<u32>(info->mArchive->countFile());
    for (u32 index = 0; index < count; ++index) {
        if (!info->mArchive->isFileEntry(index))
            continue;
        auto* entry = info->mArchive->findIdxResource(index);
        if (!entry)
            continue;
        const char* name = info->mArchive->mStringTable +
                           (entry->type_flags_and_name_offset & 0xFFFFFF);
        if (std::strcmp(name, target->file) != 0)
            continue;
        const void* source = info->mArchive->getIdxResource(index);
        if (!source) {
            svc_log->info(mod_ctx, "Geometry rebuild: source unavailable");
            return HOOK_CONTINUE;
        }
        const u32 size = info->mArchive->getExpandedResSize(source);
        const std::uint64_t source_hash =
            size != static_cast<u32>(-1)
                ? fnv1a(static_cast<const std::byte*>(source), size)
                : 0;
        if (size != target->size || source_hash != target->hash) {
            char message[240];
            std::snprintf(message, sizeof(message),
                          "Geometry rebuild: source fingerprint rejected for %s "
                          "bytes=%u hash=%016llx",
                          target->label, size,
                          static_cast<unsigned long long>(source_hash));
            svc_log->info(mod_ctx, message);
            return HOOK_CONTINUE;
        }
        const auto begin = std::chrono::steady_clock::now();
        auto rebuilt = bmd_rebuild::split_normals(
            {static_cast<const std::byte*>(source), static_cast<std::size_t>(size)});
        if (!rebuilt.ok()) {
            char message[300];
            std::snprintf(message, sizeof(message), "Geometry rebuild: rejected: %.220s",
                          rebuilt.error.c_str());
            svc_log->info(mod_ctx, message);
            return HOOK_CONTINUE;
        }
        if (rebuilt.bytes.size() > std::numeric_limits<u32>::max()) {
            svc_log->info(mod_ctx, "Geometry rebuild: output exceeds archive allocation limit");
            return HOOK_CONTINUE;
        }
        void* owned = JKRHeap::alloc(static_cast<u32>(rebuilt.bytes.size()), 0x20, info->mDataHeap);
        if (!owned) {
            svc_log->info(mod_ctx, "Geometry rebuild: archive allocation failed");
            return HOOK_CONTINUE;
        }
        std::memcpy(owned, rebuilt.bytes.data(), rebuilt.bytes.size());
        replacements.push_back(Replacement{target->kind, info, source, owned,
                                           static_cast<u32>(rebuilt.bytes.size())});
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                                 std::chrono::steady_clock::now() - begin)
                                 .count();
        char message[300];
        std::snprintf(message, sizeof(message),
                      "Geometry rebuild: %s bytes=%u normals=%u/%u "
                      "triangles=%u us=%lld",
                      target->label,
                      static_cast<unsigned>(rebuilt.bytes.size()),
                      rebuilt.evidence.written_normals,
                      rebuilt.evidence.rebuilt_normal_capacity, rebuilt.evidence.triangles,
                      static_cast<long long>(elapsed));
        svc_log->info(mod_ctx, message);
        return HOOK_CONTINUE;
    }
    return HOOK_CONTINUE;
}

struct MutationBackup {
    enum class Kind { None, Diagnostic, RigidSmoothing, SkinnedSmoothing } kind = Kind::None;
    const char* label = "unknown";
    dRes_info_c* owner = nullptr;
    J3DModelData* model_data = nullptr;
    void* normals = nullptr;
    u32 bytes = 0;
    u32 instances = 0;
    std::unique_ptr<std::byte[]> original;
};
std::vector<MutationBackup> mutations;

bool is_smoothing(MutationBackup::Kind kind) {
    return kind == MutationBackup::Kind::RigidSmoothing ||
           kind == MutationBackup::Kind::SkinnedSmoothing;
}

auto find_mutation(J3DModelData* model) {
    return std::find_if(mutations.begin(), mutations.end(),
                        [model](const MutationBackup& item) {
                            return item.model_data == model;
                        });
}

bool has_mutation_kind(MutationBackup::Kind kind) {
    return std::any_of(mutations.begin(), mutations.end(), [kind](const MutationBackup& item) {
        return item.kind == kind;
    });
}

void restore_mutations(MutationBackup::Kind kind = MutationBackup::Kind::None,
                       dRes_info_c* owner = nullptr) {
    for (auto it = mutations.begin(); it != mutations.end();) {
        if ((kind != MutationBackup::Kind::None && it->kind != kind) ||
            (owner && it->owner != owner)) {
            ++it;
            continue;
        }
        std::memcpy(it->normals, it->original.get(), it->bytes);
        svc_log->info(mod_ctx, is_smoothing(it->kind)
                                   ? "Geometry smoothing: original normals restored"
                                   : "Geometry mutation test: original normals restored");
        it = mutations.erase(it);
    }
}

void maybe_mutate(dRes_info_c& info, const char* file_name, J3DModelData& model) {
    // One exact game-owned BMD, verified offline from the USA GZ2E01 disc.
    if (!delete_registered || !settings::geometry_mutation_test_enabled() || !mutations.empty() ||
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
    mutations.push_back({MutationBackup::Kind::Diagnostic,
                         "L_mbox_00.arc/l_metabox_00.bmd", &info, &model, normals, bytes, 0,
                         std::move(original)});
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
    const auto length = std::strlen(name);
    return info.mArchiveName && length >= 4 && std::strcmp(name + length - 4, ".bmd") == 0;
}

bool is_smoothing_target(const dRes_info_c& info, const char* name) {
    if (!info.mArchiveName)
        return false;
    if (settings::geometry_smoothing_enabled() &&
        std::strcmp(info.mArchiveName, "OBJ_GM") == 0 &&
        std::strcmp(name, "k_kumo_iwa00.bmd") == 0)
        return true;
    const auto rebuilt = std::find_if(replacements.begin(), replacements.end(),
                                      [&info](const Replacement& item) {
                                          return item.owner == &info;
                                      });
    if (rebuilt == replacements.end())
        return false;
    if (rebuilt->kind == Replacement::Kind::Pot)
        return settings::geometry_smoothing_enabled() &&
               std::strcmp(info.mArchiveName, "OBJ_GM") == 0 &&
               std::strcmp(name, "k_kumo_tubo01.bmd") == 0;
    if (rebuilt->kind == Replacement::Kind::Nest)
        return settings::geometry_smoothing_enabled() &&
               std::strcmp(info.mArchiveName, "E_nest") == 0 &&
               std::strcmp(name, "o_hachinosu_01.bmd") == 0;
    if (rebuilt->kind == Replacement::Kind::Pumpkin)
        return settings::geometry_smoothing_enabled() &&
               std::strcmp(info.mArchiveName, "pumpkin") == 0 &&
               std::strcmp(name, "pumpkin.bmd") == 0;
    return settings::geometry_skinned_smoothing_enabled() &&
           std::strcmp(info.mArchiveName, "Kmdl") == 0 && std::strcmp(name, "al.bmd") == 0;
}

bool array_in_resource(const dRes_info_c& info, const J3DModelData& model, const void* array,
                       u32 count, u32 stride) {
    if (!array || !info.mArchive || !model.getRawData() || !stride || count > 65536)
        return false;
    const auto* owned_replacement = replacement_for(model.getRawData());
    const u32 size = owned_replacement ? owned_replacement->size
                                       : info.mArchive->getExpandedResSize(model.getRawData());
    const auto base = reinterpret_cast<std::uintptr_t>(model.getRawData());
    const auto address = reinterpret_cast<std::uintptr_t>(array);
    return size != static_cast<u32>(-1) && address >= base && address - base <= size &&
           count <= (size - (address - base)) / stride;
}

template <class T> std::uint64_t vector_bytes(const std::vector<T>& values) {
    return static_cast<std::uint64_t>(values.capacity()) * sizeof(T);
}
template <class T> std::uint64_t nested_vector_bytes(const std::vector<std::vector<T>>& values) {
    std::uint64_t bytes = vector_bytes(values);
    for (const auto& inner : values)
        bytes += vector_bytes(inner);
    return bytes;
}

void analyze_topology(const dRes_info_c& info, const char* name, J3DModelData& model) {
    const bool target = is_smoothing_target(info, name);
    const bool apply = target && !has_mutation_kind(MutationBackup::Kind::Diagnostic) &&
                       find_mutation(&model) == mutations.end();
    if ((!settings::topology_diagnostics_enabled() || !is_topology_target(info, name)) && !apply)
        return;
    const auto begin = std::chrono::steady_clock::now();
    const auto* owned_replacement = replacement_for(model.getRawData());
    const u32 resource_size = owned_replacement
                                  ? owned_replacement->size
                                  : info.mArchive->getExpandedResSize(model.getRawData());
    const std::uint64_t resource_hash =
        resource_size != static_cast<u32>(-1)
            ? fnv1a(static_cast<const std::byte*>(model.getRawData()), resource_size)
            : 0;
    const J3DVertexData& vertex = model.getVertexData();
    const u32 pos_count = vertex.getVtxArrNum(GX_VA_POS);
    const u32 normal_count = vertex.getNrmNum();
    const u32 stride = vertex.getVtxArrStride(GX_VA_POS);
    const int pos_type = vertex.getVtxPosType();
    topology::Result result;
    std::uint64_t peak_vector_bytes = 0;
    if (!array_in_resource(info, model, vertex.getVtxPosArray(), pos_count, stride) ||
        pos_count == 0 ||
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
        peak_vector_bytes = vector_bytes(positions) + vector_bytes(formats) +
                            nested_vector_bytes(attrs) + nested_vector_bytes(groups) +
                            vector_bytes(shapes) + vector_bytes(result.triangles) +
                            result.peak_temporary_vector_bytes;
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                             std::chrono::steady_clock::now() - begin)
                             .count();
    smoothing::Result smooth;
    long long smooth_us = 0;
    long long normal_decode_us = 0;
    const int normal_type = vertex.getVtxNrmType();
    const u32 normal_stride = vertex.getVtxArrStride(GX_VA_NRM);
    if (result.ok() && !vertex.getVtxNBTArray() &&
        array_in_resource(info, model, vertex.getVtxNrmArray(), normal_count, normal_stride) &&
        ((normal_type == GX_F32 && normal_stride == 12) ||
         (normal_type == GX_S16 && normal_stride == 6))) {
        const auto normal_begin = std::chrono::steady_clock::now();
        std::vector<topology::Vec3> normals(normal_count);
        const auto* bytes = static_cast<const std::byte*>(vertex.getVtxNrmArray());
        const float scale = std::ldexp(1.0f, -static_cast<int>(vertex.getVtxNrmFrac()));
        for (u32 i = 0; i < normal_count; ++i) {
            float values[3];
            for (unsigned c = 0; c < 3; ++c) {
                if (normal_type == GX_F32)
                    std::memcpy(&values[c], bytes + i * normal_stride + c * 4, 4);
                else {
                    std::int16_t value;
                    std::memcpy(&value, bytes + i * normal_stride + c * 2, 2);
                    values[c] = value * scale;
                }
            }
            normals[i] = {values[0], values[1], values[2]};
        }
        const auto smoothing_begin = std::chrono::steady_clock::now();
        normal_decode_us =
            std::chrono::duration_cast<std::chrono::microseconds>(smoothing_begin - normal_begin)
                .count();
        smoothing::Options options;
        options.face_angle_degrees = settings::geometry_smoothing_angle();
        smooth = smoothing::plan(result, normals, options);
        smooth_us = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now() - smoothing_begin)
                        .count();
        peak_vector_bytes =
            std::max(peak_vector_bytes, vector_bytes(result.triangles) + vector_bytes(normals) +
                                            smooth.working_vector_bytes);
    } else {
        smooth.error = "unsupported normal array or topology";
    }
    if (apply) {
        const auto* rebuilt = replacement_for(model.getRawData());
        const bool pot_exact = rebuilt && rebuilt->kind == Replacement::Kind::Pot &&
                               result.corner_hash == 0xc25e3fbcbca371d2ULL && pos_count == 90 &&
                               normal_count == 880 && model.getWEvlpMtxNum() == 0;
        const bool link_exact = rebuilt && rebuilt->kind == Replacement::Kind::LinkBody &&
                                result.corner_hash == 0x6ed5b0df43c35b63ULL && pos_count == 1560 &&
                                normal_count == 10064 && model.getWEvlpMtxNum() == 110;
        const bool nest_exact = rebuilt && rebuilt->kind == Replacement::Kind::Nest &&
                                result.corner_hash == 0xd107e10bca9ec9d0ULL && pos_count == 61 &&
                                normal_count == 400 && model.getWEvlpMtxNum() == 0;
        const bool pumpkin_exact = rebuilt && rebuilt->kind == Replacement::Kind::Pumpkin &&
                                   result.corner_hash == 0x2b808b13bc7ab6e8ULL &&
                                   pos_count == 306 && normal_count == 1818 &&
                                   model.getWEvlpMtxNum() == 0;
        const bool rock_exact = !rebuilt && std::strcmp(info.mArchiveName, "OBJ_GM") == 0 &&
                                std::strcmp(name, "k_kumo_iwa00.bmd") == 0 &&
                                resource_size == 13856 &&
                                resource_hash == 0xaae14e74c4cca0b8ULL &&
                                result.corner_hash == 0x4905bff69be3ae64ULL && pos_count == 40 &&
                                normal_count == 197 && model.getWEvlpMtxNum() == 0;
        const unsigned normal_fraction = vertex.getVtxNrmFrac();
        const bool exact = normal_type == GX_S16 && normal_stride == 6 &&
                           (((pot_exact || link_exact) && normal_fraction == 14) ||
                            ((nest_exact || pumpkin_exact) && normal_fraction == 15) ||
                            (rock_exact && normal_fraction == 15));
        if (exact && smooth.safe() && smooth.changed_indices > 0 && delete_registered) {
            const u32 bytes_count = normal_count * normal_stride;
            std::unique_ptr<std::byte[]> backup{new (std::nothrow) std::byte[bytes_count]};
            if (backup) {
                auto* destination = static_cast<std::byte*>(vertex.getVtxNrmArray());
                std::memcpy(backup.get(), destination, bytes_count);
                for (u32 i = 0; i < normal_count; ++i) {
                    std::int16_t encoded[3];
                    // Source values are S16, and the plan only returns finite originals or
                    // normalized weighted faces; the checked codec rejects any regression.
                    if (!smoothing::encode_s16_xyz(smooth.normals[i], normal_fraction, encoded)) {
                        std::memcpy(destination, backup.get(), bytes_count);
                        svc_log->info(mod_ctx, "Geometry smoothing: normal encoding rejected");
                        return;
                    }
                    std::memcpy(destination + i * 6, encoded, 6);
                }
                const char* label = pot_exact    ? "OBJ_GM.arc/k_kumo_tubo01.bmd"
                                    : link_exact ? "Kmdl.arc/al.bmd"
                                    : nest_exact ? "E_nest.arc/o_hachinosu_01.bmd"
                                    : pumpkin_exact ? "pumpkin.arc/pumpkin.bmd"
                                                 : "OBJ_GM.arc/k_kumo_iwa00.bmd";
                mutations.push_back({link_exact ? MutationBackup::Kind::SkinnedSmoothing
                                                : MutationBackup::Kind::RigidSmoothing,
                                     label, const_cast<dRes_info_c*>(&info), &model, destination,
                                     bytes_count, 0, std::move(backup)});
                svc_log->info(
                    mod_ctx,
                    pot_exact ? "Geometry smoothing: applied OBJ_GM.arc/k_kumo_tubo01.bmd"
                              : link_exact ? "Geometry smoothing: applied Kmdl.arc/al.bmd"
                              : nest_exact ? "Geometry smoothing: applied "
                                             "E_nest.arc/o_hachinosu_01.bmd"
                              : pumpkin_exact ? "Geometry smoothing: applied "
                                                "pumpkin.arc/pumpkin.bmd"
                                           : "Geometry smoothing: applied "
                                             "OBJ_GM.arc/k_kumo_iwa00.bmd");
            }
        } else {
            svc_log->info(mod_ctx, "Geometry smoothing: target rejected by safety checks");
        }
    }
    const auto total_us = std::chrono::duration_cast<std::chrono::microseconds>(
                              std::chrono::steady_clock::now() - begin)
                              .count();
    char report[850];
    std::snprintf(
        report, sizeof(report),
        "Topology JSON: {\"archive\":\"%s\",\"file\":\"%s\",\"resourceBytes\":%u,"
        "\"resourceHash\":\"%016llx\",\"positions\":%u,"
        "\"normals\":%u,\"shapes\":%u,\"envelopes\":%u,\"primitives\":%u,"
        "\"normalType\":%d,\"normalStride\":%u,\"normalFrac\":%u,"
        "\"strips\":%u,\"fans\":%u,\"indexed\":%u,\"triangles\":%zu,"
        "\"degenerate\":%u,\"uniquePositions\":%u,\"uniqueNormals\":%u,"
        "\"positionNormalSplits\":%u,\"cornerHash\":\"%016llx\","
        "\"decodeUs\":%lld,\"error\":\"%s\",\"smoothGroups\":%u,"
        "\"smoothCandidates\":%u,\"smoothChanges\":%u,\"indexConflicts\":%u,"
        "\"ambiguousFaces\":%u,\"normalDecodeUs\":%lld,\"adjacencyUs\":%llu,"
        "\"smoothUs\":%lld,\"totalUs\":%lld,\"peakTrackedVectorBytes\":%llu,\"backupBytes\":%u,"
        "\"cacheEntries\":%u,\"smoothError\":\"%s\"}",
        info.mArchiveName, name, resource_size,
        static_cast<unsigned long long>(resource_hash), pos_count, normal_count,
        model.getShapeNum(),
        model.getWEvlpMtxNum(), result.primitive_count, normal_type, normal_stride,
        vertex.getVtxNrmFrac(), result.strip_count, result.fan_count,
        result.indexed_count, result.triangles.size(), result.degenerate_count,
        result.unique_positions, result.unique_normals, result.position_normal_splits,
        static_cast<unsigned long long>(result.corner_hash), static_cast<long long>(elapsed),
        result.error ? result.error : "", smooth.smoothing_groups, smooth.candidate_indices,
        smooth.changed_indices, smooth.index_conflicts, smooth.ambiguous_faces, normal_decode_us,
        static_cast<unsigned long long>(smooth.adjacency_us), smooth_us,
        static_cast<long long>(total_us), static_cast<unsigned long long>(peak_vector_bytes),
        find_mutation(&model) != mutations.end() ? find_mutation(&model)->bytes : 0,
        static_cast<unsigned>(std::count_if(
            mutations.begin(), mutations.end(), [](const MutationBackup& item) {
                return is_smoothing(item.kind);
            })),
        smooth.error ? smooth.error : "");
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
    auto* info = args ? mods::arg<dRes_info_c*>(args, 0) : nullptr;
    if (info) {
        const auto owner = std::find(loading_owners.rbegin(), loading_owners.rend(), info);
        if (owner != loading_owners.rend())
            loading_owners.erase(std::next(owner).base());
    }
    if ((!settings::geometry_diagnostics_enabled() && !settings::geometry_mutation_test_enabled() &&
         !settings::topology_diagnostics_enabled() && !settings::geometry_smoothing_enabled() &&
         !settings::geometry_skinned_smoothing_enabled()) ||
        !args || !retval || *static_cast<int*>(retval) < 0)
        return;
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
    auto* owner = args ? mods::arg<dRes_info_c*>(args, 0) : nullptr;
    if (!owner)
        return HOOK_CONTINUE;
    if (std::any_of(mutations.begin(), mutations.end(),
                    [owner](const MutationBackup& item) {
                        return item.owner == owner && is_smoothing(item.kind);
                    }))
        svc_log->info(mod_ctx, "Geometry smoothing: archive pre-delete restoration");
    restore_mutations(MutationBackup::Kind::None, owner);
    const auto old_size = replacements.size();
    std::erase_if(replacements, [owner](const Replacement& item) {
        return item.owner == owner;
    });
    if (replacements.size() != old_size)
        svc_log->info(mod_ctx, "Geometry rebuild: archive replacement released");
    return HOOK_CONTINUE;
}

void on_model_created(ModContext*, void* args, void* retval, void*) {
    if (!args || !retval || !*static_cast<J3DModel**>(retval))
        return;
    auto found = find_mutation(mods::arg<J3DModelData*>(args, 0));
    if (found == mutations.end() || !is_smoothing(found->kind))
        return;
    char message[160];
    std::snprintf(message, sizeof(message), "Geometry smoothing: %s instance %u", found->label,
                  ++found->instances);
    svc_log->info(mod_ctx, message);
}
} // namespace

void initialize() {
    load_registered = false;
    delete_registered = false;
    create_registered = false;
    model_load_registered = false;
    replacements.clear();
    loading_owners.clear();
    mutations.clear();
    if (!svc_hook)
        return;
    if (mods::hook::add_pre<ModelLoad>(use_rebuilt_model) == MOD_OK)
        model_load_registered = true;
    else
        (void)mods::hook::uninstall<ModelLoad>();
    if (mods::hook::add_pre<ResourceDelete>(on_resource_delete) == MOD_OK)
        delete_registered = true;
    else
        (void)mods::hook::uninstall<ResourceDelete>();
    if (mods::hook::add_pre<ResourceLoad>(prepare_rebuilt_model) == MOD_OK &&
        mods::hook::add_post<ResourceLoad>(on_resource_loaded) == MOD_OK) {
        load_registered = true;
    } else {
        (void)mods::hook::uninstall<ResourceLoad>();
    }
    if (mods::hook::add_post<ModelCreate>(on_model_created) == MOD_OK)
        create_registered = true;
    else
        (void)mods::hook::uninstall<ModelCreate>();
}

void shutdown() {
    if (load_registered)
        (void)mods::hook::uninstall<ResourceLoad>();
    restore_mutations();
    if (delete_registered)
        (void)mods::hook::uninstall<ResourceDelete>();
    if (create_registered)
        (void)mods::hook::uninstall<ModelCreate>();
    if (model_load_registered)
        (void)mods::hook::uninstall<ModelLoad>();
    load_registered = delete_registered = create_registered = false;
    model_load_registered = false;
    replacements.clear();
    loading_owners.clear();
}

void restore_mutation() {
    restore_mutations(MutationBackup::Kind::Diagnostic);
}
void restore_smoothing(bool skinned) {
    restore_mutations(skinned ? MutationBackup::Kind::SkinnedSmoothing
                              : MutationBackup::Kind::RigidSmoothing);
}
} // namespace midnafx::geometry_probe
