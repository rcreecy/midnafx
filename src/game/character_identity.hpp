#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace midnafx::character_identity {

// Native loading converts GXVtxAttrFmtList fields to host endian, leaving only
// the three implicit padding bytes unspecified. Canonicalize those bytes while
// hashing; all resource bytes remain read-only. Zero denotes an invalid layout.
inline std::uint64_t normalized_loaded_hash(std::span<const std::byte> bytes) noexcept {
    constexpr std::size_t MaximumSize = 512 * 1024;
    if (bytes.size() < 32 || bytes.size() > MaximumSize ||
        std::memcmp(bytes.data(), "J3D2bmd3", 8) != 0)
        return 0;
    const auto be32 = [&](std::size_t offset) {
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i)
            value = (value << 8) | std::to_integer<unsigned char>(bytes[offset + i]);
        return value;
    };
    const auto block_count = be32(12);
    if (be32(8) != bytes.size() || !block_count || block_count > (bytes.size() - 32) / 8)
        return 0;

    std::size_t offset = 32, vertex = 0, vertex_size = 0;
    for (std::uint32_t i = 0; i < block_count; ++i) {
        if (offset > bytes.size() - 8)
            return 0;
        const auto size = be32(offset + 4);
        if (size < 8 || size > bytes.size() - offset)
            return 0;
        if (std::memcmp(bytes.data() + offset, "VTX1", 4) == 0) {
            if (vertex || size < 0x40)
                return 0;
            vertex = offset;
            vertex_size = size;
        }
        offset += size;
    }
    if (offset != bytes.size() || !vertex)
        return 0;

    const auto format_offset = be32(vertex + 8);
    if (format_offset < 0x40 || format_offset > vertex_size - 16)
        return 0;
    const std::size_t formats_begin = vertex + format_offset;
    const std::size_t vertex_end = vertex + vertex_size;
    std::size_t formats_end = 0;
    std::array<bool, 12> seen{};
    for (unsigned i = 0; i < 32; ++i) {
        const std::size_t record = formats_begin + i * 16;
        if (record > vertex_end - 16)
            return 0;
        std::uint32_t attribute = 0;
        std::memcpy(&attribute, bytes.data() + record, sizeof(attribute));
        if (attribute == 255) {
            formats_end = record + 16;
            break;
        }
        if (attribute < 9 || attribute > 20 || seen[attribute - 9])
            return 0;
        seen[attribute - 9] = true;
    }
    if (!formats_end)
        return 0;

    std::uint64_t hash = 14695981039346656037ULL;
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const bool padding = i >= formats_begin && i < formats_end &&
                             (i - formats_begin) % 16 >= 13;
        hash ^= padding ? 0xff : std::to_integer<unsigned char>(bytes[i]);
        hash *= 1099511628211ULL;
    }
    return hash;
}

} // namespace midnafx::character_identity
