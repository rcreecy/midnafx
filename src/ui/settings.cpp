#include "settings.hpp"
#include "../render/renderer.hpp"
#include "../services.hpp"

#include <chrono>
#include <cstdio>

namespace midnafx::settings {
namespace {
struct Setting {
    const char* name;
    bool value = false;
    ConfigVarHandle handle = 0;
    ConfigSubscriptionHandle subscription = 0;
};
Setting enabled_setting{"grading_enabled"};
Setting diagnostics_setting{"diagnostics"};
UiElementHandle status_element = 0;
UiElementHandle diagnostics_element = 0;
bool ui_error_logged = false;
std::chrono::steady_clock::time_point next_refresh{};

void warn(const char* message) {
    if (svc_log != nullptr)
        svc_log->warn(mod_ctx, message);
}

void changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*,
             void* user_data) {
    if (value != nullptr && value->type == CONFIG_VAR_BOOL)
        static_cast<Setting*>(user_data)->value = value->bool_value;
}

void register_setting(Setting& setting) {
    if (svc_config == nullptr)
        return;
    ConfigVarDesc desc = CONFIG_VAR_DESC_INIT;
    desc.name = setting.name;
    desc.type = CONFIG_VAR_BOOL;
    if (svc_config->register_var(mod_ctx, &desc, &setting.handle) != MOD_OK) {
        setting.handle = 0;
        warn("Could not register a setting; using session-only defaults.");
        return;
    }
    bool saved = false;
    if (svc_config->get_bool(mod_ctx, setting.handle, &saved) == MOD_OK) {
        setting.value = saved;
    } else {
        warn("Could not read a saved setting; using the safe default.");
    }
    if (svc_config->subscribe(mod_ctx, setting.handle, changed, &setting, &setting.subscription) !=
        MOD_OK) {
        setting.subscription = 0;
        warn("Setting change subscription unavailable; local controls remain usable.");
    }
}

void get_setting(ModContext*, void* user_data, UiControlValue* value) {
    value->bool_value = static_cast<const Setting*>(user_data)->value;
}

void set_setting(ModContext*, void* user_data, const UiControlValue* value) {
    auto& setting = *static_cast<Setting*>(user_data);
    if (svc_config != nullptr && setting.handle != 0) {
        if (svc_config->set_bool(mod_ctx, setting.handle, value->bool_value) != MOD_OK) {
            warn("Could not save setting; the previous value remains active.");
            return;
        }
    }
    setting.value = value->bool_value;
}

void check_ui(ModResult result) {
    if (result != MOD_OK && !ui_error_logged) {
        ui_error_logged = true;
        warn("Some MidnaFX UI content is unavailable; rendering remains independent.");
    }
}

void add_toggle(UiElementHandle panel, const char* label, Setting& setting) {
    UiControlDesc control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_TOGGLE;
    control.label = label;
    control.get = get_setting;
    control.set = set_setting;
    control.user_data = &setting;
    check_ui(svc_ui->pane_add_control(mod_ctx, panel, &control, nullptr));
}

void refresh() {
    const auto data = render::diagnostics();
    char status[768];
    std::snprintf(status, sizeof(status), "%s | %s | %u x %u | Backend: %s",
                  enabled_setting.value ? "Enabled" : "Disabled",
                  data.status != nullptr ? data.status : "Unavailable", data.width, data.height,
                  data.backend != nullptr ? data.backend : "Unknown");
    if (status_element != 0)
        check_ui(svc_ui->elem_set_text(mod_ctx, status_element, status));
    char details[1024];
    if (diagnostics_setting.value) {
        std::snprintf(details, sizeof(details),
                      "Submitted draws: %llu | Encoded draws: %llu | Bind groups: %llu | Pipeline "
                      "builds: %llu\n"
                      "CPU callback: %.2f us (%s). GPU timing unavailable.\n"
                      "Snapshot memory: host-managed; a four-byte color snapshot at this size is "
                      "approximately %.2f MiB. "
                      "This estimate excludes MSAA and other host allocations.",
                      static_cast<unsigned long long>(data.submitted_draws),
                      static_cast<unsigned long long>(data.encoded_draws),
                      static_cast<unsigned long long>(data.bind_groups),
                      static_cast<unsigned long long>(data.pipeline_builds), data.callback_us,
                      data.timing_enabled ? "recording time, not GPU time" : "timing disabled",
                      static_cast<double>(data.width) * static_cast<double>(data.height) * 4.0 /
                          1048576.0);
    } else {
        std::snprintf(details, sizeof(details), "Diagnostics disabled. GPU timing unavailable.");
    }
    if (diagnostics_element != 0)
        check_ui(svc_ui->elem_set_text(mod_ctx, diagnostics_element, details));
}

ModResult build_panel(ModContext*, UiElementHandle panel, void*, ModError*) {
    status_element = 0;
    diagnostics_element = 0;
    check_ui(svc_ui->pane_add_section(mod_ctx, panel, "MidnaFX — M1 passthrough"));
    check_ui(svc_ui->pane_add_text(mod_ctx, panel,
                                   "M1 validates a pre-HUD scene copy and fullscreen passthrough. "
                                   "Color grading and presets are not implemented.",
                                   nullptr));
    add_toggle(panel, "Enable passthrough (adds a scene copy and draw)", enabled_setting);
    add_toggle(panel, "Enable diagnostics", diagnostics_setting);
    check_ui(svc_ui->pane_add_text(mod_ctx, panel, "", &status_element));
    check_ui(svc_ui->pane_add_text(mod_ctx, panel, "", &diagnostics_element));
    refresh();
    next_refresh = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
    return MOD_OK;
}

ModResult update_panel(ModContext*, void*, ModError*) {
    const auto now = std::chrono::steady_clock::now();
    if (now >= next_refresh) {
        refresh();
        next_refresh = now + std::chrono::milliseconds(250);
    }
    return MOD_OK;
}
} // namespace

bool initialize() {
    enabled_setting = Setting{"grading_enabled"};
    diagnostics_setting = Setting{"diagnostics"};
    status_element = diagnostics_element = 0;
    ui_error_logged = false;
    register_setting(enabled_setting);
    register_setting(diagnostics_setting);
    if (svc_config == nullptr)
        warn("Configuration service unavailable; settings last for this session only.");
    if (svc_ui != nullptr) {
        UiModsPanelDesc panel = UI_MODS_PANEL_DESC_INIT;
        panel.build = build_panel;
        panel.update = update_panel;
        check_ui(svc_ui->register_mods_panel(mod_ctx, &panel));
    } else {
        warn("UI service unavailable; using saved settings or defaults.");
    }
    return true;
}

bool enabled() { return enabled_setting.value; }
bool diagnostics_enabled() { return diagnostics_setting.value; }

void shutdown() {
    // The host removes panels and their callbacks immediately after mod_shutdown.
    // Explicit subscriptions are removed before resetting their callback storage.
    for (auto* setting : {&enabled_setting, &diagnostics_setting}) {
        if (svc_config != nullptr && setting->subscription != 0)
            (void)svc_config->unsubscribe(mod_ctx, setting->subscription);
        setting->subscription = 0;
        setting->handle = 0;
        setting->value = false;
    }
    status_element = diagnostics_element = 0;
}
} // namespace midnafx::settings
