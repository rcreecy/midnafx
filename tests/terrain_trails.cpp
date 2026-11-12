#include "game/terrain_trails.hpp"
#include <limits>
#define CHECK(c) if (!(c)) return __LINE__
int main() {
    using namespace midnafx::terrain_trails;
    History history;
    Sources actors{{{0,0,0,68},{0,0,0,0}}};
    history.update(actors,0.1f);
    CHECK(history.active()==0);
    for (unsigned i=1;i<=20;++i) { actors[0][0]=float(i)*10; history.update(actors,0.05f); }
    CHECK(history.active()>0 && history.active()<=32);
    const auto fresh=history.sample({100,0,0});
    CHECK(fresh[0]>0 && fresh[2]>0.5f && fresh[2]<=1);
    CHECK(history.sample({100,200,0})[2]==0);
    CHECK(history.sample({100,0,500})[2]==0);
    for (unsigned i=0;i<20;++i) history.update(actors,0.05f);
    CHECK(history.sample({100,0,0})[2]<fresh[2]);
    for (unsigned i=0;i<80;++i) history.update(actors,0.05f);
    CHECK(history.active()==0);
    // A teleport must never connect a long false trail.
    actors[0][0]=1000; history.update(actors,0.03f);
    CHECK(history.active()==0 && history.sample({500,0,0})[2]==0);
    for (unsigned i=0;i<200;++i) { actors[0][0]+=10; history.update(actors,0.05f); }
    CHECK(history.active()<=32);
    history.update(actors,std::numeric_limits<float>::quiet_NaN());
    CHECK(history.active()==0);
    // The wider second actor affects a wider corridor.
    actors={{{0,0,0,0},{0,0,0,145}}}; history.update(actors,0.1f);
    for (unsigned i=0;i<5;++i) { actors[1][0]+=10; history.update(actors,0.05f); }
    CHECK(history.sample({20,0,80})[2]>0);
    history.clear(); CHECK(history.active()==0);
}
