#include "smoothing.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <vector>

namespace midnafx::smoothing {
namespace {
using topology::Vec3;
constexpr float Pi = 3.14159265358979323846f;

float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
float magnitude(Vec3 a) { return std::sqrt(dot(a, a)); }
Vec3 normalize(Vec3 a) {
    const float size = magnitude(a);
    return size > 1e-8f && std::isfinite(size) ? Vec3{a.x / size, a.y / size, a.z / size} : Vec3{};
}
Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 mul(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
float cosine(float degrees) { return std::cos(degrees * Pi / 180.0f); }

struct Ref {
    std::uint32_t triangle;
    std::uint8_t corner;
};
template <class T> std::uint64_t vector_bytes(const std::vector<T>& values) {
    return static_cast<std::uint64_t>(values.capacity()) * sizeof(T);
}
std::uint64_t vector_bytes(const std::vector<bool>& values) { return (values.capacity() + 7) / 8; }
template <class T> std::uint64_t nested_vector_bytes(const std::vector<std::vector<T>>& values) {
    std::uint64_t bytes = vector_bytes(values);
    for (const auto& inner : values)
        bytes += vector_bytes(inner);
    return bytes;
}
} // namespace

Vec3 decode_s16_xyz(const std::int16_t values[3], unsigned fraction_bits) {
    const float scale = std::ldexp(1.0f, -static_cast<int>(fraction_bits));
    return {values[0] * scale, values[1] * scale, values[2] * scale};
}

bool encode_s16_xyz(Vec3 normal, unsigned fraction_bits, std::int16_t values[3]) {
    if (fraction_bits > 15 || !std::isfinite(normal.x) || !std::isfinite(normal.y) ||
        !std::isfinite(normal.z))
        return false;
    const float scale = std::ldexp(1.0f, static_cast<int>(fraction_bits));
    const float components[]{normal.x, normal.y, normal.z};
    for (unsigned c = 0; c < 3; ++c)
        values[c] = static_cast<std::int16_t>(
            std::clamp(std::round(components[c] * scale), -32768.0f, 32767.0f));
    return true;
}

Result plan(const topology::Result& mesh, std::span<const Vec3> originals, Options options) {
    Result result;
    if (!mesh.ok() || originals.empty() || originals.size() > 65536 ||
        !(options.face_angle_degrees > 0 && options.face_angle_degrees <= 90) ||
        !(options.original_split_degrees > 0 && options.original_split_degrees <= 90) ||
        !(options.index_conflict_degrees > 0 && options.index_conflict_degrees <= 45) ||
        !(options.geometric_weight > 0 && options.geometric_weight <= 1)) {
        result.error = "invalid smoothing input";
        return result;
    }
    std::vector<Vec3> original_unit(originals.size());
    for (std::size_t i = 0; i < originals.size(); ++i) {
        original_unit[i] = normalize(originals[i]);
        if (!std::isfinite(original_unit[i].x) || !std::isfinite(original_unit[i].y) ||
            !std::isfinite(original_unit[i].z)) {
            result.error = "nonfinite original normal";
            return result;
        }
    }
    std::uint16_t max_position = 0;
    std::vector<Vec3> oriented_faces(mesh.triangles.size());
    for (std::size_t ti = 0; ti < mesh.triangles.size(); ++ti) {
        const auto& triangle = mesh.triangles[ti];
        float alignment = 0;
        for (const auto& corner : triangle.corners) {
            if (corner.normal >= originals.size()) {
                result.error = "normal index outside array";
                return result;
            }
            alignment += dot(triangle.face_normal, original_unit[corner.normal]);
        }
        if (!std::isfinite(alignment)) {
            result.error = "nonfinite face orientation";
            return result;
        }
        if (std::abs(alignment) < 0.01f) {
            ++result.ambiguous_faces;
            continue; // Leave these normals untouched; do not infer orientation.
        }
        oriented_faces[ti] = alignment < 0 ? Vec3{-triangle.face_normal.x, -triangle.face_normal.y,
                                                  -triangle.face_normal.z}
                                           : triangle.face_normal;
    }
    const auto adjacency_begin = std::chrono::steady_clock::now();
    for (const auto& triangle : mesh.triangles)
        for (const auto& corner : triangle.corners) {
            if (corner.normal >= originals.size()) {
                result.error = "normal index outside array";
                return result;
            }
            max_position = std::max(max_position, corner.position);
        }
    std::vector<std::vector<Ref>> by_position(std::size_t(max_position) + 1);
    for (std::uint32_t ti = 0; ti < mesh.triangles.size(); ++ti)
        for (std::uint8_t ci = 0; ci < 3; ++ci)
            by_position[mesh.triangles[ti].corners[ci].position].push_back({ti, ci});
    result.adjacency_us =
        static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                       std::chrono::steady_clock::now() - adjacency_begin)
                                       .count());

