#include "settings.hpp"
#include "render/renderer.hpp"
#include "services.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>

namespace midnafx::settings {
namespace {
struct Setting {
    const char* name;
    const char* label;
    std::int64_t min, max, initial, value;
    bool active = true;
    ConfigVarHandle value_handle = 0, active_handle = 0;
};
struct Toggle {
    const char* name;
    bool value = false;
    ConfigVarHandle handle = 0;
};

Toggle master{"grading_enabled"}, diagnostics_toggle{"diagnostics"},
    passthrough{"passthrough_test"};
std::array<Setting, grade::Count> effects{{
    {"exposure", "Exposure (0.01 EV)", -200, 200, 0, 0},
    {"black_point", "Black point (%)", 0, 20, 0, 0},
    {"contrast", "Contrast (%)", 50, 150, 100, 100},
    {"gamma", "Gamma (%)", 70, 150, 100, 100},
    {"saturation", "Saturation (%)", 0, 200, 100, 100},
    {"rolloff", "Highlight rolloff (%)", 0, 100, 0, 0},
    {"temperature", "Temperature (%)", -100, 100, 0, 0},
    {"tint", "Tint (%)", -100, 100, 0, 0},
}};
grade::Prepared prepared = grade::prepare({});
UiElementHandle status_element = 0, detail_element = 0;
std::chrono::steady_clock::time_point next_refresh{};
bool warned_ui = false;

void warn(const char* message) {
    if (svc_log != nullptr)
        svc_log->warn(mod_ctx, message);
}
void update_grade() {
    grade::Controls controls;
    for (unsigned i = 0; i < grade::Count; ++i) {
        controls.value[i] = static_cast<float>(effects[i].value) / 100.0f;
        controls.active[i] = effects[i].active;
    }
    prepared = grade::prepare(controls);
}
void on_toggle_config(ModContext*, ConfigVarHandle, const ConfigVarValue* value,
                      const ConfigVarValue*, void* user) {
    if (value && value->type == CONFIG_VAR_BOOL)
        static_cast<Toggle*>(user)->value = value->bool_value;
}
void on_effect_config(ModContext*, ConfigVarHandle variable, const ConfigVarValue* value,
                      const ConfigVarValue*, void* user) {
    auto& effect = *static_cast<Setting*>(user);
    if (!value)
        return;
    if (variable == effect.value_handle && value->type == CONFIG_VAR_INT)
        effect.value = std::clamp(value->int_value, effect.min, effect.max);
    if (variable == effect.active_handle && value->type == CONFIG_VAR_BOOL)
        effect.active = value->bool_value;
    update_grade();
}
void register_toggle(Toggle& toggle) {
    toggle.value = false;
    toggle.handle = 0;
    if (!svc_config)
        return;
    ConfigVarDesc desc = CONFIG_VAR_DESC_INIT;
    desc.name = toggle.name;
    desc.type = CONFIG_VAR_BOOL;
    if (svc_config->register_var(mod_ctx, &desc, &toggle.handle) != MOD_OK) {
        warn("MidnaFX setting could not be registered.");
        return;
    }
    (void)svc_config->get_bool(mod_ctx, toggle.handle, &toggle.value);
    (void)svc_config->subscribe(mod_ctx, toggle.handle, on_toggle_config, &toggle, nullptr);
}
void register_effect(Setting& effect) {
    effect.value = effect.initial;
    effect.active = true;
    effect.value_handle = effect.active_handle = 0;
    if (!svc_config)
        return;
    ConfigVarDesc desc = CONFIG_VAR_DESC_INIT;
    desc.name = effect.name;
    desc.type = CONFIG_VAR_INT;
    desc.default_int = effect.initial;
    if (svc_config->register_var(mod_ctx, &desc, &effect.value_handle) == MOD_OK) {
        std::int64_t saved = effect.initial;
        if (svc_config->get_int(mod_ctx, effect.value_handle, &saved) == MOD_OK)
            effect.value = std::clamp(saved, effect.min, effect.max);
        (void)svc_config->subscribe(mod_ctx, effect.value_handle, on_effect_config, &effect,
                                    nullptr);
    } else
        warn("A grading value could not be registered.");
    desc = CONFIG_VAR_DESC_INIT;
    // Names are stable literals; ConfigService copies them at registration.
    static constexpr const char* active_names[grade::Count] = {
        "exposure_on",   "black_point_on", "contrast_on",    "gamma_on",
        "saturation_on", "rolloff_on",     "temperature_on", "tint_on"};
    const auto index = static_cast<unsigned>(&effect - effects.data());
    desc.name = active_names[index];
    desc.type = CONFIG_VAR_BOOL;
    desc.default_bool = true;
    if (svc_config->register_var(mod_ctx, &desc, &effect.active_handle) == MOD_OK) {
        (void)svc_config->get_bool(mod_ctx, effect.active_handle, &effect.active);
        (void)svc_config->subscribe(mod_ctx, effect.active_handle, on_effect_config, &effect,
                                    nullptr);
    } else
        warn("An effect toggle could not be registered.");
}
void get_toggle(ModContext*, void* user, UiControlValue* out) {
    out->bool_value = static_cast<Toggle*>(user)->value;
}
void set_toggle(ModContext*, void* user, const UiControlValue* in) {
    auto& toggle = *static_cast<Toggle*>(user);
    if (svc_config && toggle.handle &&
        svc_config->set_bool(mod_ctx, toggle.handle, in->bool_value) != MOD_OK) {
        warn("Could not save setting.");
        return;
    }
    toggle.value = in->bool_value;
}
void get_effect_value(ModContext*, void* user, UiControlValue* out) {
    out->int_value = static_cast<Setting*>(user)->value;
}
void set_effect_value(ModContext*, void* user, const UiControlValue* in) {
    auto& effect = *static_cast<Setting*>(user);
    const auto value = std::clamp(in->int_value, effect.min, effect.max);
    if (svc_config && effect.value_handle &&
        svc_config->set_int(mod_ctx, effect.value_handle, value) != MOD_OK) {
        warn("Could not save grading value.");
        return;
    }
    effect.value = value;
    update_grade();
}
void get_effect_active(ModContext*, void* user, UiControlValue* out) {
    out->bool_value = static_cast<Setting*>(user)->active;
}
void set_effect_active(ModContext*, void* user, const UiControlValue* in) {
    auto& effect = *static_cast<Setting*>(user);
    if (svc_config && effect.active_handle &&
        svc_config->set_bool(mod_ctx, effect.active_handle, in->bool_value) != MOD_OK) {
        warn("Could not save effect toggle.");
        return;
    }
    effect.active = in->bool_value;
    update_grade();
}
void check_ui(ModResult result) {
    if (result != MOD_OK && !warned_ui) {
        warned_ui = true;
        warn("Some MidnaFX controls are unavailable.");
    }
}
void add_toggle(UiElementHandle panel, const char* label, Toggle& toggle) {
    UiControlDesc desc = UI_CONTROL_DESC_INIT;
    desc.kind = UI_CONTROL_TOGGLE;
    desc.label = label;
    desc.get = get_toggle;
    desc.set = set_toggle;
    desc.user_data = &toggle;
    check_ui(svc_ui->pane_add_control(mod_ctx, panel, &desc, nullptr));
}
void add_effect(UiElementHandle panel, Setting& effect) {
    static constexpr const char* enabled_labels[grade::Count] = {
        "Exposure enabled",   "Black point enabled",       "Contrast enabled",    "Gamma enabled",
        "Saturation enabled", "Highlight rolloff enabled", "Temperature enabled", "Tint enabled"};
    const auto index = static_cast<unsigned>(&effect - effects.data());
    UiControlDesc desc = UI_CONTROL_DESC_INIT;
    desc.kind = UI_CONTROL_TOGGLE;
    desc.label = enabled_labels[index];
    desc.get = get_effect_active;
    desc.set = set_effect_active;
    desc.user_data = &effect;
    check_ui(svc_ui->pane_add_control(mod_ctx, panel, &desc, nullptr));
    desc = UI_CONTROL_DESC_INIT;
    desc.kind = UI_CONTROL_NUMBER;
    desc.label = effect.label;
    desc.user_data = &effect;
    desc.get = get_effect_value;
    desc.set = set_effect_value;
    desc.min = effect.min;
    desc.max = effect.max;
    desc.step = 1;
    check_ui(svc_ui->pane_add_control(mod_ctx, panel, &desc, nullptr));
}
void reset_neutral(ModContext*, void*) {
    for (auto& effect : effects) {
        if (!svc_config || !effect.value_handle ||
            svc_config->set_int(mod_ctx, effect.value_handle, effect.initial) == MOD_OK)
            effect.value = effect.initial;
        else
            warn("Could not restore a saved grading value.");
        if (!svc_config || !effect.active_handle ||
            svc_config->set_bool(mod_ctx, effect.active_handle, true) == MOD_OK)
            effect.active = true;
        else
            warn("Could not restore a saved effect toggle.");
    }
    update_grade();
}
void refresh_status() {
    const auto data = render::diagnostics();
    char status[512];
    std::snprintf(status, sizeof(status), "%s | %s | %u x %u",
                  master.value ? (prepared.neutral ? "Neutral" : "Grading") : "Disabled",
                  data.status, data.width, data.height);
    if (status_element)
        check_ui(svc_ui->elem_set_text(mod_ctx, status_element, status));
    if (!detail_element)
        return;
    char detail[640];
    if (diagnostics_toggle.value)
        std::snprintf(detail, sizeof(detail),
                      "Queued: %llu | Encoded: %llu | Bind groups: %llu | Pipelines: %llu\n"
                      "CPU stage: %.2f us. GPU timing unavailable. Backend: %s",
                      static_cast<unsigned long long>(data.submitted_draws),
                      static_cast<unsigned long long>(data.encoded_draws),
                      static_cast<unsigned long long>(data.bind_groups),
                      static_cast<unsigned long long>(data.pipeline_builds), data.callback_us,
                      data.backend);
    else
        std::snprintf(detail, sizeof(detail), "Diagnostics disabled. GPU timing unavailable.");
    check_ui(svc_ui->elem_set_text(mod_ctx, detail_element, detail));
}
ModResult build_panel(ModContext*, UiElementHandle panel, void*, ModError*) {
    status_element = detail_element = 0;
    check_ui(svc_ui->pane_add_section(mod_ctx, panel, "MidnaFX"));
    add_toggle(panel, "Enable grading", master);
    add_toggle(panel, "Force passthrough comparison", passthrough);
    UiControlDesc reset = UI_CONTROL_DESC_INIT;
    reset.kind = UI_CONTROL_BUTTON;
    reset.label = "Restore neutral grading";
    reset.on_pressed = reset_neutral;
    check_ui(svc_ui->pane_add_control(mod_ctx, panel, &reset, nullptr));
    check_ui(svc_ui->pane_add_section(mod_ctx, panel, "Tone"));
    for (unsigned i = grade::Exposure; i <= grade::Gamma; ++i)
        add_effect(panel, effects[i]);
    add_effect(panel, effects[grade::HighlightRolloff]);
    check_ui(svc_ui->pane_add_section(mod_ctx, panel, "Color"));
    add_effect(panel, effects[grade::Saturation]);
    add_effect(panel, effects[grade::Temperature]);
    add_effect(panel, effects[grade::Tint]);
    check_ui(svc_ui->pane_add_section(mod_ctx, panel, "Diagnostics"));
    add_toggle(panel, "Enable CPU diagnostics", diagnostics_toggle);
    check_ui(svc_ui->pane_add_text(mod_ctx, panel, "", &status_element));
    check_ui(svc_ui->pane_add_text(mod_ctx, panel, "", &detail_element));
    refresh_status();
    next_refresh = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
    return MOD_OK;
}
ModResult update_panel(ModContext*, void*, ModError*) {
    const auto now = std::chrono::steady_clock::now();
    if (now >= next_refresh) {
        refresh_status();
        next_refresh = now + std::chrono::milliseconds(250);
    }
    return MOD_OK;
}
} // namespace

bool initialize() {
    register_toggle(master);
    register_toggle(diagnostics_toggle);
    register_toggle(passthrough);
    for (auto& effect : effects)
        register_effect(effect);
    update_grade();
    if (!svc_config)
        warn("Configuration service unavailable; settings last for this session only.");
    if (svc_ui) {
        UiModsPanelDesc panel = UI_MODS_PANEL_DESC_INIT;
        panel.build = build_panel;
        panel.update = update_panel;
        check_ui(svc_ui->register_mods_panel(mod_ctx, &panel));
    } else
        warn("UI service unavailable; using saved settings or defaults.");
    return true;
}
bool enabled() { return master.value; }
bool diagnostics_enabled() { return diagnostics_toggle.value; }
bool passthrough_test() { return passthrough.value; }
grade::Prepared prepared_grade() { return prepared; }
void shutdown() { status_element = detail_element = 0; }
} // namespace midnafx::settings
