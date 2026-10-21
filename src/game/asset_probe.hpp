#pragma once
class dRes_info_c;
class J3DModelData;
namespace midnafx::asset_probe {
void begin(dRes_info_c* owner) noexcept;
void loaded(dRes_info_c* owner, unsigned index, J3DModelData* model) noexcept;
void created(J3DModelData* model) noexcept;
void erase(dRes_info_c* owner) noexcept;
void reset() noexcept;
void initialize_draw_probe();
void shutdown_draw_probe();
void request_draw_sample() noexcept;
}
