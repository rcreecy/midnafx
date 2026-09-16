#include "render/renderer.hpp"
#include "services.hpp"
#include "ui/settings.hpp"
#include <mods/svc/gfx.h>

DEFINE_MOD();
IMPORT_OPTIONAL_SERVICE_VERSION(GfxService, svc_gfx, 2);
IMPORT_OPTIONAL_SERVICE(ConfigService, svc_config);
IMPORT_OPTIONAL_SERVICE(UiService, svc_ui);
IMPORT_SERVICE(LogService, svc_log);

extern "C" {
MOD_EXPORT ModResult mod_initialize(ModError*) {
    midnafx::settings::initialize();
    midnafx::render::initialize();
    svc_log->info(mod_ctx, "MidnaFX M3 initialized; grading and presets ready when supported");
    return MOD_OK;
}
MOD_EXPORT ModResult mod_update(ModError*) {
    midnafx::render::update();
    return MOD_OK;
}
MOD_EXPORT ModResult mod_shutdown(ModError*) {
    // Dusklight deactivates/drains draw callbacks before invoking this export.
    midnafx::render::shutdown();
    midnafx::settings::shutdown();
    return MOD_OK;
}
}
