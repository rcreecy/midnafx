#pragma once
#include <cstdint>
namespace midnafx::asset_probe {
// Pinned J3DTevBlock subclasses have different fixed texture arrays.
constexpr unsigned texture_capacity(std::uint32_t tag) {
    switch (tag) {
    case 0x54564231: return 1; // TVB1
    case 0x54564232: return 2; // TVB2
    case 0x54564234: return 4; // TVB4
    case 0x54563136: case 0x54565054: return 8; // TV16 / TVPT
    default: return 0;
    }
}
}
