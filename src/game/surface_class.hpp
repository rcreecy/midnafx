#pragma once
#include <array>
#include <string_view>
namespace midnafx::surface_class {
enum Kind : unsigned { Unknown, Rock, Wood, Cloth, Metal, Wet, Leather, Count };
inline unsigned evidence(std::string_view name) {
    unsigned mask=0;
    const auto word=[&](std::string_view token) {
        const auto starts=[&](std::string_view value) { return token.starts_with(value); };
        if (starts("rock") || starts("stone") || starts("brick") || starts("iwa") || starts("ishi")) mask|=1u<<Rock;
        if (starts("wood") || starts("bark") || starts("trunk") || starts("plank") || starts("miki") ||
            starts("nenrin") || starts("kibako") || starts("kihasira") || starts("kirikabu") ||
            token=="ita" || token=="fita" || starts("mokume")) mask|=1u<<Wood;
        if (starts("cloth") || starts("fabric") || starts("linen") || starts("nuno") || starts("fuku") ||
            token=="pants" || token=="skirt" || token=="shirt" || token=="tunic" || token=="garment") mask|=1u<<Cloth;
        if (starts("metal") || starts("iron") || starts("steel") || starts("silver") || starts("gold") ||
            starts("copper") || starts("kinzoku") || starts("tetsu") || token=="earring" || token=="buckle") mask|=1u<<Metal;
        if (token=="wet" || starts("nure")) mask|=1u<<Wet;
        if (starts("leather") || token=="belt" || token=="belts" || token=="boot" || token=="boots") mask|=1u<<Leather;
    };
    std::array<char,128> token{};
    unsigned size=0;
    bool previous_lower=false;
    for (unsigned i=0;i<name.size() && i<256;++i) {
        const char c=name[i];
        const bool upper=c>='A' && c<='Z', lower=c>='a' && c<='z';
        if ((!upper && !lower) || (upper && previous_lower)) {
            if (size) word({token.data(),size});
            size=0;
        }
        if ((upper || lower) && size<token.size()) token[size++]=upper ? char(c+32) : c;
        previous_lower=lower;
    }
    if (size) word({token.data(),size});
    return mask;
}
inline Kind classify(std::string_view material, std::string_view texture) {
    const unsigned mask=evidence(material)|evidence(texture);
    if (mask&(1u<<Wet)) return Wet;
    for (unsigned kind=1;kind<Count;++kind) if (mask==(1u<<kind)) return Kind(kind);
    return Unknown;
}
inline constexpr std::array<const char*,Count> Labels{"fallback","rock","wood","cloth","metal","wet","leather"};
}
