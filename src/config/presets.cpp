#include "presets.hpp"

#include <algorithm>
#include <charconv>
#include <limits>
#include <string_view>

namespace midnafx::presets {
namespace {
constexpr std::array<std::int64_t, grade::Count> minimum{-200, 0, 50, 70, 0, 0, -100, -100};
constexpr std::array<std::int64_t, grade::Count> maximum{200, 20, 150, 150, 200, 100, 100, 100};
bool number(std::string_view text, std::int64_t& result) {
    if (text.empty())
        return false;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}
bool parse_row(std::string_view row, unsigned version, Entry& entry) {
    if (version == 3) {
        const auto last = row.rfind(',');
        if (last == row.npos || !number(row.substr(last + 1), entry.snapshot.product_flags) ||
            entry.snapshot.product_flags < -1 || entry.snapshot.product_flags > 63)
            return false;
        row = row.substr(0, last);
    }
    const auto tab = row.find('\t');
    if (tab == row.npos)
        return false;
    entry.name = row.substr(0, tab);
    if (!valid_name(entry.name))
        return false;
    row.remove_prefix(tab + 1);
    for (unsigned i = 0; i < grade::Count; ++i) {
        const auto comma = row.find(',');
        if (comma == row.npos)
            return false;
        if (!number(row.substr(0, comma), entry.snapshot.values[i]) ||
            entry.snapshot.values[i] < minimum[i] || entry.snapshot.values[i] > maximum[i])
            return false;
        row.remove_prefix(comma + 1);
    }
    std::int64_t mask = 0;
    if (version >= 2) {
        const auto first = row.find(',');
        if (first == row.npos || !number(row.substr(0, first), mask))
            return false;
        row.remove_prefix(first + 1);
        const auto second = row.find(',');
        std::int64_t detail_on = 0;
        if (second == row.npos || !number(row.substr(0, second), detail_on) ||
            (detail_on != 0 && detail_on != 1))
            return false;
        entry.snapshot.detail_enabled = detail_on == 1;
        row.remove_prefix(second + 1);
        if (!number(row, entry.snapshot.detail_strength) || entry.snapshot.detail_strength < 0 ||
            entry.snapshot.detail_strength > 50)
            return false;
    } else if (!number(row, mask))
        return false;
    if (mask < 0 || mask > 255)
        return false;
    for (unsigned i = 0; i < grade::Count; ++i)
        entry.snapshot.active[i] = (mask & (std::int64_t{1} << i)) != 0;
    return true;
}
} // namespace

bool valid_name(const std::string& name) {
    if (name.empty() || name.size() > 32 || name == "Vanilla" || name == "Custom")
        return false;
    for (unsigned char c : name)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == ' ' || c == '_' || c == '-'))
            return false;
    return true;
}

std::string encode(const std::vector<Entry>& entries) {
    if (entries.size() > Maximum)
        return {};
    const bool product = std::any_of(entries.begin(), entries.end(), [](const Entry& entry) {
        return entry.snapshot.product_flags != -1;
    });
    std::string result = product ? "MFX3\n" : "MFX2\n";
    for (const auto& entry : entries) {
        if (!valid_name(entry.name))
            return {};
        result += entry.name;
        result += '\t';
        unsigned mask = 0;
        for (unsigned i = 0; i < grade::Count; ++i) {
            if (entry.snapshot.values[i] < minimum[i] || entry.snapshot.values[i] > maximum[i])
                return {};
            result += std::to_string(entry.snapshot.values[i]);
            result += ',';
            if (entry.snapshot.active[i])
                mask |= 1u << i;
        }
        result += std::to_string(mask);
        if (entry.snapshot.detail_strength < 0 || entry.snapshot.detail_strength > 50)
            return {};
        result += ',';
        result += entry.snapshot.detail_enabled ? '1' : '0';
        result += ',';
        result += std::to_string(entry.snapshot.detail_strength);
        if (entry.snapshot.product_flags < -1 || entry.snapshot.product_flags > 63)
            return {};
        if (product) {
            result += ',';
            result += std::to_string(entry.snapshot.product_flags);
        }
        result += '\n';
    }
    return result.size() <= 8192 ? result : std::string{};
}

bool decode(const std::string& text, std::vector<Entry>& output) {
    if (text.empty()) {
        output.clear();
        return true;
    }
    if (text.size() > 8192 || (text.substr(0, 5) != "MFX1\n" && text.substr(0, 5) != "MFX2\n" &&
                               text.substr(0, 5) != "MFX3\n"))
        return false;
    const unsigned version = text[3] - '0';
    std::vector<Entry> parsed;
    std::string_view remaining(text.data() + 5, text.size() - 5);
    while (!remaining.empty()) {
        const auto end = remaining.find('\n');
        if (end == remaining.npos || parsed.size() >= Maximum)
            return false;
        Entry next;
        if (!parse_row(remaining.substr(0, end), version, next))
            return false;
        for (const auto& prior : parsed)
            if (prior.name == next.name)
                return false;
        parsed.push_back(std::move(next));
        remaining.remove_prefix(end + 1);
    }
    output = std::move(parsed);
    return true;
}

Snapshot vivid_realism() {
    Snapshot result;
    // Preserve the black floor and neutral white balance. A small midtone lift,
    // restrained chroma boost and highlight shoulder retain the scene's lighting.
    result.values = {0, 0, 100, 102, 108, 25, 0, 0};
    result.detail_enabled = true;
    result.detail_strength = 12;
    return result;
}

Snapshot smoke_test() {
    Snapshot result;
    result.values = {120, 12, 145, 85, 0, 70, 85, -65};
    result.detail_enabled = true;
    result.detail_strength = 35;
    return result;
}

Snapshot vanilla_plus() {
    Snapshot result;
    result.values = {0, 0, 100, 101, 103, 10, 0, 0};
    result.detail_enabled = true;
    result.detail_strength = 8;
    result.product_flags = Grading;
    return result;
}
Snapshot enhanced() {
    auto result = vivid_realism();
    // Geometry and DOF remain explicit opt-ins until broad art validation.
    result.product_flags = Grading | Camera;
    return result;
}
} // namespace midnafx::presets
