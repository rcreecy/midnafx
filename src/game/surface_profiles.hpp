#pragma once
class dRes_info_c;
class J3DModelData;
class J3DMaterial;
namespace midnafx::surface_profiles {
void remember(dRes_info_c* owner, J3DModelData* model) noexcept;
void erase(dRes_info_c* owner) noexcept;
void clear() noexcept;
void update() noexcept;
unsigned lookup(const J3DMaterial* material) noexcept;
}
