#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace midnafx::material_identity {
// Exact original pumpkin layout: four GXVtxAttrFmtList records. Native endian
// conversion leaves their three padding bytes unspecified. Normalize only those
// bytes while hashing; never write to the live resource.
inline std::uint64_t source_hash(std::span<const unsigned char> bytes, bool native_loaded) noexcept {
    std::uint64_t hash = 14695981039346656037ULL;
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const bool padding = native_loaded && i >= 0xcd && i <= 0xff && (i & 0xf) >= 0xd;
        hash ^= padding ? 0xff : bytes[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}
} // namespace midnafx::material_identity
