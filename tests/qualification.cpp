#include "game/smoothing.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#define check(c)                                                                                   \
    do {                                                                                           \
        if (!(c)) {                                                                                \
            std::fprintf(stderr, "qualification failure line %d\n", __LINE__);                     \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (false)
using namespace midnafx;
using smoothing::Class;
using topology::Vec3;

int main() {
    // A bent, connected patch with independently owned corner normals.
    const std::array<Vec3, 4> positions{{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, -0.4f}}};
    std::array<Vec3, 6> normals;
    normals.fill({0, 0, 1});
    topology::Result mesh;
    mesh.triangles = {
        {{{{0, 0}, {1, 1}, {2, 2}}}, 0, 0, 0, {0, 0, 1}, {1, 1, 1}},
        {{{{2, 3}, {1, 4}, {3, 5}}}, 0, 0, 0, {0.348155f, 0.348155f, 0.870388f}, {1, 1, 1}}};
    const smoothing::Representation automatic{true, true, false, false};
    auto q = smoothing::qualify(mesh, positions, normals, automatic);
    check(q.safe() && q.smoothing.changed_indices > 0);
    const auto changed = q.smoothing.changed_indices;
    std::reverse(mesh.triangles.begin(), mesh.triangles.end());
    check(smoothing::qualify(mesh, positions, normals, automatic).smoothing.changed_indices ==
          changed);
    std::reverse(mesh.triangles.begin(), mesh.triangles.end());
    auto altered = mesh;
    altered.triangles[1].material = 1;
    q = smoothing::qualify(altered, positions, normals, automatic);
    check(q.safe() && q.smoothing.changed_indices == 0);
    altered = mesh;
    altered.triangles[1].face_normal = {1, 0, 0};
    check(!smoothing::qualify(altered, positions, normals, automatic).safe());
    auto invalid = normals;
    invalid[0].x = NAN;
    check(!smoothing::plan(mesh, invalid).safe());
    check(smoothing::qualify(mesh, positions, invalid, automatic).classification ==
          Class::Unsupported);
    invalid = normals;
    invalid[0].x = INFINITY;
    check(!smoothing::plan(mesh, invalid).safe());
    invalid = normals;
    invalid[0] = {};
    check(!smoothing::qualify(mesh, positions, invalid, automatic).safe());
    auto points = positions;
    points[0].x = NAN;
    check(!smoothing::qualify(mesh, points, normals, automatic).safe());
    points = positions;
    points[3] = points[0];
    check(smoothing::qualify(mesh, points, normals, automatic).classification == Class::Ambiguous);
    altered = mesh;
    altered.degenerate_count = 1;
    check(!smoothing::qualify(altered, positions, normals, automatic).safe());
    altered = mesh;
    altered.ignored_nontriangles = 1;
    check(!smoothing::qualify(altered, positions, normals, automatic).safe());
    altered = mesh;
    altered.error = "malformed draw";
    check(smoothing::qualify(altered, positions, normals, automatic).classification ==
          Class::Unsupported);
    altered = mesh;
    altered.triangles[0].corners[0].normal = 6;
    check(!smoothing::qualify(altered, positions, normals, automatic).safe());
    altered = mesh;
    altered.triangles[0].corners[0].position = 4;
    check(!smoothing::qualify(altered, positions, normals, automatic).safe());
    altered = mesh;
    altered.triangles[0].material = 0xffff;
    check(!smoothing::qualify(altered, positions, normals, automatic).safe());
    altered = mesh;
    altered.triangles.push_back(mesh.triangles[1]);
    check(smoothing::qualify(altered, positions, normals, automatic).classification ==
          Class::Ambiguous);
    altered = mesh;
    std::swap(altered.triangles[1].corners[0], altered.triangles[1].corners[1]);
    check(!smoothing::qualify(altered, positions, normals, automatic).safe());
    altered = mesh;
    altered.triangles[1].corners[0].normal = 0;
    check(smoothing::qualify(altered, positions, normals, automatic).smoothing.index_conflicts > 0);
    auto representation = automatic;
    representation.single_matrix = false;
    check(smoothing::qualify(mesh, positions, normals, representation).classification ==
          Class::Unsupported);
    representation.known_good = true;
    check(smoothing::qualify(mesh, positions, normals, representation).safe());
    representation.known_bad = true;
    check(!smoothing::qualify(mesh, positions, normals, representation).safe());
    representation = automatic;
    representation.supported_normals = false;
    check(!smoothing::qualify(mesh, positions, normals, representation).safe());
    // Two fans touching only at one vertex must not be averaged together.
    const std::array<Vec3, 5> fan_points{{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {-1, 0, 0}, {0, -1, 0}}};
    altered = mesh;
    altered.triangles[1].corners = {{{0, 3}, {3, 4}, {4, 5}}};
    check(smoothing::qualify(altered, fan_points, normals, automatic).classification ==
          Class::Ambiguous);
    // Hard authored splits survive even when face angles permit smoothing.
    invalid = normals;
    invalid[3] = invalid[4] = invalid[5] = {0.5f, 0, 0.8660254f};
    q = smoothing::qualify(mesh, positions, invalid, automatic);
    check(q.safe() && q.smoothing.changed_indices == 0);
    altered = mesh;
    altered.triangles = {{{{{0, 0}, {1, 1}, {2, 2}}}, 0, 0, 0, {0, 0, 1}, {1, 1, 1}},
                         {{{{0, 3}, {2, 4}, {3, 5}}}, 0, 0, 0, {0, 0, 1}, {1, 1, 1}},
                         {{{{0, 6}, {3, 7}, {4, 8}}}, 0, 0, 0, {0, 0, 1}, {1, 1, 1}}};
    const std::array<Vec3, 9> overlapping{{{0, 0, 1},
                                           {0, 0, 1},
                                           {0, 0, 1},
                                           {0.258819f, 0, 0.965926f},
                                           {0.258819f, 0, 0.965926f},
                                           {0.258819f, 0, 0.965926f},
                                           {0.5f, 0, 0.8660254f},
                                           {0.5f, 0, 0.8660254f},
                                           {0.5f, 0, 0.8660254f}}};
    q = smoothing::qualify(altered, fan_points, overlapping, automatic);
    check(q.classification == Class::Ambiguous);
    check(std::string(q.reason) == "overlapping smoothing groups");
    std::puts("qualification: connected patch accepted; adversarial safety cases passed");
}
