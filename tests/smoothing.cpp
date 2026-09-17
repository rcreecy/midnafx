#include "game/smoothing.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#define check(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "smoothing test failed at line %d\n", __LINE__);                  \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (false)

using midnafx::topology::Corner;
using midnafx::topology::Triangle;
using midnafx::topology::Vec3;

namespace {
Triangle tri(std::array<Corner, 3> corners, Vec3 face, std::uint16_t material) {
    Triangle out;
    out.corners = corners;
    out.face_normal = face;
    out.corner_angles = {1, 1, 1};
    out.material = material;
    return out;
}
} // namespace

int main() {
    using namespace midnafx;
    std::int16_t encoded[3];
    check(smoothing::encode_s16_xyz({0.25f, -0.5f, 1.0f}, 15, encoded));
    const Vec3 decoded = smoothing::decode_s16_xyz(encoded, 15);
    check(std::abs(decoded.x - 0.25f) < 1e-4f);
    check(std::abs(decoded.y + 0.5f) < 1e-4f);
    check(std::abs(decoded.z - 1.0f) < 1e-4f);
    check(!smoothing::encode_s16_xyz({NAN, 0, 1}, 15, encoded));
    check(!smoothing::encode_s16_xyz({0, 0, 1}, 16, encoded));
    topology::Result mesh;
    mesh.triangles.push_back(tri({{{0, 0}, {1, 2}, {2, 3}}}, {0, 0, 1}, 0));
    mesh.triangles.push_back(tri({{{0, 1}, {3, 4}, {4, 5}}}, {0, 0.5f, 0.8660254f}, 0));
    const std::array<Vec3, 6> normals{{{0, 0, 1},
                                       {0, 0.5f, 0.8660254f},
                                       {0, 0, 1},
                                       {0, 0, 1},
                                       {0, 0.5f, 0.8660254f},
                                       {0, 0.5f, 0.8660254f}}};
    auto result = smoothing::plan(mesh, normals);
    check(result.safe());
    check(result.changed_indices == 0); // 30-degree original split survives.

    auto permissive = smoothing::Options{};
    permissive.original_split_degrees = 40;
    result = smoothing::plan(mesh, normals, permissive);
    check(result.safe() && result.changed_indices == 2);
    check(result.normals[0].y > 0.2f && result.normals[0].y < 0.3f);
    check(std::abs(result.normals[0].y - result.normals[1].y) < 1e-5f);

    mesh.triangles[1].material = 1;
    result = smoothing::plan(mesh, normals, permissive);
    check(result.safe() && result.changed_indices == 0);
    mesh.triangles[1].material = 0;

    auto narrow = permissive;
    narrow.face_angle_degrees = 20;
    result = smoothing::plan(mesh, normals, narrow);
    check(result.safe() && result.changed_indices == 0);

    mesh.triangles[0].corner_angles[0] = 3;
    result = smoothing::plan(mesh, normals, permissive);
    check(result.safe() && result.changed_indices == 2);
    check(result.normals[0].y < 0.2f); // Corner-angle weighting favors the first face.
    mesh.triangles[0].corner_angles[0] = 1;

    mesh.triangles[1].face_normal = {0, 0, -1};
    result = smoothing::plan(mesh, normals, permissive);
    check(result.safe() && result.changed_indices == 1); // Face orientation is aligned to source.
    mesh.triangles[1].face_normal = {0, 0.5f, 0.8660254f};

    mesh.triangles[1].corners[1].normal = 0; // shared index at another position.
    result = smoothing::plan(mesh, normals, permissive);
    check(!result.safe() && result.index_conflicts > 0);
}
