#pragma once

#include "game/topology.hpp"
#include <array>
#include <span>
#include <vector>

struct CameraInfo;

namespace midnafx::render::waterfall {
using Point = topology::Vec3;
using Triangle = std::array<Point, 3>;

struct Fall {
    std::vector<Triangle> sheet;
    Point foot;
    Point across;
    float width;
    float height;
    float bottom;
};

void collect(std::span<const Triangle> sheet, std::vector<Fall>& falls);
void initialize();
void shutdown();
void draw(std::span<const Fall* const> falls, const CameraInfo& camera);
}
