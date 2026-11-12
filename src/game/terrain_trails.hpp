#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace midnafx::terrain_trails {
using Sources = std::array<std::array<float,4>,2>;
class History {
    static constexpr unsigned PerActor = 16;
    struct Entry {
        std::array<float,4> begin{}, end{};
        float age=0, lifetime=0;
    };
    struct Track {
        std::array<float,4> previous{}, anchor{};
        float elapsed=0;
        unsigned cursor=0;
        bool valid=false;
    };
    std::array<Entry,PerActor*2> entries{};
    std::array<Track,2> tracks{};
    std::array<std::array<float,3>,2> low{}, high{};
    unsigned count=0;
    static float smooth(float value) {
        value=std::clamp(value,0.0f,1.0f);
        return value*value*(3-2*value);
    }
    void clear_actor(unsigned actor) {
        tracks[actor]={};
        for (unsigned i=0;i<PerActor;++i) entries[actor*PerActor+i]={};
    }
public:
    void clear() { entries={}; tracks={}; count=0; }
    unsigned active() const { return count; }
    void update(const Sources& sources, float elapsed) {
        if (!std::isfinite(elapsed) || elapsed<0 || elapsed>0.5f) { clear(); return; }
        if (elapsed==0) return;
        for (auto& entry:entries) {
            entry.age+=elapsed;
            if (entry.age>=entry.lifetime) entry.lifetime=0;
        }
        for (unsigned actor=0;actor<2;++actor) {
            const auto& source=sources[actor];
            if (source[3]<=0 || source[3]>300 ||
                !std::all_of(source.begin(),source.end(),[](float x){ return std::isfinite(x) && std::abs(x)<1e7f; })) {
                clear_actor(actor); continue;
            }
            auto& track=tracks[actor];
            if (track.valid) {
                const float dx=source[0]-track.previous[0], dz=source[2]-track.previous[2];
                const float jump=std::max(128.0f,elapsed*2400);
                if (dx*dx+dz*dz>jump*jump || std::abs(source[1]-track.previous[1])>120) clear_actor(actor);
            }
            if (!track.valid) {
                track.valid=true; track.anchor=track.previous=source; continue;
            }
            track.elapsed+=elapsed;
            const float dx=source[0]-track.anchor[0], dz=source[2]-track.anchor[2];
            if (track.elapsed>=0.18f && dx*dx+dz*dz>=64) {
                auto& entry=entries[actor*PerActor+track.cursor];
                entry={track.anchor,source,0,actor==0 ? 3.0f : 3.8f};
                track.cursor=(track.cursor+1)%PerActor;
                track.anchor=source; track.elapsed=0;
            } else if (dx*dx+dz*dz<1) {
                // Standing still keeps the live contact response, not a fresh trail.
                track.anchor=source; track.elapsed=0;
            }
            track.previous=source;
        }
        count=0;
        for (auto& box:low) box.fill(1e9f);
        for (auto& box:high) box.fill(-1e9f);
        for (unsigned index=0;index<entries.size();++index) if (entries[index].lifetime>0) {
            const auto& entry=entries[index];
            const unsigned actor=index/PerActor;
            ++count;
            for (unsigned axis=0;axis<3;++axis) {
                const float radius=axis==1 ? 85.0f : entry.end[3]*0.82f;
                low[actor][axis]=std::min(low[actor][axis],std::min(entry.begin[axis],entry.end[axis])-radius);
                high[actor][axis]=std::max(high[actor][axis],std::max(entry.begin[axis],entry.end[axis])+radius);
            }
        }
    }
    // XY is weighted horizontal bend direction; Z is the recovery strength.
    std::array<float,3> sample(const std::array<float,3>& point) const {
        std::array<float,3> result{};
        if (!count) return result;
        for (const float value:point) if (!std::isfinite(value)) return result;
        for (unsigned actor=0;actor<2;++actor) {
            bool outside=false;
            for (unsigned axis=0;axis<3;++axis)
                outside |= point[axis]<low[actor][axis] || point[axis]>high[actor][axis];
            if (outside) continue;
            for (unsigned index=actor*PerActor;index<(actor+1)*PerActor;++index) {
                const auto& entry=entries[index];
                if (entry.lifetime<=0) continue;
                const float dx=entry.end[0]-entry.begin[0], dz=entry.end[2]-entry.begin[2];
                const float length2=dx*dx+dz*dz;
                const float along=std::clamp(((point[0]-entry.begin[0])*dx+(point[2]-entry.begin[2])*dz)/std::max(length2,1.0f),0.0f,1.0f);
                const float x=point[0]-entry.begin[0]-dx*along, z=point[2]-entry.begin[2]-dz*along;
                const float radius=entry.end[3]*0.82f;
                const float distance2=x*x+z*z;
                if (distance2>=radius*radius) continue;
                const float vertical=std::abs(point[1]-std::lerp(entry.begin[1],entry.end[1],along));
                const float weight=(1-smooth(std::sqrt(distance2)/radius)) * (1-smooth(vertical/85)) *
                                   (1-smooth(entry.age/entry.lifetime));
                if (weight<=result[2]) continue;
                const float inverse=1/std::sqrt(std::max(length2,1.0f));
                result={dx*inverse*weight,dz*inverse*weight,weight};
            }
        }
        return result;
    }
};
}
