#pragma once
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace midnafx::render::grass_assembly {
struct Key {
    std::array<std::uint32_t, 39> camera{};
    std::uint32_t revision = 0;
    unsigned first_room = 0;
    bool operator==(const Key&) const = default;
};
template<class Camera> Key key(const Camera& camera, float canopy_reach, std::uint32_t revision, unsigned first_room) {
    Key result;
    unsigned i = 0;
    const auto append = [&](float value) { result.camera[i++] = std::bit_cast<std::uint32_t>(value); };
    for (float value : camera.proj_from_world) append(value);
    for (float value : camera.view_from_world) append(value);
    for (float value : camera.eye) append(value);
    append(camera.fovy); append(camera.aspect); append(camera.far_plane); append(canopy_reach);
    result.revision = revision; result.first_room = first_room;
    return result;
}
class Cache {
    Key previous{};
    bool valid = false;
public:
    unsigned near_count = 0;
    std::size_t plant_start = 0;
    bool reusable(const Key& next, bool ready) const { return valid && ready && next == previous; }
    void remember(const Key& next, bool ready, unsigned near, std::size_t plants) {
        previous = next; valid = ready; near_count = near; plant_start = plants;
    }
    // Only near grass and low plants carry CPU trail response. Geometry and
    // ordering stay untouched; native actor contact and wind are separate uniforms.
    template<class Instances, class Sample> unsigned refresh(Instances& instances, Sample sample, bool* changed = nullptr) const {
        unsigned trampled = 0;
        const auto update = [&](std::size_t i) {
            const auto next = sample(instances[i].point);
            if (changed) {
                const auto& previous = instances[i].trail;
                *changed |= std::bit_cast<std::uint32_t>(previous.x) != std::bit_cast<std::uint32_t>(next.x) ||
                            std::bit_cast<std::uint32_t>(previous.y) != std::bit_cast<std::uint32_t>(next.y) ||
                            std::bit_cast<std::uint32_t>(previous.z) != std::bit_cast<std::uint32_t>(next.z);
            }
            instances[i].trail = next;
            trampled += instances[i].trail.z > 0.05f;
        };
        for (unsigned i = 0; i < near_count; ++i) update(i);
        for (std::size_t i = plant_start; i < instances.size(); ++i) update(i);
        return trampled;
    }
    void reset() { valid = false; }
};
} // namespace midnafx::render::grass_assembly
