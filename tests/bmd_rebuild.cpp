#include "game/bmd_rebuild.hpp"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void check(bool condition) {
    if (!condition)
        throw std::runtime_error("BMD rebuild test failed");
}

void put16(std::vector<std::byte>& data, std::size_t at, std::uint16_t value) {
    data[at] = std::byte(value >> 8);
    data[at + 1] = std::byte(value);
}

void put32(std::vector<std::byte>& data, std::size_t at, std::uint32_t value) {
    data[at] = std::byte(value >> 24);
    data[at + 1] = std::byte(value >> 16);
    data[at + 2] = std::byte(value >> 8);
    data[at + 3] = std::byte(value);
}

void text(std::vector<std::byte>& data, std::size_t at, const char* value) {
    for (unsigned i = 0; i < 4; ++i)
        data[at + i] = std::byte(value[i]);
}

void f32(std::vector<std::byte>& data, std::size_t at, std::uint32_t bits) {
    put32(data, at, bits);
}

std::vector<std::byte> fixture() {
    constexpr std::size_t vtx = 32, vtx_size = 256;
    constexpr std::size_t shp = vtx + vtx_size, shp_size = 192;
    std::vector<std::byte> data(shp + shp_size);
    text(data, 0, "J3D2");
    text(data, 4, "bmd3");
    put32(data, 8, static_cast<std::uint32_t>(data.size()));
    put32(data, 12, 2);

    text(data, vtx, "VTX1");
    put32(data, vtx + 4, vtx_size);
    put32(data, vtx + 8, 64);
    put32(data, vtx + 12, 112);
    put32(data, vtx + 16, 160);
    put32(data, vtx + 20, 192);
    put32(data, vtx + 64, 9);
    put32(data, vtx + 68, 1);
    put32(data, vtx + 72, 4);
    put32(data, vtx + 80, 10);
    put32(data, vtx + 84, 0);
    put32(data, vtx + 88, 4);
    put32(data, vtx + 96, 255);
    const std::uint32_t points[4][3] = {
        {0, 0, 0}, {0x3f800000, 0, 0}, {0, 0x3f800000, 0}, {0x3f800000, 0x3f800000, 0}};
    for (unsigned p = 0; p < 4; ++p)
        for (unsigned c = 0; c < 3; ++c)
            f32(data, vtx + 112 + p * 12 + c * 4, points[p][c]);
    f32(data, vtx + 160 + 8, 0x3f800000);
    f32(data, vtx + 172, 0x3f800000);

    text(data, shp, "SHP1");
    put32(data, shp + 4, shp_size);
    put16(data, shp + 8, 1);
    put32(data, shp + 12, 44);
    put32(data, shp + 16, 84);
    put32(data, shp + 24, 96);
    put32(data, shp + 32, 128);
    put32(data, shp + 36, 160);
    put32(data, shp + 40, 160);
    put16(data, shp + 44 + 2, 1);
    put16(data, shp + 44 + 4, 0);
    put16(data, shp + 44 + 8, 0);
    put16(data, shp + 84, 0);
    put32(data, shp + 96, 9);
    put32(data, shp + 100, 3);
    put32(data, shp + 104, 10);
    put32(data, shp + 108, 3);
    put32(data, shp + 112, 255);
    data[shp + 128] = std::byte{0x98};
    put16(data, shp + 129, 4);
    const std::uint16_t corners[4][2] = {{0, 0}, {1, 0}, {2, 1}, {3, 1}};
    for (unsigned i = 0; i < 4; ++i) {
        put16(data, shp + 131 + i * 4, corners[i][0]);
        put16(data, shp + 133 + i * 4, corners[i][1]);
    }
    put32(data, shp + 160, 32);
    put32(data, shp + 164, 0);
    return data;
}

std::uint64_t hash_expected(bool rebuilt) {
    std::uint64_t hash = 14695981039346656037ULL;
    const auto normal = [rebuilt](std::uint16_t rebuilt_value, std::uint16_t original_value) {
        return rebuilt ? rebuilt_value : original_value;
    };
    const std::uint16_t values[] = {
        0, 0, 0, normal(2, 0), 1, normal(3, 0), 2, normal(4, 1),
        0, 0, 2, normal(5, 1), 1, normal(6, 0), 3, normal(7, 1)};
    for (const auto value : values)
        for (unsigned shift : {0u, 8u}) {
            hash ^= (value >> shift) & 0xff;
            hash *= 1099511628211ULL;
        }
    return hash;
}
} // namespace

int main(int argc, char** argv) {
    if (argc == 3) {
        std::ifstream input(argv[1], std::ios::binary);
        std::vector<char> chars{std::istreambuf_iterator<char>(input), {}};
        std::vector<std::byte> bytes(chars.size());
        for (std::size_t i = 0; i < chars.size(); ++i)
            bytes[i] = std::byte(static_cast<unsigned char>(chars[i]));
        const auto result = midnafx::bmd_rebuild::split_normals(bytes);
        if (!result.ok())
            return 2;
        std::ofstream output(argv[2], std::ios::binary);
        output.write(reinterpret_cast<const char*>(result.bytes.data()),
                     static_cast<std::streamsize>(result.bytes.size()));
        return output ? 0 : 3;
    }

    const auto original = fixture();
    const auto preserved = original;
    const auto result = midnafx::bmd_rebuild::split_normals(original);
    check(result.ok());
    check(original == preserved);
    check(result.bytes.size() == 544);
    check(result.evidence.shapes == 1);
    check(result.evidence.matrix_groups == 1);
    check(result.evidence.triangles == 2);
    check(result.evidence.degenerate_triangles == 0);
    check(result.evidence.original_normals == 2);
    check(result.evidence.written_normals == 8);
    check(result.evidence.rebuilt_normal_capacity == 8);
    check(result.evidence.source_draws == 1);
    check(result.evidence.rebuilt_draws == 2);
    check(result.evidence.original_corner_hash == hash_expected(false));
    check(result.evidence.rebuilt_corner_hash == hash_expected(true));

    auto malformed = original;
    malformed[32 + 256 + 128] = std::byte{0x61};
    check(!midnafx::bmd_rebuild::split_normals(malformed).ok());

    auto index8_normal = original;
    put32(index8_normal, 32 + 256 + 108, 2);
    check(!midnafx::bmd_rebuild::split_normals(index8_normal).ok());

    auto bad_normal = original;
    put16(bad_normal, 32 + 256 + 133, 2);
    check(!midnafx::bmd_rebuild::split_normals(bad_normal).ok());

    auto degenerate = original;
    put16(degenerate, 32 + 256 + 131, 1);
    const auto skipped = midnafx::bmd_rebuild::split_normals(degenerate);
    check(skipped.ok());
    check(skipped.evidence.triangles == 1);
    check(skipped.evidence.degenerate_triangles == 1);
}
