#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace midnafx::bmd_rebuild {

struct Evidence {
    std::uint32_t shapes = 0;
    std::uint32_t matrix_groups = 0;
    std::uint32_t triangles = 0;
    std::uint32_t degenerate_triangles = 0;
    std::uint32_t original_normals = 0;
    std::uint32_t written_normals = 0;
    std::uint32_t rebuilt_normal_capacity = 0;
    std::uint32_t source_draws = 0;
    std::uint32_t rebuilt_draws = 0;
    std::uint64_t original_corner_hash = 0;
    std::uint64_t rebuilt_corner_hash = 0;
};

struct Result {
    std::vector<std::byte> bytes;
    Evidence evidence;
    std::string error;

    bool ok() const { return error.empty(); }
};

// Builds a validated identity split: every surviving triangle corner gets a
// unique GX_INDEX16 normal entry containing its original normal bytes. No input
// bytes are changed. Unsupported layouts and malformed resources fail closed.
Result split_normals(std::span<const std::byte> input);

} // namespace midnafx::bmd_rebuild
