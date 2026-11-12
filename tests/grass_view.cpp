#include "render/grass_view.hpp"
#include <limits>
#define CHECK(condition) if (!(condition)) return __LINE__
int main() {
    using namespace midnafx::render::grass_view;
    CHECK(reach(60000,90000,60,1.8f) > 60000);
    CHECK(reach(60000,35000,60,1.8f) == 35000);
    CHECK(reach(500,10000,60,1.8f) == 2200);
    CHECK(std::isfinite(reach(std::numeric_limits<float>::quiet_NaN(),90000,0,0)));
    for (float radius : {2200.0f,10000.0f,60000.0f,2000000.0f}) {
        CHECK(tile_size(radius) >= radius/12);
        CHECK(radius/tile_size(radius) <= 12);
    }
    float m[16]{}; m[0]=m[5]=1; m[11]=-1;
    CHECK(visible(m,{-1,-1,-10},{1,1,-8}));
    CHECK(!visible(m,{20,-1,-10},{30,1,-8}));
    CHECK(!visible(m,{-1,-1,8},{1,1,10}));
    CHECK(visible(m,{-1,-1,-2},{1,1,2}));
    CHECK(visible(m,{10,-1,-10},{11,1,-9}));
}
