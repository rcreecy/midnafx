#pragma once
#include <cstdint>

namespace midnafx::geometry_probe {
struct Summary {
    std::uint32_t examined = 0, applied = 0, skipped = 0;
    const char* classification = "Not processed";
    const char* reason = "Load a scene with model shading enabled";
    bool known_good = false;
};
Summary summary();
void initialize();
void shutdown();
void restore_mutation();
void restore_smoothing(bool skinned);
} // namespace midnafx::geometry_probe
