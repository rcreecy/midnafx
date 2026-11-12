#include "render/terrain_index.hpp"
#include "render/grass_view.hpp"
#include <chrono>
#include <cstdio>
#include <limits>
#include <random>
#include <string_view>
#include <memory>

using Index = midnafx::render::TerrainIndex;
using Bounds = Index::Bounds;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "failed line %d\n", __LINE__); return 1; } } while (false)

static std::vector<Bounds> terrain(unsigned count, bool overlapping = false) {
    std::vector<Bounds> result;
    for (unsigned i = 0; i < count; ++i) {
        const float x = float(i % 128) * 160 - 10000, z = float(i / 128) * 160 - 5000;
        result.push_back(overlapping ? Bounds{-12000, 12000, -12000, 12000}
                                     : Bounds{x, x + 130, z, z + 130});
    }
    return result;
}
static Index make_index(const std::vector<Bounds>& surfaces) {
    Index index;
    index.rebuild(static_cast<unsigned>(surfaces.size()), [&](auto i) { return surfaces[i]; });
    return index;
}
static int correctness() {
    Index empty;
    CHECK(!empty.any({0, 1, 0, 1}, [](auto) { return true; }));
    std::mt19937 rng(1138);
    for (unsigned count : {1u, 8u, 9u, 32u, 8192u}) {
        auto surfaces = terrain(count);
        auto index = make_index(surfaces);
        for (unsigned trial = 0; trial < 4000; ++trial) {
            const float x = float(int(rng() % 30000) - 15000), z = float(int(rng() % 30000) - 15000);
            const float size = trial % 3 == 0 ? 256.0f : trial % 3 == 1 ? 1024.0f : 4096.0f;
            Bounds box{x, x + size, z, z + size};
            const auto accept = [&](unsigned i) { return (i + trial) % 13 == 0; };
            bool expected = false;
            for (unsigned i = 0; i < count; ++i) expected |= surfaces[i].overlaps(box) && accept(i);
            CHECK(index.any(box, accept) == expected);
        }
        // Touching edges and points remain inclusive.
        for (const auto& b : surfaces)
            CHECK(index.any({b.max_x, b.max_x, b.max_z, b.max_z}, [](auto) { return true; }));
        // Rebuild, unload, and slot reuse cannot retain old bounds.
        index.rebuild(0, [](auto) { return Bounds{}; });
        CHECK(!index.any({-20000, 20000, -20000, 20000}, [](auto) { return true; }));
        index.rebuild(1, [](auto) { return Bounds{4, 5, 6, 7}; });
        CHECK(index.any({4, 4, 6, 6}, [](auto) { return true; }));
        CHECK(!index.any({0, 1, 0, 1}, [](auto) { return true; }));
    }
    // Invalid/infinite input uses the exact linear comparison semantics.
    const float nan = std::numeric_limits<float>::quiet_NaN(), inf = std::numeric_limits<float>::infinity();
    const std::vector<Bounds> unusual{{nan, 5, 0, 5}, {-inf, inf, -inf, inf}, {10, 1, 0, 1}};
    auto index = make_index(unusual);
    CHECK(index.any({0, 1, 0, 1}, [](auto i) { return i == 1; }));
    CHECK(!index.any({0, 1, 0, 1}, [](auto i) { return i == 0; }));
    return 0;
}

