#include "game/character_identity.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

#define CHECK(condition) if (!(condition)) return __LINE__

namespace {
using Bytes = std::vector<std::byte>;

void be32(Bytes& bytes, std::size_t at, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        bytes[at + i] = std::byte((value >> ((3 - i) * 8)) & 0xff);
}

void native32(Bytes& bytes, std::size_t at, std::uint32_t value) {
    std::memcpy(bytes.data() + at, &value, sizeof(value));
}

Bytes fixture() {
    Bytes bytes(176);
    std::memcpy(bytes.data(), "J3D2bmd3", 8);
    be32(bytes, 8, static_cast<std::uint32_t>(bytes.size()));
    be32(bytes, 12, 2);
    std::memcpy(bytes.data() + 32, "VTX1", 4);
    be32(bytes, 36, 128);
    be32(bytes, 40, 64);
    native32(bytes, 96, 9);
    native32(bytes, 100, 1);
    native32(bytes, 104, 4);
    native32(bytes, 112, 10);
    native32(bytes, 116, 0);
    native32(bytes, 120, 4);
    bytes[124] = std::byte{15};
    native32(bytes, 128, 255);
    for (std::size_t at : {96, 112, 128}) {
        bytes[at + 13] = std::byte{0x11};
        bytes[at + 14] = std::byte{0x22};
        bytes[at + 15] = std::byte{0x33};
    }
    std::memcpy(bytes.data() + 160, "TEST", 4);
    be32(bytes, 164, 16);
    return bytes;
}

std::uint64_t fnv(std::span<const std::byte> bytes) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const auto byte : bytes) {
        hash ^= std::to_integer<unsigned char>(byte);
        hash *= 1099511628211ULL;
    }
    return hash;
}
} // namespace

int main() {
    using midnafx::character_identity::normalized_loaded_hash;
    const auto original = fixture();
    const auto original_copy = original;
    const std::array<std::size_t, 9> padding{109, 110, 111, 125, 126, 127, 141, 142, 143};
    auto canonical = original;
    for (const auto at : padding)
        canonical[at] = std::byte{0xff};
    const auto expected = fnv(canonical);
    CHECK(expected != 0);
    CHECK(normalized_loaded_hash(original) == expected);
    CHECK(normalized_loaded_hash(canonical) == expected);
    CHECK(original == original_copy);

    // Independently enumerate the exact nine ignored bytes, including the
    // terminal record. Every other byte must affect identity or reject input.
    for (std::size_t at = 0; at < original.size(); ++at) {
        auto changed = original;
        changed[at] ^= std::byte{0x01};
        const auto hash = normalized_loaded_hash(changed);
        if (std::find(padding.begin(), padding.end(), at) != padding.end()) {
            CHECK(hash == expected);
        } else {
            CHECK(hash != expected);
        }
    }
    auto changed_padding = original;
    for (std::size_t i = 0; i < padding.size(); ++i)
        changed_padding[padding[i]] = std::byte(static_cast<unsigned char>(i * 29));
    CHECK(normalized_loaded_hash(changed_padding) == expected);
    CHECK(normalized_loaded_hash({}) == 0);
    CHECK(normalized_loaded_hash(std::span(original).first(31)) == 0);
    auto largest = original;
    largest.resize(512 * 1024);
    be32(largest, 8, static_cast<std::uint32_t>(largest.size()));
    be32(largest, 164, static_cast<std::uint32_t>(largest.size() - 160));
    CHECK(normalized_loaded_hash(largest) != 0);
    largest.push_back(std::byte{});
    be32(largest, 8, static_cast<std::uint32_t>(largest.size()));
    be32(largest, 164, static_cast<std::uint32_t>(largest.size() - 160));
    CHECK(normalized_loaded_hash(largest) == 0);

    const auto rejects_be32 = [&](std::size_t at, std::uint32_t value) {
        auto changed = original;
        be32(changed, at, value);
        return normalized_loaded_hash(changed) == 0;
    };
    CHECK(rejects_be32(8, 175));
    CHECK(rejects_be32(8, 177));
    CHECK(rejects_be32(12, 0));
    CHECK(rejects_be32(12, 3));
    CHECK(rejects_be32(12, 0xffffffff));
    CHECK(rejects_be32(36, 0));
    CHECK(rejects_be32(36, 7));
    CHECK(rejects_be32(36, 0xffffffff));
    CHECK(rejects_be32(164, 24));
    CHECK(rejects_be32(40, 63));
    CHECK(rejects_be32(40, 121));
    CHECK(rejects_be32(40, 0xffffffff));

    auto invalid = original;
    invalid[0] = std::byte{'X'};
    CHECK(normalized_loaded_hash(invalid) == 0);
    invalid = original;
    invalid[7] = std::byte{'2'};
    CHECK(normalized_loaded_hash(invalid) == 0);
    invalid = original;
    std::memcpy(invalid.data() + 32, "TEST", 4);
    CHECK(normalized_loaded_hash(invalid) == 0);
    invalid = original;
    invalid.resize(240);
    be32(invalid, 8, 240);
    std::memcpy(invalid.data() + 160, "VTX1", 4);
    be32(invalid, 164, 80);
    be32(invalid, 168, 64);
    native32(invalid, 224, 255);
    CHECK(normalized_loaded_hash(invalid) == 0);
    for (const std::uint32_t attribute : {8u, 21u, 0xffffffffu}) {
        invalid = original;
        native32(invalid, 96, attribute);
        CHECK(normalized_loaded_hash(invalid) == 0);
    }
    invalid = original;
    native32(invalid, 112, 9);
    CHECK(normalized_loaded_hash(invalid) == 0);

    // A valid-looking continuation must not escape VTX1 into another block.
    invalid = original;
    native32(invalid, 128, 11);
    native32(invalid, 144, 12);
    CHECK(normalized_loaded_hash(invalid) == 0);
    invalid = original;
    invalid.resize(143);
    be32(invalid, 8, 143);
    be32(invalid, 12, 1);
    be32(invalid, 36, 111);
    CHECK(normalized_loaded_hash(invalid) == 0);

    // All twelve supported attribute IDs are valid and unique; an unterminated
    // full set is still invalid. These fixtures contain only synthetic bytes.
    Bytes all_attributes(32 + 64 + 13 * 16);
    std::memcpy(all_attributes.data(), "J3D2bmd3", 8);
    be32(all_attributes, 8, static_cast<std::uint32_t>(all_attributes.size()));
    be32(all_attributes, 12, 1);
    std::memcpy(all_attributes.data() + 32, "VTX1", 4);
    be32(all_attributes, 36, static_cast<std::uint32_t>(all_attributes.size() - 32));
    be32(all_attributes, 40, 64);
    for (std::uint32_t i = 0; i < 12; ++i)
        native32(all_attributes, 96 + i * 16, 9 + i);
    native32(all_attributes, 96 + 12 * 16, 255);
    CHECK(normalized_loaded_hash(all_attributes) != 0);
    all_attributes.resize(all_attributes.size() - 16);
    be32(all_attributes, 8, static_cast<std::uint32_t>(all_attributes.size()));
    be32(all_attributes, 36, static_cast<std::uint32_t>(all_attributes.size() - 32));
    CHECK(normalized_loaded_hash(all_attributes) == 0);
}
