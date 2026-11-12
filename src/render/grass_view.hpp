#pragma once
#include <algorithm>
#include <array>
#include <cmath>
namespace midnafx::render::grass_view {
inline float reach(float far_plane, float furthest, float fovy, float aspect) {
    if (!std::isfinite(far_plane) || far_plane <= 0) far_plane = 10000;
    if (!std::isfinite(fovy) || fovy <= 0 || fovy >= 170) fovy = 60;
    if (!std::isfinite(aspect) || aspect <= 0 || aspect > 10) aspect = 1.8f;
    const float tangent = std::tan(fovy * 0.00872664626f);
    const float diagonal = std::sqrt(1+tangent*tangent*(1+aspect*aspect));
    return std::clamp(std::min(far_plane*diagonal, furthest), 2200.0f, 2000000.0f);
}
inline float tile_size(float reach) {
    float size = 2048;
    while (size < reach/12) size *= 2;
    return size;
}
inline bool visible(const float* m, const std::array<float,3>& low, const std::array<float,3>& high) {
    unsigned outside[5]{};
    for (unsigned corner = 0; corner < 8; ++corner) {
        const float x = corner&1 ? high[0] : low[0], y = corner&2 ? high[1] : low[1],
                    z = corner&4 ? high[2] : low[2];
        const float w = m[3]*x+m[7]*y+m[11]*z+m[15];
        const float cx = m[0]*x+m[4]*y+m[8]*z+m[12], cy = m[1]*x+m[5]*y+m[9]*z+m[13];
        outside[0] += w <= 0;
        outside[1] += cx < -w*1.15f; outside[2] += cx > w*1.15f;
        outside[3] += cy < -w*1.15f; outside[4] += cy > w*1.15f;
    }
    return std::none_of(std::begin(outside), std::end(outside), [](unsigned n) { return n == 8; });
}
}
