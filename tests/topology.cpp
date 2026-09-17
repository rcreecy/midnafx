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
Result run(const std::vector<std::uint8_t>& bytes) {
    const Group group{bytes.data(), bytes.size()};
    const Shape shape{attrs, std::span<const Group>(&group, 1), 2};
    return decode(std::span<const Shape>(&shape, 1), formats, points, 4);
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

    indexed[10] = 0xff;
    indexed[11] = 0x7f;
    r = run(indexed);
    check(!r.ok());
}
