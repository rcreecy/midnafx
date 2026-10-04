// Local corpus runner. Input is generated from user-owned assets, never shipped.
#include "game/smoothing.hpp"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
    using namespace midnafx;
    try {
        if (argc != 2)
            return 2;
        std::ifstream input(argv[1], std::ios::binary);
        const auto read = [&](void* data, std::size_t bytes) {
            if (!input.read(static_cast<char*>(data), bytes))
                throw std::runtime_error("truncated fixture");
        };
        const auto u32 = [&]() {
            std::uint32_t n;
            read(&n, 4);
            return n;
        };
        char magic[8];
        read(magic, 8);
        if (std::memcmp(magic, "MFXQ0001", 8))
            return 2;
        const auto np = u32(), nn = u32(), ns = u32(), nf = u32(), ne = u32(), nj = u32(),
                   nd = u32();
        if (np > 65536 || nn > 65536 || ns > 4096 || nf > 32)
            return 2;
        std::vector<topology::Vec3> positions(np), normals(nn);
        read(positions.data(), np * sizeof(topology::Vec3));
        read(normals.data(), nn * sizeof(topology::Vec3));
        std::vector<topology::Format> formats(nf);
        for (auto& f : formats) {
            read(&f.id, 1);
            read(&f.components, 1);
            read(&f.type, 1);
        }
        std::vector<std::vector<topology::Attribute>> attrs(ns);
        std::vector<std::vector<topology::Group>> groups(ns);
        std::vector<std::vector<std::vector<std::uint8_t>>> lists(ns);
        std::vector<topology::Shape> shapes(ns);
        for (unsigned s = 0; s < ns; ++s) {
            const auto material = u32(), na = u32(), ng = u32();
            if (material > 65535 || na > 32 || ng > 4096)
                return 2;
            attrs[s].resize(na);
            groups[s].resize(ng);
            lists[s].resize(ng);
            for (auto& a : attrs[s]) {
                read(&a.id, 1);
                read(&a.kind, 1);
            }
            for (unsigned g = 0; g < ng; ++g) {
                const auto bytes = u32();
                if (bytes > 16 * 1024 * 1024)
                    return 2;
                lists[s][g].resize(bytes);
                read(lists[s][g].data(), bytes);
                groups[s][g] = {lists[s][g].data(), bytes};
            }
            shapes[s] = {attrs[s], groups[s], static_cast<std::uint16_t>(material)};
        }
        const auto begin = std::chrono::steady_clock::now();
        const auto mesh = topology::decode(shapes, formats, positions, nn);
        const auto decode_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                   std::chrono::steady_clock::now() - begin)
                                   .count();
        const auto result = smoothing::qualify(mesh, positions, normals,
                                               {true, ne == 0 && nj == 1 && nd == 1, false, false});
        std::printf(
            "{\"classification\":\"%s\",\"reason\":\"%s\",\"positions\":%u,\"normals\":%u,"
            "\"triangles\":%zu,\"envelopes\":%u,\"conflicts\":%u,\"changes\":%u,\"decodeUs\":%lld,"
            "\"classificationUs\":%llu,\"smoothingUs\":%llu,\"trackedClassifierBytes\":%llu}\n",
            smoothing::label(result.classification), result.reason, np, nn, mesh.triangles.size(),
            ne, result.smoothing.index_conflicts, result.smoothing.changed_indices,
            static_cast<long long>(decode_us),
            static_cast<unsigned long long>(result.classification_us),
            static_cast<unsigned long long>(result.smoothing_us),
            static_cast<unsigned long long>(result.working_vector_bytes));
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 2;
    }
}
