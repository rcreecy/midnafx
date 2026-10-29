#include "game/topology.hpp"

#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

void check(bool condition) {
    if (!condition)
        throw std::runtime_error("topology test failed");
}

using namespace midnafx::topology;

namespace {
const std::array<Attribute, 2> attrs{{{9, 3}, {10, 3}}};
const std::array<Format, 2> formats{{{9, 1, 4}, {10, 0, 4}}};
const std::array<Vec3, 4> points{{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}}};

void push16(std::vector<std::uint8_t>& bytes, unsigned n) {
    bytes.push_back(static_cast<std::uint8_t>(n >> 8));
    bytes.push_back(static_cast<std::uint8_t>(n));
}
void vertex(std::vector<std::uint8_t>& bytes, unsigned p, unsigned n) {
    push16(bytes, p);
    push16(bytes, n);
}
Result run(const std::vector<std::uint8_t>& bytes, bool require_matrix_zero = false,
           std::span<const Attribute> attributes = attrs) {
    const Group group{bytes.data(), bytes.size()};
    const Shape shape{attributes, std::span<const Group>(&group, 1), 2, require_matrix_zero};
    return decode(std::span<const Shape>(&shape, 1), formats, points, 4);
}
std::vector<std::uint8_t> matrix_draw(bool aurora, unsigned kind) {
    std::vector<std::uint8_t> bytes = aurora ?
        std::vector<std::uint8_t>{0x50, 0, 0x41, 0x90, 0, 4, 0, 0, 0, 6} :
        std::vector<std::uint8_t>{0x98, 0, 4};
    if (aurora) {
        const std::array<std::uint16_t,6> order{{0,1,2,2,1,3}};
        const auto* raw = reinterpret_cast<const std::uint8_t*>(order.data());
        bytes.insert(bytes.end(), raw, raw + sizeof(order));
    }
    for (unsigned i = 0; i < 4; ++i) {
        if (kind == 3) push16(bytes, 0);
        else bytes.push_back(0);
        vertex(bytes, i, i);
    }
    return bytes;
}
} // namespace

int main() {
    // The second strip triangle reverses its first two encoded corners.
    std::vector<std::uint8_t> strip{0x98, 0, 4};
    vertex(strip, 0, 0);
    vertex(strip, 1, 1);
    vertex(strip, 2, 2);
    vertex(strip, 3, 3);
    auto r = run(strip);
    check(r.ok() && r.triangles.size() == 2 && r.strip_count == 1);
    check(r.triangles[0].corners[0].position == 0);
    check(r.triangles[1].corners[0].position == 2);
    check(r.triangles[1].corners[1].position == 1);
    check(r.triangles[0].face_normal.z > 0 && r.triangles[1].face_normal.z > 0);
    // Constant matrix ownership is qualified outside the decoder. No explicit
    // PNMTXIDX is required when every shape uses that already validated matrix.
    check(run(strip, true).ok());

    std::vector<std::uint8_t> fan{0xa0, 0, 4};
    vertex(fan, 0, 3);
    vertex(fan, 1, 2);
    vertex(fan, 3, 1);
    vertex(fan, 2, 0);
    r = run(fan);
    check(r.ok() && r.triangles.size() == 2 && r.fan_count == 1);
    check(r.triangles[1].corners[0].normal == 3);

    std::vector<std::uint8_t> degenerate{0x90, 0, 3};
    vertex(degenerate, 0, 0);
    vertex(degenerate, 0, 1);
    vertex(degenerate, 2, 2);
    r = run(degenerate);
    check(r.ok() && r.triangles.empty() && r.degenerate_count == 1);

    std::vector<std::uint8_t> indexed{0x50, 0, 0x41, 0x90, 0, 4, 0, 0, 0, 6};
    const std::array<std::uint16_t, 6> order{{0, 1, 2, 2, 1, 3}};
    const auto* raw = reinterpret_cast<const std::uint8_t*>(order.data());
    indexed.insert(indexed.end(), raw, raw + sizeof(order));
    for (unsigned i = 0; i < 4; ++i)
        vertex(indexed, i, i);
    r = run(indexed);
    check(r.ok() && r.indexed_count == 1 && r.triangles.size() == 2);
    check(r.triangles[1].corners[0].position == 2);
    check(run(indexed, true).ok());

    indexed[10] = 0xff;
    indexed[11] = 0x7f;
    r = run(indexed);
    check(!r.ok());

    for (bool aurora : {false, true}) {
        std::array<Attribute,3> matrix_attributes{{{0,1}, {9,3}, {10,3}}};
        const auto zero = matrix_draw(aurora, 1);
        const auto accepted = run(zero, true, matrix_attributes);
        check(accepted.ok() && accepted.triangles.size() == 2);
        check(accepted.indexed_count == unsigned(aurora));
        check(accepted.triangles[0].corners[0].position == 0 &&
              accepted.triangles[1].corners[0].position == 2);
        const size_t vertices = aurora ? 10 + 6 * sizeof(std::uint16_t) : 3;
        for (unsigned vertex_index = 0; vertex_index < 4; ++vertex_index)
            for (std::uint8_t matrix : {std::uint8_t{1}, std::uint8_t{3}, std::uint8_t{255}}) {
                auto nonzero = zero;
                nonzero[vertices + vertex_index * 5] = matrix;
                check(!run(nonzero, true, matrix_attributes).ok());
                const auto legacy = run(nonzero, false, matrix_attributes);
                check(legacy.ok() && legacy.corner_hash == accepted.corner_hash);
            }
        for (unsigned indexed_kind : {2u, 3u}) {
            matrix_attributes[0].kind = static_cast<std::uint8_t>(indexed_kind);
            const auto matrix_indices = matrix_draw(aurora, indexed_kind);
            check(!run(matrix_indices, true, matrix_attributes).ok());
            check(run(matrix_indices, false, matrix_attributes).ok());
        }
    }
}
