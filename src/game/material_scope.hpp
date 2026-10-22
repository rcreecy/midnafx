#pragma once

#include <array>
#include <cstdint>

namespace midnafx::material_scope {

// This proof is restricted to the fingerprinted pumpkin body's single stage.
// Qualification does not establish resource identity or snapshot live GX state.
constexpr bool qualifies_stage(unsigned stage_count, std::uint8_t color_op,
                               std::uint8_t color_ab, std::uint8_t color_cd) noexcept {
    return stage_count == 1 && color_op == 0x28 && color_ab == 0xf8 && color_cd == 0xaf;
}

struct alignas(32) CommandBuffer {
    std::array<std::uint8_t, 32> bytes;
};

namespace detail {
constexpr CommandBuffer scale_command(std::uint8_t scale) noexcept {
    // BP mask 0x00300000 applies to the next BP write only. Stage zero's RGB
    // register is 0xc0; its alpha register at 0xc1 is never written here.
    return {{{0x61, 0xfe, 0x30, 0x00, 0x00,
              0x61, 0xc0, static_cast<std::uint8_t>(scale << 4), 0x00, 0x00}}};
}
} // namespace detail

// Reduce the qualified SCALE_4 operation to SCALE_2, then restore its known
// definition. These commands preserve other live register bits, but restoration
// is not a snapshot of a possibly different live scale operation.
inline constexpr CommandBuffer apply = detail::scale_command(1);
inline constexpr CommandBuffer restore = detail::scale_command(2);

static_assert(alignof(CommandBuffer) == 32 && sizeof(CommandBuffer) == 32);

} // namespace midnafx::material_scope