struct Result { double us; std::uint64_t hits; };
struct Allocations { std::size_t allocations = 0, frees = 0, bytes = 0; };
template<class T> struct CountingAllocator {
    using value_type = T;
    Allocations* counts;
    T* allocate(std::size_t n) { ++counts->allocations; counts->bytes += n * sizeof(T); return std::allocator<T>{}.allocate(n); }
    void deallocate(T* p, std::size_t n) { ++counts->frees; std::allocator<T>{}.deallocate(p, n); }
    bool operator==(const CountingAllocator& other) const { return counts == other.counts; }
};
// Allocation model uses the same reserve/append/clear operations as the tile lists.
// This isolates allocator calls; it does not measure whole-game heap activity.
static void allocation_benchmark() {
    using List = std::vector<const unsigned*, CountingAllocator<const unsigned*>>;
    const std::array<unsigned, 3> sizes{256, 384, 192};
    unsigned source = 0;
    Allocations before, after;
    for (unsigned frame = 0; frame < 1000; ++frame) {
        for (auto size : sizes) {
            List list{CountingAllocator<const unsigned*>{&before}};
            list.reserve(size);
            for (unsigned i = 0; i < size; ++i) list.push_back(&source);
            source += unsigned(list.size());
        }
    }
    std::size_t retained = 0;
    {
        std::array<List, 3> lists{List{CountingAllocator<const unsigned*>{&after}},
                                 List{CountingAllocator<const unsigned*>{&after}},
                                 List{CountingAllocator<const unsigned*>{&after}}};
        for (unsigned frame = 0; frame < 1000; ++frame) {
            for (unsigned layer = 0; layer < 3; ++layer) {
                auto& list = lists[layer]; list.clear(); list.reserve(sizes[layer]);
                for (unsigned i = 0; i < sizes[layer]; ++i) list.push_back(&source);
                source += unsigned(list.size()); list.clear();
            }
        }
        for (const auto& list : lists) retained += list.capacity() * sizeof(const unsigned*);
    }
    std::printf("\"tile_list_allocation_model\":{\"frames\":1000,\"baseline_allocations\":%zu,\"baseline_frees\":%zu,"
                "\"optimized_allocations\":%zu,\"optimized_frees_after_shutdown\":%zu,\"retained_bytes\":%zu,\"checksum\":%u},",
                before.allocations, before.frees, after.allocations, after.frees, retained, source);
}
static Result run(const std::vector<Bounds>& surfaces, const Index* index) {
    const auto start = std::chrono::steady_clock::now();
    std::uint64_t hits = 0;
    // Same circle and frustum test as request selection; a deterministic synthetic camera.
    float m[16]{}; m[0] = m[5] = 1; m[11] = -1;
    for (unsigned repeat = 0; repeat < 4; ++repeat) {
        for (float size : {256.0f, 1024.0f, 4096.0f}) {
            for (int z = -22; z <= 22; ++z) for (int x = -22; x <= 22; ++x) {
                const float px = x * size, pz = z * size;
                const Bounds box{px, px + size, pz, pz + size};
                const auto visible = [&](unsigned) {
                    return midnafx::render::grass_view::visible(m, {px, 0, pz}, {px + size, 40, pz + size});
                };
                bool hit = false;
                if (index) hit = index->any(box, visible);
                else hit = std::any_of(surfaces.begin(), surfaces.end(), [&](const auto& surface) {
                    return surface.overlaps(box) && visible(0);
                });
                hits += hit;
            }
        }
    }
    return {std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count(), hits};
}
int main(int argc, char** argv) {
    if (correctness()) return 1;
    if (argc < 2 || std::string_view(argv[1]) != "--benchmark") return 0;
    std::puts("{\"schema\":1,\"units\":\"cpu_microseconds_per_24300_queries\",");
    allocation_benchmark();
    std::puts("\"workloads\":[");
    unsigned workload = 0;
    for (unsigned count : {8u, 8192u, 8192u}) {
        const bool overlapping = workload == 2;
        auto surfaces = terrain(count, overlapping);
        const auto build_start = std::chrono::steady_clock::now();
        auto index = make_index(surfaces);
        const auto build_us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - build_start).count();
        run(surfaces, nullptr); run(surfaces, &index); // Warm both paths.
        std::printf("%s{\"scenario\":\"%s\",\"surfaces\":%u,\"index_bytes\":%zu,\"build_us\":%.3f,\"trials\":[",
                    workload ? "," : "", overlapping ? "overlapping" : count == 8 ? "tiny" : "spread", count, index.bytes(), build_us);
        for (unsigned trial = 0; trial < 12; ++trial) {
            Result before{}, after{};
            if (trial % 2) { after = run(surfaces, &index); before = run(surfaces, nullptr); }
            else { before = run(surfaces, nullptr); after = run(surfaces, &index); }
            CHECK(before.hits == after.hits);
            std::printf("%s{\"order\":\"%s\",\"baseline_us\":%.3f,\"optimized_us\":%.3f,\"hits\":%llu}",
                trial ? "," : "", trial % 2 ? "BA" : "AB", before.us, after.us,
                static_cast<unsigned long long>(before.hits));
        }
        std::puts("]}"); ++workload;
    }
    std::puts("]}");
}
