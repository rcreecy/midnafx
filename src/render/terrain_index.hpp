#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace midnafx::render {
// Mod-owned bounds and integer indices only; no game-resource pointers.
class TerrainIndex {
public:
    struct Bounds {
        float min_x, max_x, min_z, max_z;
        bool overlaps(const Bounds& other) const {
            return max_x >= other.min_x && min_x <= other.max_x &&
                   max_z >= other.min_z && min_z <= other.max_z;
        }
    };
private:
    struct Node { Bounds bounds; std::uint32_t first, count, right; };
    std::vector<Bounds> bounds_;
    std::vector<std::uint32_t> order_;
    std::vector<Node> nodes_;
    bool linear_ = false;

    std::uint32_t split(std::uint32_t first, std::uint32_t count) {
        Bounds box{std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
                   std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()};
        for (auto i = first; i < first + count; ++i) {
            const auto& b = bounds_[order_[i]];
            box.min_x = std::min(box.min_x, b.min_x); box.max_x = std::max(box.max_x, b.max_x);
            box.min_z = std::min(box.min_z, b.min_z); box.max_z = std::max(box.max_z, b.max_z);
        }
        const auto node = static_cast<std::uint32_t>(nodes_.size());
        nodes_.push_back({box, first, count, 0});
        if (count <= 8) return node;
        const auto middle = first + count / 2;
        const bool x = box.max_x - box.min_x >= box.max_z - box.min_z;
        std::nth_element(order_.begin() + first, order_.begin() + middle, order_.begin() + first + count,
            [&](auto a, auto b) {
                const auto& p = bounds_[a]; const auto& q = bounds_[b];
                // Halving first avoids overflow for large finite coordinates.
                return x ? p.min_x * 0.5f + p.max_x * 0.5f < q.min_x * 0.5f + q.max_x * 0.5f
                         : p.min_z * 0.5f + p.max_z * 0.5f < q.min_z * 0.5f + q.max_z * 0.5f;
            });
        split(first, middle - first); // Left child is adjacent to its parent.
        const auto right = split(middle, first + count - middle);
        nodes_[node].count = 0; nodes_[node].right = right;
        return node;
    }
    template<class Predicate> bool query(std::uint32_t index, const Bounds& box, Predicate& predicate) const {
        const auto& node = nodes_[index];
        if (!node.bounds.overlaps(box)) return false;
        if (!node.count) return query(index + 1, box, predicate) || query(node.right, box, predicate);
        for (auto i = node.first; i < node.first + node.count; ++i) {
            const auto surface = order_[i];
            if (bounds_[surface].overlaps(box) && predicate(surface)) return true;
        }
        return false;
    }
public:
    template<class GetBounds> void rebuild(std::uint32_t count, GetBounds get) {
        bounds_.resize(count); order_.resize(count); nodes_.clear(); linear_ = false;
        for (std::uint32_t i = 0; i < count; ++i) {
            bounds_[i] = get(i); order_[i] = i;
            const auto& b = bounds_[i];
            linear_ |= !std::isfinite(b.min_x) || !std::isfinite(b.max_x) ||
                       !std::isfinite(b.min_z) || !std::isfinite(b.max_z) ||
                       b.min_x > b.max_x || b.min_z > b.max_z;
        }
        if (count && !linear_) split(0, count);
    }
    // Predicate must be pure: traversal order differs from the source order.
    template<class Predicate> bool any(const Bounds& box, Predicate predicate) const {
        if (linear_) {
            for (std::uint32_t i = 0; i < bounds_.size(); ++i)
                if (bounds_[i].overlaps(box) && predicate(i)) return true;
            return false;
        }
        return !nodes_.empty() && query(0, box, predicate);
    }
    std::size_t bytes() const {
        return bounds_.capacity() * sizeof(Bounds) + order_.capacity() * sizeof(std::uint32_t) +
               nodes_.capacity() * sizeof(Node);
    }
};
} // namespace midnafx::render
