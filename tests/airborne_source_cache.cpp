#include "render/airborne_source_cache.hpp"
#include <cstdio>
#include <vector>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "line %d\n", __LINE__); return 1; } } while (false)
struct Camera { float proj_from_world[16]{}, eye[3]{}; };
struct Source { std::array<float, 3> point{}; bool forest = false; bool operator==(const Source&) const = default; };
int main() {
    midnafx::render::airborne_sources::Cache<Source> cache;
    Camera camera;
    std::array<Source, 4> output{};
    const std::array<Source, 3> sources{{{{1,2,3},true},{{1,2,3},false},{{7,8,9},true}}};
    unsigned count = 99;
    CHECK(!cache.load(camera, 1, output, count) && count == 99);
    cache.store(camera, 1, output.size(), sources);
    output[3] = {{99,99,99},true};
    CHECK(cache.load(camera, 1, output, count) && count == sources.size());
    for (unsigned i = 0; i < count; ++i) CHECK(output[i] == sources[i]);
    CHECK(output[3].point[0] == 99); // Preserve the unused output tail.
    for (unsigned i = 0; i < 16; ++i) {
        auto changed = camera; changed.proj_from_world[i] = 1;
        CHECK(!cache.load(changed, 1, output, count));
    }
    for (unsigned i = 0; i < 3; ++i) {
        auto changed = camera; changed.eye[i] = 1;
        CHECK(!cache.load(changed, 1, output, count));
    }
    auto changed = camera; changed.eye[0] = -0.0f;
    CHECK(!cache.load(changed, 1, output, count));
    CHECK(!cache.load(camera, 2, output, count));
    CHECK(!cache.load(camera, 1, std::span(output).first(2), count));
    // Requested count matters: partial_sort's tie order can depend on it.
    cache.store(camera, 1, 2, std::span(sources).first(2));
    CHECK(cache.load(camera, 1, std::span(output).first(2), count) && count == 2);
    CHECK(!cache.load(camera, 1, output, count));
    std::vector<Source> large(65);
    cache.store(camera, 1, large.size(), large);
    CHECK(!cache.load(camera, 1, large, count));
    cache.store(camera, 1, 0, {});
    CHECK(cache.load(camera, 1, {}, count) && count == 0);
    cache.reset(); CHECK(!cache.load(camera, 1, {}, count));
    // Unload and same-address resource reuse cannot retain the prior values.
    cache.store(camera, 2, output.size(), sources);
    auto replacement = sources; replacement[0].point[0] = 42;
    cache.store(camera, 3, output.size(), replacement);
    CHECK(cache.load(camera, 3, output, count) && output[0] == replacement[0]);
    CHECK(!cache.load(camera, 2, output, count));
}
