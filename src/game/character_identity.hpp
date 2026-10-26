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

namespace detail {
inline std::uint32_t be32(std::span<const std::byte> bytes, std::size_t at) noexcept {
    std::uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i)
        value = (value << 8) | std::to_integer<unsigned char>(bytes[at + i]);
    return value;
}
inline std::uint32_t be16(std::span<const std::byte> bytes, std::size_t at) noexcept {
    return (std::uint32_t(std::to_integer<unsigned char>(bytes[at])) << 8) |
           std::to_integer<unsigned char>(bytes[at + 1]);
}
inline bool fits(std::span<const std::byte> block, std::size_t offset, std::size_t size) noexcept {
    return offset >= 44 && offset <= block.size() && size <= block.size() - offset;
}
struct SourceShape {
    std::span<const std::byte> block;
    std::size_t init = 0;
};
inline SourceShape source_shape(std::span<const std::byte> bytes, unsigned shape) noexcept {
    if (bytes.size() < 32 || bytes.size() > 512 * 1024 ||
        std::memcmp(bytes.data(), "J3D2bmd3", 8) != 0)
        return {};
    const auto count = be32(bytes, 12);
    if (be32(bytes, 8) != bytes.size() || !count || count > (bytes.size() - 32) / 8)
        return {};
    std::size_t at = 32, block = 0, block_size = 0;
    for (std::uint32_t i = 0; i < count; ++i) {
        if (at > bytes.size() - 8)
            return {};
        const auto size = be32(bytes, at + 4);
        if (size < 8 || size > bytes.size() - at)
            return {};
        if (std::memcmp(bytes.data() + at, "SHP1", 4) == 0) {
            if (block || size < 44)
                return {};
            block = at;
            block_size = size;
        }
        at += size;
    }
    if (at != bytes.size() || !block)
        return {};
    const auto view = bytes.subspan(block, block_size);
    const auto shape_count = be16(view, 8);
    const auto init_offset = be32(view, 12);
    const auto remap_offset = be32(view, 16);
    if (shape >= shape_count || !fits(view, remap_offset, std::size_t(shape_count) * 2) ||
        !fits(view, init_offset, 40))
        return {};
    const auto remapped = be16(view, remap_offset + std::size_t(shape) * 2);
    if (remapped >= shape_count || std::size_t(remapped) > (block_size - init_offset) / 40)
        return {};
    const std::size_t init = init_offset + std::size_t(remapped) * 40;
    if (!fits(view, init, 40))
        return {};
    return {view, init};
}
} // namespace detail

// Locate an original SHP1 draw without using the mutable optimized native copy.
// The caller must qualify resource identity before using the returned bytes.
inline std::span<const std::uint8_t> source_draw(std::span<const std::byte> bytes,
                                               unsigned shape, unsigned group) noexcept {
    const auto source = detail::source_shape(bytes, shape);
    const auto block = source.block;
    if (block.empty())
        return {};
    const auto draw_base = detail::be32(block, 32);
    const auto table_offset = detail::be32(block, 40);
    if (!detail::fits(block, draw_base, 0) || !detail::fits(block, table_offset, 8))
        return {};
    const auto groups = detail::be16(block, source.init + 2);
    const auto first_draw = detail::be16(block, source.init + 8);
    const auto table_entries = (block.size() - table_offset) / 8;
    if (group >= groups || first_draw > table_entries || groups > table_entries - first_draw)
        return {};
    const std::size_t record = table_offset + (std::size_t(first_draw) + group) * 8;
    const auto size = detail::be32(block, record);
    const auto offset = detail::be32(block, record + 4);
    if (!size || offset > block.size() - draw_base || size > block.size() - draw_base - offset)
        return {};
    return {reinterpret_cast<const std::uint8_t*>(block.data() + draw_base + offset), size};
}

// Native loading converts the original SHP1 descriptors to host endian. Runtime
// shapes may instead point to augmented copies; pair these original records with
// source_draw(). Return the eight-byte records without their terminating entry.
inline std::span<const std::byte> source_attributes(std::span<const std::byte> bytes,
                                                   unsigned shape) noexcept {
    const auto source = detail::source_shape(bytes, shape);
    const auto block = source.block;
    if (block.empty())
        return {};
    const auto base = detail::be32(block, 24);
    const auto offset = detail::be16(block, source.init + 4);
    if (!detail::fits(block, base, 0) || offset > block.size() - base)
        return {};
    const std::size_t begin = base + offset;
    std::array<bool, 21> seen{};
    for (unsigned i = 0; i < 32; ++i) {
        const std::size_t at = begin + i * 8;
        if (!detail::fits(block, at, 8))
            return {};
        std::array<std::uint32_t, 2> record{};
        std::memcpy(record.data(), block.data() + at, sizeof(record));
        const auto attribute = record[0], kind = record[1];
        if (attribute == 255)
            return block.subspan(begin, at - begin);
        if (attribute > 20 || seen[attribute] || kind < 1 || kind > 3)
            return {};
        seen[attribute] = true;
    }
    return {};
}

} // namespace midnafx::character_identity
