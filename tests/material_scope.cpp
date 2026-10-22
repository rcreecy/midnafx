#include "game/material_scope.hpp"
#include "game/material_identity.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace {
using midnafx::material_scope::CommandBuffer;

void check(bool condition) {
    if (!condition)
        throw std::runtime_error("material scope test failed");
}

std::uint32_t word(const CommandBuffer& command, std::size_t offset) {
    const auto& bytes = command.bytes;
    return (std::uint32_t(bytes[offset]) << 24) |
           (std::uint32_t(bytes[offset + 1]) << 16) |
           (std::uint32_t(bytes[offset + 2]) << 8) | bytes[offset + 3];
}

void check_encoding(const CommandBuffer& command, std::uint32_t expected_write) {
    check(command.bytes[0] == 0x61 && command.bytes[5] == 0x61);
    check(word(command, 1) == 0xfe300000);
    check(word(command, 6) == expected_write);
    check((word(command, 6) >> 24) == 0xc0);
    check((word(command, 6) & ~0x00300000u) == 0xc0000000);
    for (std::size_t i = 10; i < command.bytes.size(); ++i)
        check(command.bytes[i] == 0); // GX NOP padding, not another register write.
    check(reinterpret_cast<std::uintptr_t>(&command) % 32 == 0);
}

using Registers = std::array<std::uint32_t, 256>;

// A small independent BP interpreter models the one-write mask semantics.
void execute(const CommandBuffer& command, Registers& registers) {
    std::uint32_t mask = 0x00ffffff;
    for (std::size_t at = 0; at < command.bytes.size();) {
        const auto opcode = command.bytes[at++];
        if (opcode == 0)
            continue;
        check(opcode == 0x61 && at + 4 <= command.bytes.size());
        const auto value = word(command, at);
        at += 4;
        const auto address = value >> 24;
        if (address == 0xfe) {
            mask = value & 0x00ffffff;
        } else {
            registers[address] = (registers[address] & ~mask) | (value & mask);
            mask = 0x00ffffff;
        }
    }
    check(mask == 0x00ffffff);
}
} // namespace

int main() {
    using namespace midnafx::material_scope;
    // Independent fixed digest for 17,824 zero bytes with just the twelve native
    // padding bytes canonicalized. Every semantic byte must still affect identity.
    std::array<unsigned char, 17824> source{};
    constexpr std::uint64_t normalized = 0x3da090435a3d8425ULL;
    check(midnafx::material_identity::source_hash(source, true) == normalized);
    constexpr std::array<std::size_t, 12> padding{0xcd, 0xce, 0xcf, 0xdd, 0xde, 0xdf,
                                                0xed, 0xee, 0xef, 0xfd, 0xfe, 0xff};
    for (std::size_t byte = 0; byte < source.size(); ++byte) {
        source[byte] = 0x7f;
        bool ignored = false;
        for (auto offset : padding) if (byte == offset) ignored = true;
        check((midnafx::material_identity::source_hash(source, true) == normalized) == ignored);
        source[byte] = 0;
    }
    const auto original_hash = midnafx::material_identity::source_hash(source, false);
    source[0xcd] = 0x7f;
    check(midnafx::material_identity::source_hash(source, false) != original_hash);
    check(qualifies_stage(1, 0x28, 0xf8, 0xaf));
    for (unsigned byte = 0; byte < 256; ++byte) {
        check(qualifies_stage(1, static_cast<std::uint8_t>(byte), 0xf8, 0xaf) ==
              (byte == 0x28));
        check(qualifies_stage(1, 0x28, static_cast<std::uint8_t>(byte), 0xaf) ==
              (byte == 0xf8));
        check(qualifies_stage(1, 0x28, 0xf8, static_cast<std::uint8_t>(byte)) ==
              (byte == 0xaf));
    }
    for (unsigned count = 0; count <= 17; ++count)
        check(qualifies_stage(count, 0x28, 0xf8, 0xaf) == (count == 1));
    check(!qualifies_stage(std::numeric_limits<unsigned>::max(), 0x28, 0xf8, 0xaf));

    check_encoding(apply, 0xc0100000);
    check_encoding(restore, 0xc0200000);

    // Vary every non-scale RGB bit independently, then vary combinations. The
    // current qualified scale is always four; no arbitrary-scale restore claim.
    for (unsigned trial = 0; trial < 280; ++trial) {
        Registers original{};
        for (unsigned address = 0; address < original.size(); ++address)
            original[address] = (address * 0x13579u + trial * 0x2468bu) & 0x00ffffff;
        const auto other_bits = trial < 24 ? (1u << trial) : (trial * 0x5a31c7u);
        original[0xc0] = (other_bits & 0x00cfffffu) | 0x00200000;
        auto registers = original;
        execute(apply, registers);
        check(registers[0xc0] == ((original[0xc0] & ~0x00300000u) | 0x00100000));
        check(((registers[0xc0] ^ original[0xc0]) & ~0x00300000u) == 0);
        check(registers[0xc1] == original[0xc1]);
        for (unsigned address = 0; address < registers.size(); ++address)
            if (address != 0xc0)
                check(registers[address] == original[address]);
        execute(restore, registers);
        check(registers == original);
    }
}
