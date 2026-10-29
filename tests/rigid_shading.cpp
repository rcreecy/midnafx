#include "game/rigid_shading_state.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <span>
#include <vector>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "Rigid shading check failed at line %d: %s\n", __LINE__, #condition); \
            return __LINE__;                                                                       \
        }                                                                                          \
    } while (false)

int main() {
    using namespace midnafx::rigid_shading;
    static_assert(rejection(Layout{}) == nullptr);
    const Layout allowed;
    CHECK(rejection(allowed) == nullptr);
    for (unsigned count : {1u, 2u, 255u, 256u}) {
        auto layout = allowed; layout.shapes = layout.materials = count;
        CHECK(rejection(layout) == nullptr);
    }
    for (unsigned value : {1u, 2u, std::numeric_limits<unsigned>::max()}) {
        auto layout = allowed; layout.envelopes = value;
        CHECK(rejection(layout) != nullptr);
        layout = allowed; layout.draw_flag = value;
        CHECK(rejection(layout) != nullptr);
        layout = allowed; layout.draw_index = value;
        CHECK(rejection(layout) != nullptr);
    }
    for (unsigned value : {0u, 2u, std::numeric_limits<unsigned>::max()}) {
        auto layout = allowed; layout.joints = value;
        CHECK(rejection(layout) != nullptr);
        layout = allowed; layout.draw_matrices = value;
        CHECK(rejection(layout) != nullptr);
    }
    for (unsigned value : {0u, 257u, std::numeric_limits<unsigned>::max()}) {
        auto layout = allowed; layout.shapes = value;
        CHECK(rejection(layout) != nullptr);
        layout = allowed; layout.materials = value;
        CHECK(rejection(layout) != nullptr);
    }
    auto layout = allowed; layout.opaque = false;
    CHECK(rejection(layout) != nullptr);
    layout = allowed; layout.billboard = true;
    CHECK(rejection(layout) != nullptr);
    layout = allowed; layout.bump = true;
    CHECK(rejection(layout) != nullptr);
    layout = allowed; layout.deformed = true;
    CHECK(rejection(layout) != nullptr);

    static_assert(noexcept(aliases_source(nullptr, nullptr, 0, nullptr, nullptr, 0)));
    const int raw_a = 0, raw_b = 0;
    std::array<std::byte, 64> range{};
    std::array<std::byte, 16> separate{};
    CHECK(aliases_source(&raw_a, range.data(), range.size(), &raw_a, separate.data(), separate.size()));
    CHECK(aliases_source(&raw_a, nullptr, 0, &raw_a, nullptr, 0));
    CHECK(aliases_source(&raw_a, range.data(), 12, &raw_b, range.data(), 12));
    CHECK(aliases_source(nullptr, range.data(), 12, nullptr, range.data(), 12));
    CHECK(aliases_source(&raw_a, range.data(), 12, &raw_b, range.data() + 11, 12));
    CHECK(aliases_source(&raw_b, range.data() + 11, 12, &raw_a, range.data(), 12));
    CHECK(aliases_source(&raw_a, range.data(), 32, &raw_b, range.data() + 8, 4));
    CHECK(!aliases_source(&raw_a, range.data(), 12, &raw_b, range.data() + 12, 12));
    CHECK(!aliases_source(&raw_b, range.data() + 12, 12, &raw_a, range.data(), 12));
    CHECK(!aliases_source(&raw_a, range.data(), 12, &raw_b, range.data() + 24, 12));
    CHECK(!aliases_source(nullptr, nullptr, 0, nullptr, nullptr, 0));
    CHECK(!aliases_source(&raw_a, range.data(), 0, &raw_b, range.data(), 12));
    CHECK(!aliases_source(&raw_a, range.data(), 12, &raw_b, range.data(), 0));
    CHECK(!aliases_source(&raw_a, nullptr, 12, &raw_b, range.data(), 12));
    CHECK(!aliases_source(&raw_a, range.data(), 12, &raw_b, nullptr, 12));
    const auto maximum_address = std::numeric_limits<std::uintptr_t>::max();
    const auto near_end = reinterpret_cast<const void*>(maximum_address - 3);
    CHECK(aliases_source(&raw_a, near_end, 8, &raw_b, range.data(), 12));
    CHECK(aliases_source(&raw_a, range.data(), 12, &raw_b, near_end, 8));
    CHECK(!aliases_source(&raw_a, near_end, 3, &raw_b, range.data(), 12));
    CHECK(!aliases_source(&raw_a, near_end, 8, &raw_b, nullptr, 12));
    CHECK(!aliases_source(&raw_a, near_end, 8, &raw_b, range.data(), 0));

    for (std::size_t stride : {6u, 12u}) {
        constexpr std::size_t records = 4, guard = 8;
        const std::size_t size = stride * records;
        std::vector<std::byte> original(size), applied(size);
        for (std::size_t i = 0; i < size; ++i) {
            original[i] = std::byte((i * 17 + 3) & 255);
            applied[i] = std::byte((i * 17 + 91) & 255);
        }
        const auto original_copy = original, applied_copy = applied;
        std::vector<std::byte> storage(size + guard * 2, std::byte{0xa5});
        auto destination = std::span<std::byte>(storage).subspan(guard, size);
        std::copy(applied.begin(), applied.end(), destination.begin());
        const auto full = restore_owned(destination, original, applied, stride);
        CHECK(full.valid && full.restored == records && full.preserved == 0);
        CHECK(std::equal(destination.begin(), destination.end(), original.begin()));
        CHECK(std::all_of(storage.begin(), storage.begin() + guard, [](std::byte b) { return b == std::byte{0xa5}; }));
        CHECK(std::all_of(storage.end() - guard, storage.end(), [](std::byte b) { return b == std::byte{0xa5}; }));
        CHECK(original == original_copy && applied == applied_copy);

        std::copy(applied.begin(), applied.end(), destination.begin());
        destination[stride * 2 - 1] ^= std::byte{0xff}; // One changed byte protects the entire second normal.
        std::copy(original.begin() + stride * 2, original.begin() + stride * 3, destination.begin() + stride * 2);
        const std::vector<std::byte> mixed_before(destination.begin(), destination.end());
        auto mixed_expected = mixed_before;
        std::copy(original.begin(), original.begin() + stride, mixed_expected.begin());
        std::copy(original.begin() + stride * 3, original.end(), mixed_expected.begin() + stride * 3);
        const auto mixed = restore_owned(destination, original, applied, stride);
        CHECK(mixed.valid && mixed.restored == 2 && mixed.preserved == 2);
        CHECK(std::equal(destination.begin(), destination.end(), mixed_expected.begin()));
        const auto repeated = restore_owned(destination, original, applied, stride);
        CHECK(repeated.valid && repeated.restored == 0 && repeated.preserved == records);
        CHECK(std::equal(destination.begin(), destination.end(), mixed_expected.begin()));

        std::copy(applied.begin(), applied.end(), destination.begin());
        for (std::size_t record = 0; record < records; ++record)
            destination[record * stride + record] ^= std::byte{0x40};
        const auto conflicted = storage;
        const auto conflict = restore_owned(destination, original, applied, stride);
        CHECK(conflict.valid && conflict.restored == 0 && conflict.preserved == records);
        CHECK(storage == conflicted);
        CHECK(original == original_copy && applied == applied_copy);

        std::copy(applied.begin(), applied.end(), destination.begin());
        const auto unchanged = storage;
        const auto invalid = [&](std::span<std::byte> dest, std::span<const std::byte> before,
                                 std::span<const std::byte> after, std::size_t record_stride) {
            const auto result = restore_owned(dest, before, after, record_stride);
            return !result.valid && result.restored == 0 && result.preserved == 0 && storage == unchanged;
        };
        CHECK(invalid(destination, std::span<const std::byte>(original).first(size - 1), applied, stride));
        CHECK(invalid(destination, original, std::span<const std::byte>(applied).first(size - 1), stride));
        CHECK(invalid(destination.first(size - 1), original, applied, stride));
        CHECK(invalid(destination.first(size - 1), std::span<const std::byte>(original).first(size - 1),
                      std::span<const std::byte>(applied).first(size - 1), stride));
        for (std::size_t bad_stride : {std::size_t{0}, std::size_t{1}, std::size_t{4}, std::size_t{8},
                                      std::size_t{24}, std::numeric_limits<std::size_t>::max()})
            CHECK(invalid(destination, original, applied, bad_stride));
        const auto same = restore_owned(destination, applied, applied, stride);
        CHECK(same.valid && same.restored == records && same.preserved == 0 && storage == unchanged);
    }
    for (std::size_t stride : {6u, 12u}) {
        const auto empty = restore_owned({}, {}, {}, stride);
        CHECK(empty.valid && empty.restored == 0 && empty.preserved == 0);
    }
    CHECK(!restore_owned({}, {}, {}, 0).valid);
    CHECK(!restore_owned({}, {}, {}, 8).valid);
    std::puts("Rigid shading layout policy, source aliases, exact normal restoration, conflicts, and bounds: PASS");
}
