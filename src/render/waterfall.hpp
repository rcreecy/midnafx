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
    std::vector<std::array<Point, 2>> edges;
    Point foot;
    Point across;
    float width;
    float height;
    float bottom;
};

void collect(std::span<const Triangle> sheet, std::vector<Fall>& falls,
             std::span<const Triangle> basin = {});
void initialize();
void shutdown();
struct FrameClip {
    float left, top, right, bottom;
};
void draw(std::span<const Fall* const> falls, const CameraInfo& camera, const FrameClip* contact_clip = nullptr);
}
