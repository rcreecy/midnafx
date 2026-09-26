#pragma once

#include "grade.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace midnafx::presets {
constexpr std::size_t Maximum = 16;
struct Snapshot {
    std::array<std::int64_t, grade::Count> values{0, 0, 100, 100, 100, 0, 0, 0};
    std::array<bool, grade::Count> active{true, true, true, true, true, true, true, true};
    bool detail_enabled = false;
    std::int64_t detail_strength = 20;
};
struct Entry {
    std::string name;
    Snapshot snapshot;
};
bool valid_name(const std::string& name);
std::string encode(const std::vector<Entry>& entries);
bool decode(const std::string& text, std::vector<Entry>& output);
Snapshot smoke_test();
Snapshot vivid_realism();
} // namespace midnafx::presets
