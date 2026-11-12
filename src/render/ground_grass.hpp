#pragma once
#include <array>
#include <span>

struct CameraInfo;
namespace midnafx::render::ground_grass {
struct AirborneSource { std::array<float, 3> point; bool forest; };
unsigned airborne_sources(const CameraInfo& camera, std::span<AirborneSource> output);
void nearby_spray(const float eye[3], std::span<std::array<float, 4>> sources);
void initialize();
void update(float elapsed);
void shutdown();
}
