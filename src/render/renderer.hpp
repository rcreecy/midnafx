#pragma once
#include <cstdint>

namespace midnafx::render {
struct Diagnostics {
    const char* status;
    std::uint32_t width, height;
    std::uint64_t submitted_draws, encoded_draws, bind_groups, pipeline_builds;
    std::uint64_t disabled_samples, neutral_samples, snapshot_requests;
    double callback_us, disabled_us, layout_us, resolve_us;
    double active_p50_us, active_p95_us, disabled_p50_us, disabled_p95_us;
    bool timing_enabled;
    const char* backend;
};
void initialize();
void update();
void shutdown();
void reset_timing_samples();
std::uint64_t timing_sample_count();
Diagnostics diagnostics();
} // namespace midnafx::render