    const float face_limit = cosine(options.face_angle_degrees);
    const float split_limit = cosine(options.original_split_degrees);
    const float conflict_limit = cosine(options.index_conflict_degrees);
    std::vector<Vec3> desired(originals.size());
    std::vector<bool> assigned(originals.size());
    std::vector<bool> candidate(originals.size());
    const auto base_working_bytes = vector_bytes(original_unit) + vector_bytes(oriented_faces) +
                                    nested_vector_bytes(by_position) + vector_bytes(desired) +
                                    vector_bytes(assigned) + vector_bytes(candidate);
    result.working_vector_bytes = base_working_bytes;

    for (const auto& refs : by_position) {
        if (refs.empty())
            continue;
        if (refs.size() > 256) {
            result.error = "adjacency degree exceeds limit";
            return result;
        }
        result.positions_with_adjacency += refs.size() > 1;
        std::vector<std::vector<Ref>> groups;
        for (const Ref ref : refs) {
            const auto& tri = mesh.triangles[ref.triangle];
            if (tri.material == 0xffff) {
                result.error = "missing material association";
                return result;
            }
            const Vec3 normal = original_unit[tri.corners[ref.corner].normal];
            if (magnitude(normal) < 0.5f) {
                result.error = "zero referenced normal";
                return result;
            }
            bool placed = false;
            for (auto& group : groups) {
                bool compatible = true;
                for (const Ref other : group) {
                    const auto& ot = mesh.triangles[other.triangle];
                    if (magnitude(oriented_faces[ref.triangle]) < 0.5f ||
                        magnitude(oriented_faces[other.triangle]) < 0.5f ||
                        tri.material != ot.material ||
                        dot(oriented_faces[ref.triangle], oriented_faces[other.triangle]) <
                            face_limit ||
                        dot(normal, original_unit[ot.corners[other.corner].normal]) < split_limit) {
                        compatible = false;
                        break;
                    }
                }
                if (compatible) {
                    group.push_back(ref);
                    placed = true;
                    break;
                }
            }
            if (!placed)
                groups.push_back({ref});
        }
        result.smoothing_groups += static_cast<std::uint32_t>(groups.size());
        for (const auto& group : groups) {
            if (magnitude(oriented_faces[group.front().triangle]) < 0.5f) {
                const auto& ref = group.front();
                const auto index = mesh.triangles[ref.triangle].corners[ref.corner].normal;
                candidate[index] = true;
                if (assigned[index] && dot(desired[index], original_unit[index]) < conflict_limit)
                    ++result.index_conflicts;
                else if (!assigned[index]) {
                    desired[index] = original_unit[index];
                    assigned[index] = true;
                }
                continue;
            }
            Vec3 weighted{};
            for (const Ref ref : group) {
                const auto& tri = mesh.triangles[ref.triangle];
                weighted =
                    add(weighted, mul(oriented_faces[ref.triangle], tri.corner_angles[ref.corner]));
            }
            const Vec3 replacement = normalize(weighted);
            if (magnitude(replacement) < 0.5f) {
                result.error = "invalid weighted normal";
                return result;
            }
            const bool blend = group.size() > 1;
            for (const Ref ref : group) {
                const auto& tri = mesh.triangles[ref.triangle];
                const auto index = tri.corners[ref.corner].normal;
                const Vec3 target = blend
                                        ? normalize(add(mul(original_unit[index],
                                                            1.0f - options.geometric_weight),
                                                        mul(replacement,
                                                            options.geometric_weight)))
                                        : original_unit[index];
                candidate[index] = true;
                if (assigned[index] && dot(desired[index], target) < conflict_limit)
                    ++result.index_conflicts;
                else if (!assigned[index]) {
                    desired[index] = target;
                    assigned[index] = true;
                }
            }
        }
        result.working_vector_bytes =
            std::max(result.working_vector_bytes, base_working_bytes + nested_vector_bytes(groups));
    }
    result.candidate_indices =
        static_cast<std::uint32_t>(std::count(candidate.begin(), candidate.end(), true));
    if (result.index_conflicts)
        return result;
    result.normals.assign(originals.begin(), originals.end());
    result.working_vector_bytes =
        std::max(result.working_vector_bytes, base_working_bytes + vector_bytes(result.normals));
    for (std::size_t i = 0; i < originals.size(); ++i)
        if (assigned[i] && dot(original_unit[i], desired[i]) < conflict_limit) {
            result.normals[i] = desired[i];
            ++result.changed_indices;
        }
    return result;
}
} // namespace midnafx::smoothing
