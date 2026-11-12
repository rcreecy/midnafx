#include "render/surface_map_cache.hpp"
#include <vector>
#define CHECK(c) if (!(c)) return __LINE__
int main() {
    aurora::gfx::surface_maps::Cache<int> cache;
    auto a=std::make_shared<int>(1), b=std::make_shared<int>(2), c=std::make_shared<int>(3);
    auto first=cache.request(a,1), second=cache.request(b,1);
    CHECK(first.created && second.created && first.layer!=second.layer);
    CHECK(first.layer>0 && first.layer<64);
    CHECK(cache.request(c,1).layer==0);
    auto reuse=cache.request(a,1); CHECK(!reuse.created && reuse.layer==first.layer);
    CHECK(cache.request(c,2).created);
    cache.clear();
    std::vector<std::shared_ptr<int>> held;
    for (unsigned i=0;i<63;++i) {
        held.push_back(std::make_shared<int>(i));
        CHECK(cache.request(held.back(),i/2+1).created);
    }
    // Every live layer is pinned in the current frame; pressure must fall back.
    for (const auto& item:held) CHECK(cache.request(item,100).layer>0);
    CHECK(cache.request(a,100).layer==0);
    CHECK(cache.request(a,101).created);
    // Reused pointer addresses cannot revive a dead weak texture identity.
    cache.clear(); auto old=cache.request(a,1); cache.cancel(old.layer,a);
    CHECK(cache.request(a,2).created);
    cache.clear(); CHECK(cache.request({},1).layer==0);
}
