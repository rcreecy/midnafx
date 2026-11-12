#include "render/material_relighting.hpp"
#include "render/native_material_service.hpp"
#include "services.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>

namespace {
bool enabled = false, relief_active = false;
unsigned requests = 0, relief_requests = 0;
unsigned profile_requests=0, last_profile=0;
ModResult profile(ModContext*,uint32_t value) {
    ++profile_requests; last_profile=value; return value<=6 ? MOD_OK : MOD_INVALID_ARGUMENT;
}
ModResult result = MOD_OK;
void quiet(ModContext*, const char*) {}
const LogService logs{.info = quiet, .warn = quiet};
ModResult relighting(ModContext*, uint32_t value) {
    ++requests;
    if (result == MOD_OK && !value) relief_active = false;
    return result;
}
ModResult relief(ModContext*, uint32_t value) {
    ++relief_requests;
    relief_active = value != 0;
    return MOD_OK;
}
void require(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}
}
ModContext* mod_ctx = nullptr;
extern "C" {
const NativeMaterialService* svc_native_material = nullptr;
const LogService* svc_log = &logs;
}
namespace midnafx::settings { bool material_relighting_enabled() { return enabled; } }

int main() {
    namespace material = midnafx::render::material_relighting;
    require(!material::available(), "Missing host must bypass relighting");
    NativeMaterialService service{
        .header = {offsetof(NativeMaterialService, set_surface_relief), 1, 0},
        .set_relighting = relighting, .set_surface_relief = relief};
    svc_native_material = &service;
    require(material::available() && !material::surface_relief_available(),
            "Service 1.0 must retain relighting without accessing the appended field");
    enabled = true;
    material::update(); material::update();
    require(requests == 1 && relief_requests == 0, "Legacy enable must not request relief or repeat");
    material::shutdown();
    require(requests == 2, "Legacy shutdown must restore native rendering");

    service.header = {sizeof(service), 1, 1};
    require(material::surface_relief_available(), "Complete service 1.1 must expose relief");
    requests = relief_requests = 0;
    material::update(); material::update();
    require(requests == 1 && relief_requests == 1 && relief_active, "Enable must request relief exactly once");
    enabled = false;
    material::update();
    require(requests == 2 && !relief_active, "Disable must release the combined native scope");
    material::shutdown();

    service.header.struct_size = offsetof(NativeMaterialService, set_surface_relief);
    require(material::available() && !material::surface_relief_available(), "Minor version alone cannot authorize an absent field");
    service.header.struct_size = sizeof(service);
    service.set_surface_relief = nullptr;
    require(material::available() && !material::surface_relief_available(), "Missing optional callback must retain ordinary relighting");
    service.set_surface_relief = relief;
    requests = relief_requests = 0;
    enabled = true; result = MOD_CONFLICT;
    material::update();
    require(requests == 1 && relief_requests == 0, "An ownership conflict must not request relief");
    result = MOD_OK;
    material::update();
    require(requests == 2 && relief_requests == 1, "A rejected request must retry after ownership clears");
    material::shutdown();
    require(!relief_active, "Shutdown must release relief ownership");
    require(!material::surface_profiles_available(),"Service 1.1 must not access a profile callback");
    service.header.minor_version=2;
    require(!material::surface_profiles_available(),"Null profile callback must bypass finishes");
    service.set_surface_profile=profile;
    service.header.struct_size=offsetof(NativeMaterialService,set_surface_profile);
    require(!material::surface_profiles_available(),"Truncated service 1.2 must bypass finishes");
    service.header.struct_size=sizeof(service);
    require(material::surface_profiles_available(),"Complete service 1.2 must expose profiles");
    require(material::set_surface_profile(4) && last_profile==4,"Metal profile must reach host");
    require(material::set_surface_profile(0) && last_profile==0,"Fallback profile must restore");
    require(!material::set_surface_profile(7) && profile_requests==3,"Invalid host profiles must fail");
    service.header.struct_size = offsetof(NativeMaterialService, set_relighting);
    require(!material::available(), "Truncated service must bypass callbacks");
    std::puts("Native material legacy and extended capability checks pass");
}
