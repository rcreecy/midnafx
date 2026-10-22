#include "asset_probe.hpp"
#include "asset_identity.hpp"
#include "material_scope.hpp"
#include "services.hpp"
#include "ui/settings.hpp"
#include <JSystem/J3DGraphAnimator/J3DModelData.h>
#include <JSystem/J3DGraphBase/J3DMaterial.h>
#include <JSystem/J3DGraphBase/J3DShape.h>
#include <dolphin/gx/GXAurora.h>
#include <dolphin/gx/GXDispList.h>
#include <mods/svc/hook.hpp>
#include <JSystem/JKernel/JKRArchive.h>
#include <d/d_resorce.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace midnafx::asset_probe {
namespace {
// Values are copied; owner/model addresses are session-only lookup keys. No
// instance pointer is retained, and archive deletion invalidates every binding.
struct Record {
    dRes_info_c* owner;
    unsigned index;
    J3DModelData* model = nullptr;
    std::string id;
    unsigned creations = 0;
    unsigned draw_samples = 0;
};
std::vector<Record> records;
unsigned events = 0;
bool limited = false;
DEFINE_HOOK(&J3DShape::drawFast, AssetShapeDraw);
bool draw_hook = false;
bool draw_scopes[8]{};
bool body_scopes[8]{};
void* draw_args[8]{};
unsigned draw_depth = 0;
unsigned long long draw_epoch = 0;
unsigned long long draw_token = 0;
J3DMaterial* pumpkin_materials[2]{};
dRes_info_c* pumpkin_owner = nullptr;
unsigned long long draw_probe_calls = 0, draw_probe_ns = 0;
std::atomic<bool> draw_sample_requested{false};
std::atomic<bool> body_control_requested{false};
struct BodyBinding {
    dRes_info_c* owner = nullptr;
    unsigned index = 0;
    J3DMaterial* material = nullptr;
} body_binding;
bool body_control_last = false;
unsigned long long body_applies = 0, body_restores = 0, body_rejections = 0;
void body_event(const char* event) noexcept {
    if (!svc_log) return;
    char message[256];
    std::snprintf(message, sizeof(message),
        "MaterialControl event=%s requested=%u bound=%u applies=%llu restores=%llu rejected=%llu",
        event, unsigned(body_control_requested.load(std::memory_order_relaxed)),
        unsigned(body_binding.material != nullptr), body_applies, body_restores, body_rejections);
    svc_log->info(mod_ctx, message);
}
bool qualifies_body(J3DMaterial* material) noexcept {
    if (!material || material->getIndex() != 1 || !material->getTevBlock() ||
        !texture_capacity(material->getTevBlock()->getType())) return false;
    auto* block = material->getTevBlock();
    if (block->getTevStageNum() != 1) return false;
    const auto* stage = block->getTevStage(0);
    return stage && stage->field_0x0 == 0xc0 && material_scope::qualifies_stage(
        block->getTevStageNum(), stage->mTevColorOp, stage->mTevColorAB, stage->mTevColorCD);
}
struct DrawProbeTimer {
    bool active;
    std::chrono::steady_clock::time_point start;
    explicit DrawProbeTimer(bool enabled) : active(enabled) {
        if (active) start = std::chrono::steady_clock::now();
    }
    ~DrawProbeTimer() {
        if (!active) return;
        draw_probe_ns += static_cast<unsigned long long>(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - start).count());
    }
};
std::string quote(const char* s) {
    std::string out = "\"";
    for (unsigned i = 0; s && s[i]; ++i) {
        const unsigned char c = s[i];
        if (c == '"' || c == '\\') out += '\\';
        if (c >= 32 && c < 127) out += char(c);
        else { char escaped[7]; std::snprintf(escaped, sizeof(escaped), "\\u%04x", c); out += escaped; }
    }
    return out + '"';
}
void emit(const std::string& body) {
    if (events < 4096 && svc_log) {
        ++events;
        const auto& message = events == 4096 ?
            std::string("\"event\":\"unsupported\",\"reason\":\"event limit reached; discovery incomplete\"") : body;
        svc_log->info(mod_ctx, ("AssetGraph {\"schema\":1," + message + "}").c_str());
    }
}
void failure() noexcept {
    if (svc_log) svc_log->warn(mod_ctx, "AssetGraph diagnostic failed; game resource unchanged");
}
HookAction before_shape(ModContext*, void* args, void*, void*) {
    const bool control = body_control_requested.load(std::memory_order_relaxed);
    if (control != body_control_last) {
        body_control_last = control;
        body_event(control ? "enabled" : "disabled");
    }
    const bool observe = settings::asset_draw_diagnostics_enabled() && settings::asset_diagnostics_enabled();
    if (!draw_depth && !observe && !control) return HOOK_CONTINUE;
    if (!args || draw_depth >= 8) return HOOK_CONTINUE;
    DrawProbeTimer timer(observe);
    if (observe) ++draw_probe_calls;
    const auto depth = draw_depth++;
    draw_args[depth] = args;
    draw_scopes[depth] = false;
    body_scopes[depth] = false;
    try {
        const auto* shape = mods::arg<const J3DShape*>(args, 0);
        auto* material = shape ? shape->getMaterial() : nullptr;
        // The pinned drawFast path is non-recursive and leaves TEV state alone.
        // This is a static, qualified SCALE_4 restore, not a generic state stack.
        if (control && !depth && material && material == body_binding.material) {
            if (qualifies_body(material)) {
                GXCallDisplayList(material_scope::apply.bytes.data(), sizeof(material_scope::apply));
                body_scopes[depth] = true;
                ++body_applies;
            } else {
                ++body_rejections;
                body_binding.material = nullptr;
                body_event("stage-rejected");
            }
        }
        if (!observe) return HOOK_CONTINUE;
        if (!depth && draw_sample_requested.load(std::memory_order_relaxed) &&
            draw_sample_requested.exchange(false, std::memory_order_relaxed)) {
            // Only an explicit request scans the bounded registry. All model
            // dereferences stay on the shape callback's game thread.
            for (auto& record : records) {
                if (record.id != "pumpkin.arc/pumpkin.bmd#fnv1a64:9b2ddb5ecbd95421" ||
                    !record.model || record.model->getMaterialNum() != 2) continue;
                record.draw_samples = 0;
                pumpkin_owner = record.owner;
                for (unsigned m = 0; m < 2; ++m) pumpkin_materials[m] = record.model->getMaterialNodePointer(m);
                emit("\"event\":\"draw-resample\",\"id\":" + quote(record.id.c_str()) +
                     ",\"next_token\":" + std::to_string(draw_token + 1));
                break;
            }
        }
        if (!material || (material != pumpkin_materials[0] && material != pumpkin_materials[1]))
            return HOOK_CONTINUE;
        for (auto& record : records) {
            if (record.id != "pumpkin.arc/pumpkin.bmd#fnv1a64:9b2ddb5ecbd95421" ||
                !record.model || record.draw_samples >= 32) continue;
            const auto index = material->getIndex();
            if (index >= record.model->getMaterialNum() || record.model->getMaterialNodePointer(index) != material)
                continue;
            const auto token = ++draw_token;
            const auto label = "MidnaFXAssetV1:" + std::to_string(draw_epoch) + ":" + std::to_string(token);
            emit("\"event\":\"draw-label\",\"id\":" + quote(record.id.c_str()) +
                 ",\"material\":" + std::to_string(index) + ",\"label\":" + quote(label.c_str()) +
                 ",\"provenance\":\"J3DShape-drawFast-entry-not-GPU-proof\"");
            GXPushDebugGroup(label.c_str());
            draw_scopes[depth] = true;
            ++record.draw_samples;
            if (record.draw_samples == 32) pumpkin_materials[0] = pumpkin_materials[1] = nullptr;
            break;
        }
    } catch (...) { failure(); }
    return HOOK_CONTINUE;
}
void after_shape(ModContext*, void* args, void*, void*) {
    // A preceding hook may skip our pre callback while all post callbacks still
    // execute. Match the trampoline's invocation array before consuming a scope.
    if (!draw_depth || draw_args[draw_depth - 1] != args) return;
    DrawProbeTimer timer(settings::asset_draw_diagnostics_enabled() && settings::asset_diagnostics_enabled());
    const auto depth = --draw_depth;
    draw_args[depth] = nullptr;
    // Restoration cannot depend on the current UI request: it may change between
    // callbacks. The copied FIFO bytes hold no pointer into the game material.
    if (depth < 8 && body_scopes[depth]) {
        GXCallDisplayList(material_scope::restore.bytes.data(), sizeof(material_scope::restore));
        body_scopes[depth] = false;
        ++body_restores;
    }
    if (depth < 8 && draw_scopes[depth]) {
        GXPopDebugGroup();
        draw_scopes[depth] = false;
    }
}
}
void set_body_control(bool enabled) noexcept {
    body_control_requested.store(enabled, std::memory_order_relaxed);
}
bool body_control_enabled() noexcept {
    return body_control_requested.load(std::memory_order_relaxed);
}
void request_draw_sample() noexcept {
    if (settings::asset_draw_diagnostics_enabled() && settings::asset_diagnostics_enabled())
        draw_sample_requested.store(true, std::memory_order_relaxed);
}
void initialize_draw_probe() {
    draw_epoch = static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count());
    draw_token = 0;
    draw_probe_calls = draw_probe_ns = 0;
    body_applies = body_restores = body_rejections = 0;
    body_control_last = false;
    if (!svc_hook) return;
    draw_hook = mods::hook::add_pre<AssetShapeDraw>(before_shape) == MOD_OK &&
                mods::hook::add_post<AssetShapeDraw>(after_shape) == MOD_OK;
    if (!draw_hook) {
        (void)mods::hook::uninstall<AssetShapeDraw>();
        if (svc_log) svc_log->warn(mod_ctx, "Asset shape hooks unavailable; labels and body control disabled");
    }
}
void shutdown_draw_probe() {
    body_event("shutdown");
    if (draw_probe_calls) {
        try { emit("\"event\":\"draw-cost\",\"callbacks\":" + std::to_string(draw_probe_calls) +
                   ",\"observer_ns\":" + std::to_string(draw_probe_ns)); }
        catch (...) { failure(); }
    }
    if (draw_hook) (void)mods::hook::uninstall<AssetShapeDraw>();
    draw_hook = false;
    draw_depth = 0;
}
void begin(dRes_info_c* owner) noexcept {
    if (!owner || !owner->mArchive || !owner->mArchiveName) return;
    try {
        // Independent of discovery limits and diagnostic logging. Enabling after
        // an unobserved load requires reloading that archive.
        if (draw_hook && body_control_enabled() && !std::strcmp(owner->mArchiveName, "pumpkin")) {
            body_binding = {};
            auto* archive = owner->mArchive;
            for (unsigned i = 0; i < unsigned(archive->countFile()); ++i) {
                if (!archive->isFileEntry(i)) continue;
                auto* entry = archive->findIdxResource(i);
                if (!entry) continue;
                const char* name = archive->mStringTable + (entry->type_flags_and_name_offset & 0xffffff);
                if (std::strcmp(name, "pumpkin.bmd")) continue;
                const auto* bytes = static_cast<const unsigned char*>(archive->getIdxResource(i));
                const auto size = bytes ? archive->getExpandedResSize(bytes) : 0;
                if (!bytes || size < 32 || size > 8 * 1024 * 1024 || std::memcmp(bytes, "J3D2bmd3", 8)) break;
                unsigned long long hash = 14695981039346656037ULL;
                for (unsigned j = 0; j < size; ++j) { hash ^= bytes[j]; hash *= 1099511628211ULL; }
                if (hash == 0x9b2ddb5ecbd95421ULL) body_binding = {owner, i, nullptr};
                else body_event("fingerprint-rejected");
                break;
            }
        }
        if (!settings::asset_diagnostics_enabled()) return;
        const auto start = std::chrono::steady_clock::now();
        auto* archive = owner->mArchive;
        if (records.size() == 128 && !limited) {
            limited = true;
            emit("\"event\":\"unsupported\",\"reason\":\"resource limit reached; discovery incomplete\"");
        }
        // Reuse the archive's validated file table; do not parse arbitrary memory.
        for (unsigned i = 0; i < unsigned(archive->countFile()) && records.size() < 128; ++i) {
            if (!archive->isFileEntry(i)) continue;
            auto* entry = archive->findIdxResource(i);
            if (!entry) continue;
            const char* name = archive->mStringTable + (entry->type_flags_and_name_offset & 0xffffff);
            const auto length = std::strlen(name);
            if (length < 4 || std::strcmp(name + length - 4, ".bmd")) continue;
            const auto* bytes = static_cast<const unsigned char*>(archive->getIdxResource(i));
            const auto size = bytes ? archive->getExpandedResSize(bytes) : 0;
            if (!bytes || size < 32 || size > 8 * 1024 * 1024 || std::memcmp(bytes, "J3D2bmd3", 8)) {
                emit("\"event\":\"unsupported\",\"reason\":\"resource format or bounds\""); continue;
            }
            unsigned long long hash = 14695981039346656037ULL;
            for (unsigned j = 0; j < size; ++j) { hash ^= bytes[j]; hash *= 1099511628211ULL; }
            char hex[17]; std::snprintf(hex, sizeof(hex), "%016llx", hash);
            const std::string id = std::string(owner->mArchiveName) + ".arc/" + name + "#fnv1a64:" + hex;
            records.push_back({owner, i, nullptr, id, 0});
            emit("\"event\":\"resource\",\"id\":" + quote(id.c_str()) +
                 ",\"archive\":" + quote(owner->mArchiveName) + ",\"resource\":" + quote(name) +
                 ",\"bytes\":" + std::to_string(size) + ",\"provenance\":\"archive-pre-load\"");
        }
        std::size_t retained_bytes = records.capacity() * sizeof(Record);
        for (const auto& record : records) retained_bytes += record.id.capacity() + 1;
        const auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
        emit("\"event\":\"cost\",\"load_us\":" + std::to_string(us) +
             ",\"retained_records\":" + std::to_string(records.size()) +
             ",\"retained_metadata_bytes_estimate\":" + std::to_string(retained_bytes) +
             ",\"limit\":128");
    } catch (...) { failure(); }
}
void loaded(dRes_info_c* owner, unsigned index, J3DModelData* model) noexcept {
    if (!model) return;
    try {
        if (owner == body_binding.owner && index == body_binding.index) {
            auto* material = model->getMaterialNum() == 2 ? model->getMaterialNodePointer(1) : nullptr;
            if (qualifies_body(material)) {
                body_binding.material = material;
                body_event("bound-pumpkin-body");
            } else {
                body_binding = {};
                ++body_rejections;
                body_event("model-rejected");
            }
        }
        if (!settings::asset_diagnostics_enabled()) return;
        auto it = std::find_if(records.begin(), records.end(), [&](const Record& r) {return r.owner==owner && r.index==index;});
        if (it == records.end()) return;
        it->model = model;
        if (it->id == "pumpkin.arc/pumpkin.bmd#fnv1a64:9b2ddb5ecbd95421" && model->getMaterialNum() == 2) {
            pumpkin_owner = owner;
            for (unsigned m = 0; m < 2; ++m) pumpkin_materials[m] = model->getMaterialNodePointer(m);
        }
        if (model->getMaterialNum() > 256) { emit("\"event\":\"unsupported\",\"reason\":\"material limit\""); return; }
        for (unsigned m=0; m<model->getMaterialNum(); ++m) {
            auto* material = model->getMaterialNodePointer(m);
            if (!material || !material->getTevBlock()) continue;
            const auto capacity = texture_capacity(material->getTevBlock()->getType());
            std::string slots = "[";
            for (unsigned s=0; s<8; ++s) { if(s) slots+=','; slots+= s < capacity ? std::to_string(material->getTexNo(s)) : "null"; }
            slots += ']';
            emit("\"event\":\"material\",\"id\":" + quote(it->id.c_str()) +
                 ",\"material\":" + std::to_string(m) + ",\"texture_slots\":" + slots +
                 ",\"provenance\":\"loaded-J3D-initial-slots\"");
        }
    } catch (...) { failure(); }
}
void created(J3DModelData* model) noexcept {
    if (!settings::asset_diagnostics_enabled() || !model) return;
    try {
        auto it=std::find_if(records.begin(),records.end(),[&](const Record& r){return r.model==model;});
        if(it!=records.end()) emit("\"event\":\"creation\",\"id\":" + quote(it->id.c_str()) +
            ",\"ordinal\":" + std::to_string(++it->creations) + ",\"provenance\":\"mDoExt-model-create\"");
    } catch (...) { failure(); }
}
void erase(dRes_info_c* owner) noexcept {
    if (owner && owner == body_binding.owner) {
        body_binding = {};
        body_event("archive-delete");
    }
    if (owner == pumpkin_owner) {
        pumpkin_materials[0] = pumpkin_materials[1] = nullptr;
        pumpkin_owner = nullptr;
    }
    try {
        for (const auto& r:records) if(r.owner==owner && settings::asset_diagnostics_enabled())
            emit("\"event\":\"archive-delete\",\"id\":"+quote(r.id.c_str()));
    } catch (...) { failure(); }
    std::erase_if(records,[&](const Record& r){return r.owner==owner;});
}
void reset() noexcept {
    body_binding = {};
    draw_sample_requested.store(false, std::memory_order_relaxed);
    pumpkin_materials[0] = pumpkin_materials[1] = nullptr;
    pumpkin_owner = nullptr;
    // Release diagnostic storage on mod shutdown/reload, including vector capacity.
    std::vector<Record>().swap(records);
    events=0;
    limited=false;
}
}
