#include "surface_profiles.hpp"
#include "surface_class.hpp"
#include "services.hpp"
#include "geometry_probe.hpp"
#include "render/material_relighting.hpp"
#include <JSystem/J3DGraphAnimator/J3DModelData.h>
#include <JSystem/J3DGraphBase/J3DMaterial.h>
#include <JSystem/J3DGraphBase/J3DTexture.h>
#include <JSystem/JUtility/JUTNameTab.h>
#include <unordered_map>
#include <cstdio>
#include <d/d_com_inf_game.h>
#include <d/d_resorce.h>
#include <JSystem/JKernel/JKRArchive.h>
namespace midnafx::surface_profiles {
namespace {
struct Entry { dRes_info_c* owner; unsigned kind; };
std::unordered_map<const J3DMaterial*,Entry> profiles;
unsigned owner_cursor=0,node_cursor=0,file_cursor=0;
}
void remember(dRes_info_c* owner,J3DModelData* model) noexcept {
    if (!render::material_relighting::surface_profiles_available() || !owner || !model ||
        model->getMaterialNum()>4096 || !model->getMaterialName()) return;
    try {
        std::array<unsigned,surface_class::Count> counts{};
        for (unsigned i=0;i<model->getMaterialNum() && profiles.size()<65536;++i) {
            auto* material=model->getMaterialNodePointer(i);
            if (!material || !material->getTevBlock()) continue;
            const auto* name=model->getMaterialName()->getName(i);
            const char* texture_name=nullptr;
            const auto texture=material->getTexNo(0);
            if (model->getTexture() && model->getTextureName() && texture<model->getTexture()->getNum())
                texture_name=model->getTextureName()->getName(texture);
            const auto kind=surface_class::classify(name ? name : "",texture_name ? texture_name : "");
            auto [it,inserted]=profiles.try_emplace(material,Entry{owner,kind});
            // Shared ownership cannot establish which resource supplies the finish.
            if (!inserted && it->second.owner!=owner) it->second={nullptr,0};
            ++counts[kind];
        }
        if (counts[1]+counts[2]+counts[3]+counts[4]+counts[5]+counts[6]) {
            char message[200];
            std::snprintf(message,sizeof(message),"Material finishes cached: rock=%u wood=%u cloth=%u metal=%u wet=%u leather=%u fallback=%u",
                counts[1],counts[2],counts[3],counts[4],counts[5],counts[6],counts[0]);
            svc_log->info(mod_ctx,message);
        }
    } catch (...) { /* Uncached materials retain the existing lighting response. */ }
}
unsigned lookup(const J3DMaterial* material) noexcept {
    const auto it=profiles.find(material);
    return it==profiles.end() ? 0 : it->second.kind;
}
void erase(dRes_info_c* owner) noexcept { std::erase_if(profiles,[&](const auto& item){return item.second.owner==owner;}); }
void clear() noexcept { profiles.clear(); owner_cursor=node_cursor=file_cursor=0; }
void update() noexcept {
    if (owner_cursor>=192 || geometry_probe::resource_load_active() ||
        !render::material_relighting::surface_profiles_available()) return;
    unsigned models=0;
    for (unsigned budget=0;budget<8 && owner_cursor<192;++budget) {
        auto& resources=g_dComIfG_gameInfo.mResControl;
        auto& owner=owner_cursor<128 ? resources.mObjectInfo[owner_cursor] : resources.mStageInfo[owner_cursor-128];
        auto* archive=owner.mArchive;
        if (owner.mCount && !owner.mDMCommand && owner.mRes && archive && archive->isMounted() &&
            archive->mArcInfoBlock && archive->mNodes && archive->countFile()<=4096 &&
            archive->countDirectory()>=0 && archive->countDirectory()<=4096) {
            for (;node_cursor<unsigned(archive->countDirectory());++node_cursor,file_cursor=0) {
                const auto& node=archive->mNodes[node_cursor];
                if (node.type!='BMDR' && node.type!='BMDV' && node.type!='BMDE' && node.type!='BMWR' &&
                    node.type!='BMWE' && node.type!='BMDP' && node.type!='BMDG' && node.type!='BMDA') continue;
                const unsigned count=archive->countFile();
                if (node.first_file_index>count || node.num_entries>count-node.first_file_index) continue;
                while (file_cursor<node.num_entries) {
                    const unsigned index=node.first_file_index+file_cursor++;
                    if (!archive->isFileEntry(index) || !owner.mRes[index]) continue;
                    remember(&owner,static_cast<J3DModelData*>(owner.mRes[index]));
                    if (++models>=4) return;
                }
            }
        }
        ++owner_cursor; node_cursor=file_cursor=0;
    }
}
}
