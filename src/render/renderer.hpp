#pragma once
#include <cstdint>

namespace midnafx::render {
struct Diagnostics {
    const char* status;
    std::uint32_t width, height;
    std::uint64_t submitted_draws, encoded_draws, bind_groups, pipeline_builds;
    double callback_us;
    bool timing_enabled;
    const char* backend;
};
void initialize();
void update();
void shutdown();
Diagnostics diagnostics();
} // namespace midnafx::render
