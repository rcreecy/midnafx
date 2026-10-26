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

void be16(Bytes& bytes, std::size_t at, std::uint16_t value) {
    bytes[at] = std::byte(value >> 8);
    bytes[at + 1] = std::byte(value & 0xff);
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

Bytes draw_fixture() {
    Bytes bytes(272);
    std::memcpy(bytes.data(), "J3D2bmd3", 8);
    be32(bytes, 8, static_cast<std::uint32_t>(bytes.size()));
    be32(bytes, 12, 2);
    std::memcpy(bytes.data() + 32, "SHP1", 4);
    be32(bytes, 36, 224);
    be16(bytes, 40, 2);
    be32(bytes, 44, 48);
    be32(bytes, 48, 128);
    be32(bytes, 64, 144);
    be32(bytes, 72, 176);
    be16(bytes, 82, 1);
    be16(bytes, 88, 2);
    be16(bytes, 122, 2);
    be16(bytes, 128, 0);
    be16(bytes, 160, 1);
    be16(bytes, 162, 0);
    for (unsigned i = 0; i < 24; ++i)
        bytes[176 + i] = std::byte(0x80 + i);
    be32(bytes, 208, 4);
    be32(bytes, 212, 0);
    be32(bytes, 216, 6);
    be32(bytes, 220, 8);
    be32(bytes, 224, 3);
    be32(bytes, 228, 16);
    std::memcpy(bytes.data() + 256, "TEST", 4);
    be32(bytes, 260, 16);
    return bytes;
}

Bytes attributes_fixture() {
    auto bytes = draw_fixture();
    bytes.resize(368);
    be32(bytes, 8, static_cast<std::uint32_t>(bytes.size()));
    be32(bytes, 36, 320);
    be32(bytes, 56, 200);
    be16(bytes, 124, 0);
    be16(bytes, 84, 40);
    const std::array<std::uint32_t, 10> first{0, 1, 9, 3, 10, 3, 13, 3, 255, 0};
    std::memcpy(bytes.data() + 232, first.data(), sizeof(first));
    const std::array<std::uint32_t, 6> second{9, 3, 10, 3, 255, 0};
    std::memcpy(bytes.data() + 272, second.data(), sizeof(second));
    std::memcpy(bytes.data() + 352, "TEST", 4);
    be32(bytes, 356, 16);
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

    using midnafx::character_identity::source_draw;
    const auto draws = draw_fixture();
    const auto draws_copy = draws;
    const auto first = source_draw(draws, 0, 0);
    CHECK(first.size() == 4 && first.data() == reinterpret_cast<const std::uint8_t*>(draws.data() + 176));
    CHECK(first.front() == 0x80 && first.back() == 0x83);
    const auto second = source_draw(draws, 0, 1);
    CHECK(second.size() == 6 && second.front() == 0x88 && second.back() == 0x8d);
    const auto remapped = source_draw(draws, 1, 0);
    CHECK(remapped.size() == 3 && remapped.front() == 0x90 && remapped.back() == 0x92);
    CHECK(draws == draws_copy);
    CHECK(source_draw(draws, 2, 0).empty());
    CHECK(source_draw(draws, 0, 2).empty());
    CHECK(source_draw(draws, 1, 1).empty());
    CHECK(source_draw({}, 0, 0).empty());
    CHECK(source_draw(std::span(draws).first(31), 0, 0).empty());

    const auto rejects_draw32 = [&](std::size_t at, std::uint32_t value) {
        auto changed = draws;
        be32(changed, at, value);
        return source_draw(changed, 0, 0).empty();
    };
    CHECK(rejects_draw32(8, 271));
    CHECK(rejects_draw32(8, 273));
    CHECK(rejects_draw32(12, 0));
    CHECK(rejects_draw32(12, 3));
    CHECK(rejects_draw32(12, 0xffffffff));
    CHECK(rejects_draw32(36, 7));
    CHECK(rejects_draw32(36, 0xffffffff));
    CHECK(rejects_draw32(260, 17));
    for (const auto field : {44, 48, 64, 72}) {
        CHECK(rejects_draw32(field, 0));
        CHECK(rejects_draw32(field, 43));
        CHECK(rejects_draw32(field, 0xffffffff));
    }
    CHECK(rejects_draw32(44, 184)); // Selected remapped init does not fit.
    CHECK(rejects_draw32(48, 222)); // The complete two-entry remap does not fit.
    CHECK(rejects_draw32(72, 217)); // Truncated draw-table record.
    CHECK(rejects_draw32(208, 0));
    CHECK(rejects_draw32(208, 0xffffffff));
    CHECK(rejects_draw32(212, 0xffffffff));
    CHECK(rejects_draw32(212, 224));

    const auto rejects_draw16 = [&](std::size_t at, std::uint16_t value) {
        auto changed = draws;
        be16(changed, at, value);
        return source_draw(changed, 0, 0).empty();
    };
    CHECK(rejects_draw16(40, 0));
    CHECK(rejects_draw16(40, 0xffff));
    CHECK(rejects_draw16(160, 2));
    CHECK(rejects_draw16(160, 0xffff));
    CHECK(rejects_draw16(122, 0));
    CHECK(rejects_draw16(122, 0xffff));
    CHECK(rejects_draw16(128, 0xffff));

    invalid = draws;
    std::memcpy(invalid.data() + 32, "TEST", 4);
    CHECK(source_draw(invalid, 0, 0).empty());
    invalid = draws;
    invalid.resize(32 + 224 * 2);
    std::memcpy(invalid.data() + 256, draws.data() + 32, 224);
    be32(invalid, 8, static_cast<std::uint32_t>(invalid.size()));
    CHECK(source_draw(invalid, 0, 0).empty());
    invalid = draws;
    invalid.resize(75);
    be32(invalid, 8, 75);
    be32(invalid, 12, 1);
    be32(invalid, 36, 43);
    CHECK(source_draw(invalid, 0, 0).empty());
    invalid = draws;
    invalid.resize(512 * 1024);
    be32(invalid, 8, static_cast<std::uint32_t>(invalid.size()));
    be32(invalid, 260, static_cast<std::uint32_t>(invalid.size() - 256));
    CHECK(source_draw(invalid, 0, 0).size() == 4);
    invalid.push_back(std::byte{});
    be32(invalid, 8, static_cast<std::uint32_t>(invalid.size()));
    be32(invalid, 260, static_cast<std::uint32_t>(invalid.size() - 256));
    CHECK(source_draw(invalid, 0, 0).empty());

    using midnafx::character_identity::source_attributes;
    const auto descriptors = attributes_fixture();
    const auto descriptors_copy = descriptors;
    const auto first_attributes = source_attributes(descriptors, 0);
    const std::array<std::uint32_t, 8> expected_first{0, 1, 9, 3, 10, 3, 13, 3};
    CHECK(first_attributes.size() == sizeof(expected_first));
    CHECK(first_attributes.data() == descriptors.data() + 232);
    CHECK(std::memcmp(first_attributes.data(), expected_first.data(), sizeof(expected_first)) == 0);
    const auto second_attributes = source_attributes(descriptors, 1);
    const std::array<std::uint32_t, 4> expected_second{9, 3, 10, 3};
    CHECK(second_attributes.size() == sizeof(expected_second));
    CHECK(second_attributes.data() == descriptors.data() + 272);
    CHECK(std::memcmp(second_attributes.data(), expected_second.data(), sizeof(expected_second)) == 0);
    CHECK(descriptors == descriptors_copy);
    CHECK(source_attributes(descriptors, 2).empty());
    CHECK(source_attributes({}, 0).empty());
    CHECK(source_attributes(std::span(descriptors).first(31), 0).empty());
    CHECK(source_draw(descriptors, 0, 0).size() == 4);

    for (const std::uint32_t attribute : {21u, 0xffffffffu, 9u}) {
        invalid = descriptors;
        native32(invalid, 248, attribute);
        CHECK(source_attributes(invalid, 0).empty());
    }
    for (const std::uint32_t kind : {0u, 4u, 0xffffffffu}) {
        invalid = descriptors;
        native32(invalid, 236, kind);
        CHECK(source_attributes(invalid, 0).empty());
    }
    invalid = descriptors;
    native32(invalid, 260, 2);
    CHECK(source_attributes(invalid, 0).size() == sizeof(expected_first));
    invalid = descriptors;
    std::reverse(invalid.begin() + 236, invalid.begin() + 240);
    CHECK(source_attributes(invalid, 0).empty()); // Wrong native-record endian.
    for (const std::uint32_t base : {0u, 43u, 313u, 320u, 0xffffffffu}) {
        invalid = descriptors;
        be32(invalid, 56, base);
        CHECK(source_attributes(invalid, 0).empty());
    }
    for (const std::uint16_t offset : {113u, 121u, 0xffffu}) {
        invalid = descriptors;
        be16(invalid, 124, offset);
        CHECK(source_attributes(invalid, 0).empty());
    }
    invalid = descriptors;
    be16(invalid, 160, 2);
    CHECK(source_attributes(invalid, 0).empty());
    invalid = descriptors;
    std::memcpy(invalid.data() + 32, "TEST", 4);
    CHECK(source_attributes(invalid, 0).empty());
    invalid = descriptors;
    be32(invalid, 8, 367);
    CHECK(source_attributes(invalid, 0).empty());

    // Preserve all supported unique IDs, then reject exhaustion without sentinel.
    invalid = descriptors;
    invalid.resize(32 + 200 + 22 * 8);
    be32(invalid, 8, static_cast<std::uint32_t>(invalid.size()));
    be32(invalid, 12, 1);
    be32(invalid, 36, static_cast<std::uint32_t>(invalid.size() - 32));
    for (std::uint32_t i = 0; i <= 20; ++i) {
        native32(invalid, 232 + i * 8, i);
        native32(invalid, 236 + i * 8, 3);
    }
    native32(invalid, 232 + 21 * 8, 255);
    native32(invalid, 236 + 21 * 8, 0);
    CHECK(source_attributes(invalid, 0).size() == 21 * 8);
    invalid.resize(invalid.size() - 8);
    be32(invalid, 8, static_cast<std::uint32_t>(invalid.size()));
    be32(invalid, 36, static_cast<std::uint32_t>(invalid.size() - 32));
    CHECK(source_attributes(invalid, 0).empty());
    invalid = descriptors;
    invalid.resize(271); // Four records fit; only seven terminal bytes remain.
    be32(invalid, 8, 271);
    be32(invalid, 12, 1);
    be32(invalid, 36, 239);
    CHECK(source_attributes(invalid, 0).empty());
}
