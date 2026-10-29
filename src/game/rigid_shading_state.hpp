#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>

namespace midnafx::rigid_shading {
struct Layout {
    unsigned envelopes = 0, joints = 1, draw_matrices = 1;
    unsigned draw_flag = 0, draw_index = 0, shapes = 1, materials = 1;
    bool opaque = true, billboard = false, bump = false, deformed = false;
};

constexpr const char* rejection(Layout layout) {
    if (layout.envelopes != 0) return "Envelope matrices are unsupported";
    if (layout.joints != 1) return "Exactly one joint is required";
    if (layout.draw_matrices != 1) return "Exactly one draw matrix is required";
    if (layout.draw_flag != 0) return "Envelope draw matrices are unsupported";
    if (layout.draw_index != 0) return "The draw matrix must reference joint zero";
    if (layout.shapes < 1 || layout.shapes > 256) return "Shape count is outside 1..256";
    if (layout.materials < 1 || layout.materials > 256) return "Material count is outside 1..256";
    if (!layout.opaque) return "Translucent materials are unsupported";
    if (layout.billboard) return "Billboard transforms are unsupported";
    if (layout.bump) return "Bump mapping is unsupported";
    if (layout.deformed) return "Deformed vertex data is unsupported";
    return nullptr;
}

inline bool aliases_source(const void* raw_a, const void* normal_a, std::size_t bytes_a,
                           const void* raw_b, const void* normal_b, std::size_t bytes_b) noexcept {
    if (raw_a != nullptr && raw_a == raw_b) return true;
    if (normal_a == nullptr || normal_b == nullptr || bytes_a == 0 || bytes_b == 0) return false;
    const auto begin_a = reinterpret_cast<std::uintptr_t>(normal_a);
    const auto begin_b = reinterpret_cast<std::uintptr_t>(normal_b);
    constexpr auto maximum = std::numeric_limits<std::uintptr_t>::max();
    // An unrepresentable endpoint cannot prove separation; conservatively veto.
    if (bytes_a > maximum - begin_a || bytes_b > maximum - begin_b) return true;
    const auto end_a = begin_a + bytes_a;
    const auto end_b = begin_b + bytes_b;
    return begin_a < end_b && begin_b < end_a;
}

struct RestoreResult {
    bool valid = false;
    std::size_t restored = 0, preserved = 0;
};

inline RestoreResult restore_owned(std::span<std::byte> destination,
                                   std::span<const std::byte> original,
                                   std::span<const std::byte> applied, std::size_t stride) noexcept {
    if ((stride != 6 && stride != 12) || destination.size() != original.size() ||
        destination.size() != applied.size() || destination.size() % stride != 0)
        return {};
    RestoreResult result{true, 0, 0};
    for (std::size_t offset = 0; offset < destination.size(); offset += stride) {
        if (std::memcmp(destination.data() + offset, applied.data() + offset, stride) == 0) {
            std::memmove(destination.data() + offset, original.data() + offset, stride);
            ++result.restored;
        } else {
            ++result.preserved;
        }
    }
    return result;
}
} // namespace midnafx::rigid_shading
