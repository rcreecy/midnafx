#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <span>

namespace midnafx::render::airborne_sources {
// Cache values only. Never retain a resource/model/terrain pointer.
template<class Source, std::size_t Capacity = 64> class Cache {
    struct Key {
        std::array<std::uint32_t, 19> camera{};
        std::uint32_t revision = 0;
        std::size_t requested = 0;
        bool operator==(const Key&) const = default;
    };
    Key previous{};
    std::array<Source, Capacity> values{};
    unsigned size = 0;
    bool valid = false;
    template<class Camera> static Key key(const Camera& camera, std::uint32_t revision, std::size_t requested) {
        Key result;
        unsigned i = 0;
        for (float value : camera.proj_from_world) result.camera[i++] = std::bit_cast<std::uint32_t>(value);
        for (float value : camera.eye) result.camera[i++] = std::bit_cast<std::uint32_t>(value);
        result.revision = revision; result.requested = requested;
        return result;
    }
public:
    void reset() { valid = false; }
    template<class Camera> bool load(const Camera& camera, std::uint32_t revision, std::span<Source> output, unsigned& count) const {
        if (!valid || output.size() > Capacity || key(camera, revision, output.size()) != previous) return false;
        std::copy_n(values.begin(), size, output.begin()); count = size;
        return true;
    }
    template<class Camera> void store(const Camera& camera, std::uint32_t revision, std::size_t requested, std::span<const Source> sources) {
        if (requested > Capacity || sources.size() > requested) { reset(); return; }
        previous = key(camera, revision, requested);
        std::copy(sources.begin(), sources.end(), values.begin());
        size = static_cast<unsigned>(sources.size()); valid = true;
    }
};
} // namespace midnafx::render::airborne_sources
