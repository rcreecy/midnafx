#include "game/camera_probe.hpp"
#include "game/game_state.hpp"
#include "game/geometry_probe.hpp"
#include "render/renderer.hpp"
#include "services.hpp"
#include "ui/settings.hpp"
#include <mods/svc/gfx.h>

#include <chrono>

DEFINE_MOD();
IMPORT_OPTIONAL_SERVICE_VERSION(GfxService, svc_gfx, 2);
IMPORT_OPTIONAL_SERVICE(ConfigService, svc_config);
IMPORT_OPTIONAL_SERVICE_VERSION(CameraService, svc_camera, 1);
IMPORT_OPTIONAL_SERVICE(HookService, svc_hook);
IMPORT_OPTIONAL_SERVICE(UiService, svc_ui);
IMPORT_SERVICE(LogService, svc_log);

namespace {
std::chrono::steady_clock::time_point last_update{};
}

extern "C" {
MOD_EXPORT ModResult mod_initialize(ModError*) {
    midnafx::settings::initialize();
    midnafx::render::initialize();
    midnafx::camera_probe::initialize();
    midnafx::geometry_probe::initialize();
    last_update = std::chrono::steady_clock::now();
    svc_log->info(mod_ctx,
                  "MidnaFX grading, detail, diagnostics, and Twilight prototype initialized");
    return MOD_OK;
}
MOD_EXPORT ModResult mod_update(ModError*) {
    const auto now = std::chrono::steady_clock::now();
    const float elapsed = std::chrono::duration<float>(now - last_update).count();
    last_update = now;
    midnafx::settings::update_twilight(midnafx::game_state::sample(), elapsed);
    midnafx::render::update();
    midnafx::settings::update_diagnostics();
    return MOD_OK;
}
MOD_EXPORT ModResult mod_shutdown(ModError*) {
    // Dusklight deactivates/drains draw callbacks before invoking this export.
    midnafx::render::shutdown();
    midnafx::camera_probe::shutdown();
    midnafx::geometry_probe::shutdown();
    midnafx::settings::shutdown();
    return MOD_OK;
}
}
