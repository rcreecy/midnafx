#include "render/grass_assembly.hpp"
#include <array>
#include <cstdio>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "line %d\n", __LINE__); return 1; } } while (false)
struct Camera {
    float proj_from_world[16]{}, view_from_world[16]{}, eye[3]{};
    float fovy = 60, aspect = 1.8f, far_plane = 10000;
};
struct Vec { float x = 0, y = 0, z = 0; };
struct Instance { Vec point, trail; unsigned geometry = 123; };
int main() {
    using namespace midnafx::render::grass_assembly;
    Camera camera;
    const auto original = key(camera, 15000, 7, 6);
    Cache cache;
    CHECK(!cache.reusable(original, true));
    cache.remember(original, true, 2, 4);
    CHECK(cache.reusable(original, true) && !cache.reusable(original, false));
    for (unsigned i = 0; i < 16; ++i) {
        Camera changed = camera; changed.proj_from_world[i] = 1;
        CHECK(!cache.reusable(key(changed, 15000, 7, 6), true));
        changed = camera; changed.view_from_world[i] = 1;
        CHECK(!cache.reusable(key(changed, 15000, 7, 6), true));
    }
    for (unsigned i = 0; i < 3; ++i) {
        Camera changed = camera; changed.eye[i] = 1;
        CHECK(!cache.reusable(key(changed, 15000, 7, 6), true));
    }
    Camera changed = camera; changed.fovy = 61; CHECK(key(changed, 15000, 7, 6) != original);
    changed = camera; changed.aspect = 2; CHECK(key(changed, 15000, 7, 6) != original);
    changed = camera; changed.far_plane = 20000; CHECK(key(changed, 15000, 7, 6) != original);
    changed = camera; changed.eye[0] = -0.0f; CHECK(key(changed, 15000, 7, 6) != original);
    CHECK(!cache.reusable(key(camera, 15001, 7, 6), true));
    CHECK(!cache.reusable(key(camera, 15000, 8, 6), true));
    CHECK(!cache.reusable(key(camera, 15000, 7, 5), true));
    std::array<Instance, 6> instances{};
    unsigned samples = 0;
    float strength = 1;
    auto live = [&](Vec point) { ++samples; return Vec{point.x, point.y, strength}; };
    CHECK(cache.refresh(instances, live) == 4 && samples == 4);
    CHECK(instances[2].trail.z == 0 && instances[3].trail.z == 0);
    strength = 0.01f;
    CHECK(cache.refresh(instances, live) == 0 && samples == 8);
    CHECK(instances[0].trail.z == strength && instances[5].trail.z == strength);
    bool changed_trails = false;
    CHECK(cache.refresh(instances, live, &changed_trails) == 0 && !changed_trails);
    strength = 0.001f;
    cache.refresh(instances, live, &changed_trails); CHECK(changed_trails);
    changed_trails = false;
    strength = -0.0f;
    cache.refresh(instances, live, &changed_trails); CHECK(changed_trails);
    changed_trails = false;
    strength = 0.0f;
    cache.refresh(instances, live, &changed_trails); CHECK(changed_trails);
    for (const auto& instance : instances) CHECK(instance.geometry == 123);
    cache.remember(original, false, 2, 4); CHECK(!cache.reusable(original, true));
    cache.remember(original, true, 2, 4); cache.reset(); CHECK(!cache.reusable(original, true));
}
