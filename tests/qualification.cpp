#include "game/smoothing.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

    smoothing::Options partial;
    partial.preserve_conflicting_indices = true;
    check(!automatic.known_good && !automatic.known_bad && automatic.single_matrix);
    // Opting into index preservation must not waive any automatic graph guard.
    const auto rejected = [&](const auto& graph, const auto& graph_points,
                               const auto& original_normals, const char* reason) {
        const auto result = smoothing::qualify(graph, graph_points, original_normals, automatic, partial);
        check(!result.safe());
        check(std::string(result.reason) == reason);
    };
    rejected(altered, fan_points, overlapping, "overlapping smoothing groups");
    altered = mesh; altered.triangles.push_back(mesh.triangles[1]);
    rejected(altered, positions, normals, "non-manifold edge");
    points = positions; points[3] = points[0];
    rejected(mesh, points, normals, "duplicated position identities");
    altered = mesh; std::swap(altered.triangles[1].corners[0], altered.triangles[1].corners[1]);
    rejected(altered, positions, normals, "inconsistent edge winding");
    altered = mesh; altered.triangles[1].corners = {{{0,3}, {3,4}, {4,5}}};
    rejected(altered, fan_points, normals, "disconnected vertex fan");
    altered = mesh; altered.degenerate_count = 1;
    rejected(altered, positions, normals, "degenerate or nontriangle primitives");
    altered = mesh; altered.ignored_nontriangles = 1;
    rejected(altered, positions, normals, "degenerate or nontriangle primitives");
    altered = mesh; altered.triangles[0].material = 0xffff;
    rejected(altered, positions, normals, "missing material association");
    altered = mesh; altered.triangles[0].corners[0].normal = 6;
    rejected(altered, positions, normals, "corner index outside array");
    altered = mesh; altered.triangles[0].corners[0].position = 4;
    rejected(altered, positions, normals, "corner index outside array");
    altered = mesh; altered.error = "malformed draw";
    rejected(altered, positions, normals, "malformed draw");
    invalid = normals; invalid[0].x = NAN;
    rejected(mesh, positions, invalid, "nonfinite normal");
    points = positions; points[0].x = INFINITY;
    rejected(mesh, points, normals, "nonfinite position");
    representation = automatic; representation.single_matrix = false;
    check(!smoothing::qualify(mesh, positions, normals, representation, partial).safe());

    // Index zero is shared by an isolated corner at position zero and a bent
    // smoothing group at position two. Only zero has conflicting proposals;
    // the other shared corners still have independently valid replacements.
    altered = mesh;
    altered.triangles[1].corners[0].normal = 0;
    const auto strict = smoothing::qualify(altered, positions, normals, automatic);
    check(!strict.safe() && strict.smoothing.index_conflicts > 0);
    const auto preserved = smoothing::qualify(altered, positions, normals, automatic, partial);
    check(preserved.safe() && preserved.smoothing.normals.size() == normals.size());
    check(preserved.smoothing.index_conflicts == 0 && preserved.smoothing.preserved_indices == 1);
    check(preserved.smoothing.changed_indices == 3);
    const auto exact = [](Vec3 a, Vec3 b) {
        return std::memcmp(&a.x, &b.x, sizeof(float)) == 0 &&
               std::memcmp(&a.y, &b.y, sizeof(float)) == 0 &&
               std::memcmp(&a.z, &b.z, sizeof(float)) == 0;
    };
    for (unsigned index : {0u, 3u, 5u})
        check(exact(preserved.smoothing.normals[index], normals[index]));
    for (unsigned index : {1u, 2u, 4u}) {
        const auto normal = preserved.smoothing.normals[index];
        check(!exact(normal, normals[index]));
        check(normal.x > 0.01f && normal.y > 0.01f && normal.z > 0.95f);
        check(std::abs(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z - 1) < 0.00001f);
    }
    std::reverse(altered.triangles.begin(), altered.triangles.end());
    const auto reordered = smoothing::qualify(altered, positions, normals, automatic, partial);
    check(reordered.safe() && reordered.smoothing.changed_indices == 3 &&
          reordered.smoothing.preserved_indices == 1);
    for (unsigned i = 0; i < normals.size(); ++i)
        check(exact(reordered.smoothing.normals[i], preserved.smoothing.normals[i]));

    check(!smoothing::Options{}.preserve_uncertain_vertices);
    auto uncertain = partial;
    uncertain.preserve_uncertain_vertices = true;
    smoothing::Options uncertain_only;
    uncertain_only.preserve_uncertain_vertices = true;
    const auto same_normals = [&](const auto& first, const auto& second) {
        check(first.size() == second.size());
        for (std::size_t i = 0; i < first.size(); ++i)
            check(exact(first[i], second[i]));
    };
    const auto unchanged_order = [&](auto graph, const auto& graph_points,
                                     const auto& original_normals, const auto& expected) {
        std::reverse(graph.triangles.begin(), graph.triangles.end());
        const auto result = smoothing::qualify(graph, graph_points, original_normals, automatic, uncertain);
        check(result.safe());
        check(result.smoothing.changed_indices == expected.smoothing.changed_indices);
        check(result.smoothing.preserved_indices == expected.smoothing.preserved_indices);
        same_normals(result.smoothing.normals, expected.smoothing.normals);
    };

    // Two separate position identities occupy (10,0,0). Their normal indices
    // must remain authored, without welding either disconnected component.
    // Index six also belongs to a bent vertex elsewhere: preserving only its
    // uncertain corner would still corrupt that globally shared normal entry.
    const std::array<Vec3, 10> duplicate_points{{
        positions[0], positions[1], positions[2], positions[3],
        {10,0,0}, {11,0,0}, {10,1,0}, {10,0,0}, {9,0,0}, {10,-1,0}}};
    std::array<Vec3, 12> duplicate_normals;
    duplicate_normals.fill({0,0,1});
    auto duplicate_mesh = mesh;
    duplicate_mesh.triangles[0].corners[1].normal = 6;
    const auto without_duplicates = smoothing::qualify(
        duplicate_mesh, duplicate_points, duplicate_normals, automatic, uncertain);
    check(without_duplicates.safe());
    check(!exact(without_duplicates.smoothing.normals[6], duplicate_normals[6]));
    duplicate_mesh.triangles.push_back(
        {{{{4,6}, {5,7}, {6,8}}}, 0, 0, 0, {0,0,1}, {1,1,1}});
    duplicate_mesh.triangles.push_back(
        {{{{7,9}, {8,10}, {9,11}}}, 0, 0, 0, {0,0,1}, {1,1,1}});
    for (const auto options : {smoothing::Options{}, partial, uncertain_only}) {
        const auto result = smoothing::qualify(
            duplicate_mesh, duplicate_points, duplicate_normals, automatic, options);
        check(!result.safe() && std::string(result.reason) == "duplicated position identities");
    }
    const auto duplicate_preserved = smoothing::qualify(
        duplicate_mesh, duplicate_points, duplicate_normals, automatic, uncertain);
    check(duplicate_preserved.safe() && duplicate_preserved.smoothing.index_conflicts == 0);
    check(duplicate_preserved.smoothing.preserved_indices == 2);
    check(duplicate_preserved.smoothing.changed_indices == 3);
    for (unsigned index : {0u, 1u, 5u, 6u, 7u, 8u, 9u, 10u, 11u})
        check(exact(duplicate_preserved.smoothing.normals[index], duplicate_normals[index]));
    for (unsigned index : {2u, 3u, 4u}) {
        const auto normal = duplicate_preserved.smoothing.normals[index];
        check(!exact(normal, duplicate_normals[index]));
        check(normal.x > 0.01f && normal.y > 0.01f && normal.z > 0.95f);
    }
    unchanged_order(duplicate_mesh, duplicate_points, duplicate_normals, duplicate_preserved);

    // The central fan's 0/15/30-degree authored normals overlap the split
    // threshold nontransitively. Keep every central index, while the separate
    // two-face fans at positions two and three can still improve safely.
    auto overlapping_mesh = mesh;
    overlapping_mesh.triangles = {
        {{{{0,0}, {1,1}, {2,2}}}, 0, 0, 0, {0,0,1}, {1,1,1}},
        {{{{0,3}, {2,4}, {3,5}}}, 0, 0, 0, {0,0,1}, {1,1,1}},
        {{{{0,6}, {3,7}, {4,8}}}, 0, 0, 0, {0,0,1}, {1,1,1}}};
    for (const auto options : {smoothing::Options{}, partial, uncertain_only}) {
        const auto result = smoothing::qualify(
            overlapping_mesh, fan_points, overlapping, automatic, options);
        check(!result.safe() && std::string(result.reason) == "overlapping smoothing groups");
    }
    const auto overlap_preserved = smoothing::qualify(
        overlapping_mesh, fan_points, overlapping, automatic, uncertain);
    check(overlap_preserved.safe() && overlap_preserved.smoothing.index_conflicts == 0);
    check(overlap_preserved.smoothing.preserved_indices == 3);
    check(overlap_preserved.smoothing.changed_indices == 3);
    for (unsigned index : {0u, 1u, 2u, 3u, 6u, 8u})
        check(exact(overlap_preserved.smoothing.normals[index], overlapping[index]));
    for (unsigned index : {4u, 5u, 7u}) {
        const auto normal = overlap_preserved.smoothing.normals[index];
        check(!exact(normal, overlapping[index]));
        check(normal.x > 0 && normal.x < overlapping[index].x);
        check(normal.z > overlapping[index].z);
    }
    unchanged_order(overlapping_mesh, fan_points, overlapping, overlap_preserved);

    // Duplicating every position identity protects every referenced index;
    // allowing qualification must not invent a change or weld the two patches.
    const std::array<Vec3, 8> all_duplicate_points{{
        positions[0], positions[1], positions[2], positions[3],
        positions[0], positions[1], positions[2], positions[3]}};
    auto all_duplicate_mesh = mesh;
    for (auto triangle : mesh.triangles) {
        for (auto& corner : triangle.corners) {
            corner.position += 4;
            corner.normal += 6;
        }
        all_duplicate_mesh.triangles.push_back(triangle);
    }
    const auto all_preserved = smoothing::qualify(
        all_duplicate_mesh, all_duplicate_points, duplicate_normals, automatic, uncertain);
    check(all_preserved.safe() && all_preserved.smoothing.changed_indices == 0);
    check(all_preserved.smoothing.preserved_indices == duplicate_normals.size());
    same_normals(all_preserved.smoothing.normals, duplicate_normals);
    unchanged_order(all_duplicate_mesh, all_duplicate_points, duplicate_normals, all_preserved);

    const auto uncertain_rejected = [&](const auto& graph, const auto& graph_points,
                                        const auto& original_normals, const char* reason) {
        const auto result = smoothing::qualify(graph, graph_points, original_normals, automatic, uncertain);
        check(!result.safe() && std::string(result.reason) == reason);
    };
    altered = mesh; altered.triangles.push_back(mesh.triangles[1]);
    uncertain_rejected(altered, positions, normals, "non-manifold edge");
    altered = mesh; std::swap(altered.triangles[1].corners[0], altered.triangles[1].corners[1]);
    uncertain_rejected(altered, positions, normals, "inconsistent edge winding");
    altered = mesh; altered.triangles[1].corners = {{{0,3}, {3,4}, {4,5}}};
    uncertain_rejected(altered, fan_points, normals, "disconnected vertex fan");
    altered = mesh; altered.degenerate_count = 1;
    uncertain_rejected(altered, positions, normals, "degenerate or nontriangle primitives");
    altered = mesh; altered.triangles[0].material = 0xffff;
    uncertain_rejected(altered, positions, normals, "missing material association");
    altered = mesh; altered.triangles[0].corners[0].normal = 6;
    uncertain_rejected(altered, positions, normals, "corner index outside array");
    std::puts("qualification: connected patch accepted; adversarial safety cases passed");
}
