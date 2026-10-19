#include "asset_probe.hpp"
#include "asset_identity.hpp"
#include "services.hpp"
#include "ui/settings.hpp"
#include <JSystem/J3DGraphAnimator/J3DModelData.h>
#include <JSystem/J3DGraphBase/J3DMaterial.h>
#include <JSystem/JKernel/JKRArchive.h>
#include <d/d_resorce.h>
#include <algorithm>
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
};
std::vector<Record> records;
unsigned events = 0;
bool limited = false;
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
}
void begin(dRes_info_c* owner) noexcept {
    if (!settings::asset_diagnostics_enabled() || !owner || !owner->mArchive || !owner->mArchiveName) return;
    try {
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
    if (!settings::asset_diagnostics_enabled() || !model) return;
    try {
        auto it = std::find_if(records.begin(), records.end(), [&](const Record& r) {return r.owner==owner && r.index==index;});
        if (it == records.end()) return;
        it->model = model;
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
    try {
        for (const auto& r:records) if(r.owner==owner && settings::asset_diagnostics_enabled())
            emit("\"event\":\"archive-delete\",\"id\":"+quote(r.id.c_str()));
    } catch (...) { failure(); }
    std::erase_if(records,[&](const Record& r){return r.owner==owner;});
}
void reset() noexcept {
    // Release diagnostic storage on mod shutdown/reload, including vector capacity.
    std::vector<Record>().swap(records);
    events=0;
    limited=false;
}
}
