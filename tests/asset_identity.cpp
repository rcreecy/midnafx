#include "game/asset_identity.hpp"
int main() {
    using midnafx::asset_probe::texture_capacity;
    return texture_capacity(0x54564231)!=1 || texture_capacity(0x54564232)!=2 ||
        texture_capacity(0x54564234)!=4 || texture_capacity(0x54563136)!=8 ||
        texture_capacity(0x54565054)!=8 || texture_capacity(0x54564e4c)!=0 || texture_capacity(0)!=0;
}
