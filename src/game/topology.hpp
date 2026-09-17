#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace midnafx::topology {

struct Vec3 {
    float x = 0, y = 0, z = 0;
};

struct Attribute {
    std::uint8_t id = 0;
    std::uint8_t kind = 0; // GX_NONE, GX_DIRECT, GX_INDEX8, GX_INDEX16
};

struct Format {
    std::uint8_t id = 0;
    std::uint8_t components = 0;
    std::uint8_t type = 0;
};

struct Group {
    const std::uint8_t* display_list = nullptr;
    std::size_t size = 0;
};

struct Shape {
    std::span<const Attribute> attributes;
    std::span<const Group> groups;
    std::uint16_t material = 0xffff;
};

struct Corner {
    std::uint16_t position = 0;
    std::uint16_t normal = 0;
};

struct Triangle {
    std::array<Corner, 3> corners;
    std::uint16_t shape = 0, material = 0xffff, group = 0;
    Vec3 face_normal;
    std::array<float, 3> corner_angles{};
};

struct Result {
    std::vector<Triangle> triangles;
    std::uint32_t primitive_count = 0;
    std::uint32_t strip_count = 0, fan_count = 0, indexed_count = 0;
    std::uint32_t degenerate_count = 0;
    std::uint32_t unique_positions = 0, unique_normals = 0;
    std::uint32_t position_normal_splits = 0;
    std::uint32_t ignored_nontriangles = 0;
    std::uint64_t corner_hash = 0;
    const char* error = nullptr;

    bool ok() const { return error == nullptr; }
};

// Bounded, read-only decoder of Aurora's GX draw representation. Every triangle
// keeps independent position/normal indices and shape/material/group identity.
Result decode(std::span<const Shape> shapes, std::span<const Format> formats,
              std::span<const Vec3> positions, std::uint32_t normal_count);

} // namespace midnafx::topology
