#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <limits>
namespace aurora::gfx::surface_maps {
// Layer zero is neutral. Never replace a layer already used in this frame.
template <class T> class Cache {
    struct Slot { std::weak_ptr<T> source; std::uint64_t frame=0; bool occupied=false; };
    std::array<Slot,64> slots{};
    std::uint64_t observed=std::numeric_limits<std::uint64_t>::max();
    unsigned generated=0;
public:
    struct Request { unsigned layer=0; bool created=false; };
    Request request(const std::shared_ptr<T>& source,std::uint64_t frame) {
        if (!source) return {};
        if (observed!=frame) { observed=frame; generated=0; }
        unsigned candidate=0;
        for (unsigned i=1;i<slots.size();++i) {
            const auto current=slots[i].source.lock();
            if (current && current.get()==source.get()) { slots[i].frame=frame; return {i,false}; }
            if (slots[i].occupied && slots[i].frame==frame) continue;
            if (!candidate || !slots[i].occupied || slots[i].frame<slots[candidate].frame) candidate=i;
        }
        if (!candidate || generated>=2) return {};
        ++generated; slots[candidate]={source,frame,true};
        return {candidate,true};
    }
    void cancel(unsigned layer,const std::shared_ptr<T>& source) {
        if (layer && layer<slots.size() && slots[layer].source.lock()==source) slots[layer]={};
    }
    void clear() { slots={}; observed=std::numeric_limits<std::uint64_t>::max(); generated=0; }
};
}
