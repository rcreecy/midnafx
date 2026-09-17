#pragma once

#include "topology.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace midnafx::smoothing {

struct Options {
    float face_angle_degrees = 55.0f;
    float original_split_degrees = 20.0f;
    float index_conflict_degrees = 1.0f;
};

struct Result {
    std::vector<topology::Vec3> normals;
    std::uint32_t positions_with_adjacency = 0;
    std::uint32_t smoothing_groups = 0;
    std::uint32_t candidate_indices = 0;
    std::uint32_t changed_indices = 0;
    std::uint32_t index_conflicts = 0;
    std::uint32_t ambiguous_faces = 0;
    const char* error = nullptr;
    bool safe() const { return error == nullptr && index_conflicts == 0; }
};

// Original index ownership is preserved. A conflicting desired direction for
// one existing index rejects the entire model; no DL index rewriting occurs.
Result plan(const topology::Result& mesh, std::span<const topology::Vec3> originals,
            Options options = {});
topology::Vec3 decode_s16_xyz(const std::int16_t values[3], unsigned fraction_bits);
bool encode_s16_xyz(topology::Vec3 normal, unsigned fraction_bits, std::int16_t values[3]);

} // namespace midnafx::smoothing
