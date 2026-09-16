#include "settings.hpp"
#include "config/presets.hpp"
#include "config/visual.hpp"
#include "render/renderer.hpp"
#include "services.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

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
struct NumberSetting {
    const char* name;
    const char* label;
    std::int64_t min, max, initial, value;
    bool preset_value = false;
    ConfigVarHandle handle = 0;
};

Toggle master{"grading_enabled"}, diagnostics_toggle{"diagnostics"},
    passthrough{"passthrough_test"}, detail_toggle{"detail_enabled"};
NumberSetting detail_strength_setting{
    "detail_strength", "Detail strength (%)", 0, 50, 20, 20, true};
NumberSetting debug_mode_setting{"debug_mode", "Debug view", 0, 6, 0, 0, false};
NumberSetting split_setting{"split_percent", "A/B split position (%)", 10, 90, 50, 50, false};
constexpr const char* SmokeName = "Diagnostic / Shader Smoke Test";
constexpr std::array<const char*, 7> DebugLabels{
    "Final",           "Passthrough", "A/B Split", "Luminance", "Highlight Clipping",
    "Shadow Clipping", "Difference"};
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
double config_update_us = 0.0;
void warn(const char* message);
void check_ui(ModResult result);
void update_grade();
presets::Snapshot capture();
std::vector<presets::Entry> saved_presets;
std::string selected_preset = "Custom";
ConfigVarHandle presets_handle = 0, selected_handle = 0, custom_handle = 0;
UiElementHandle preset_control = 0;
bool applying_preset = false;
presets::Snapshot custom_snapshot;

void persist_custom() {
    if (!svc_config || !custom_handle)
        return;
    const auto encoded = presets::encode({{"Live", custom_snapshot}});
    if (encoded.empty() ||
        svc_config->set_string(mod_ctx, custom_handle, encoded.c_str()) != MOD_OK)
        warn("Could not save the Custom look.");
}
void remember_custom() {
    if (selected_preset == "Custom") {
        custom_snapshot = capture();
        persist_custom();
    }
}

void persist_selection() {
    if (svc_config && selected_handle)
        (void)svc_config->set_string(mod_ctx, selected_handle, selected_preset.c_str());
}
void mark_custom() {
    if (!applying_preset && selected_preset != "Custom") {
        selected_preset = "Custom";
        persist_selection();
    }
    if (!applying_preset)
        custom_snapshot = capture();
}
std::size_t preset_index() {
    if (selected_preset == "Vanilla")
        return 0;
    if (selected_preset == SmokeName)
        return 2;
    for (std::size_t i = 0; i < saved_presets.size(); ++i)
        if (saved_presets[i].name == selected_preset)
            return i + 3;
    return 1;
}
presets::Snapshot capture() {
    presets::Snapshot value;
    for (unsigned i = 0; i < grade::Count; ++i) {
        value.values[i] = effects[i].value;
        value.active[i] = effects[i].active;
    }
    value.detail_enabled = detail_toggle.value;
    value.detail_strength = detail_strength_setting.value;
    return value;
}
void persist_presets() {
    const auto encoded = presets::encode(saved_presets);
    if (encoded.empty() ||
        (svc_config && presets_handle &&
         svc_config->set_string(mod_ctx, presets_handle, encoded.c_str()) != MOD_OK))
        warn("Could not save presets.");
}
void refresh_preset_options() {
    if (!preset_control || !svc_ui || !svc_ui->control_set_options)
        return;
    std::vector<UiControlOption> options;
    options.push_back({sizeof(UiControlOption), "Vanilla", true});
    options.push_back({sizeof(UiControlOption), "Custom", true});
    options.push_back({sizeof(UiControlOption), SmokeName, true});
    for (const auto& preset : saved_presets)
        options.push_back({sizeof(UiControlOption), preset.name.c_str(), true});
    check_ui(svc_ui->control_set_options(mod_ctx, preset_control, options.data(), options.size()));
}
void apply_preset(const presets::Snapshot& snapshot, const std::string& name) {
    remember_custom();
    applying_preset = true;
    for (unsigned i = 0; i < grade::Count; ++i) {
        auto& effect = effects[i];
        effect.value = snapshot.values[i];
        effect.active = snapshot.active[i];
        if (svc_config && effect.value_handle &&
            svc_config->set_int(mod_ctx, effect.value_handle, effect.value) != MOD_OK)
            warn("Could not save a preset value.");
        if (svc_config && effect.active_handle &&
            svc_config->set_bool(mod_ctx, effect.active_handle, effect.active) != MOD_OK)
            warn("Could not save a preset toggle.");
    }
    detail_toggle.value = snapshot.detail_enabled;
    detail_strength_setting.value = snapshot.detail_strength;
    if (svc_config && detail_toggle.handle &&
        svc_config->set_bool(mod_ctx, detail_toggle.handle, detail_toggle.value) != MOD_OK)
        warn("Could not save preset detail toggle.");
    if (svc_config && detail_strength_setting.handle &&
        svc_config->set_int(mod_ctx, detail_strength_setting.handle,
                            detail_strength_setting.value) != MOD_OK)
        warn("Could not save preset detail strength.");
    selected_preset = name;
    persist_selection();
    applying_preset = false;
    update_grade();
}
std::string unique_name(const char* stem) {
    for (unsigned i = 1; i <= 999; ++i) {
        const auto name = std::string(stem) + " " + std::to_string(i);
        bool found = false;
        for (const auto& preset : saved_presets)
            found |= preset.name == name;
        if (!found)
            return name;
    }
    return {};
}

void warn(const char* message) {
    if (svc_log != nullptr)
        svc_log->warn(mod_ctx, message);
}
void update_grade() {
    const bool timing = diagnostics_toggle.value;
    const auto start =
        timing ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    grade::Controls controls;
    for (unsigned i = 0; i < grade::Count; ++i) {
        controls.value[i] = static_cast<float>(effects[i].value) / 100.0f;
        controls.active[i] = effects[i].active;
    }
    prepared = grade::prepare(controls);
    prepared.uniforms.detail_strength =
        visual::detail_strength(detail_toggle.value, detail_strength_setting.value);
    prepared.uniforms.debug_mode =
        static_cast<std::uint32_t>(visual::debug_mode(debug_mode_setting.value));
    if (timing)
        config_update_us =
            std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start)
                .count();
}
void on_toggle_config(ModContext*, ConfigVarHandle, const ConfigVarValue* value,
                      const ConfigVarValue*, void* user) {
    if (value && value->type == CONFIG_VAR_BOOL) {
        static_cast<Toggle*>(user)->value = value->bool_value;
        if (user == &detail_toggle && !applying_preset) {
            mark_custom();
            update_grade();
        }
    }
}
void on_number_config(ModContext*, ConfigVarHandle, const ConfigVarValue* value,
                      const ConfigVarValue*, void* user) {
    if (!value || value->type != CONFIG_VAR_INT)
        return;
    auto& setting = *static_cast<NumberSetting*>(user);
    setting.value = std::clamp(value->int_value, setting.min, setting.max);
    if (!applying_preset) {
        if (setting.preset_value)
            mark_custom();
        update_grade();
    }
}
void register_number(NumberSetting& setting) {
    setting.value = setting.initial;
    setting.handle = 0;
    if (!svc_config)
        return;
    ConfigVarDesc desc = CONFIG_VAR_DESC_INIT;
    desc.name = setting.name;
    desc.type = CONFIG_VAR_INT;
    desc.default_int = setting.initial;
    if (svc_config->register_var(mod_ctx, &desc, &setting.handle) != MOD_OK) {
        warn("A MidnaFX number setting could not be registered.");
        return;
    }
    std::int64_t saved = setting.initial;
    if (svc_config->get_int(mod_ctx, setting.handle, &saved) == MOD_OK)
        setting.value = std::clamp(saved, setting.min, setting.max);
    (void)svc_config->subscribe(mod_ctx, setting.handle, on_number_config, &setting, nullptr);
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
    if (!applying_preset) {
        mark_custom();
        update_grade();
    }
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
    if (&toggle == &detail_toggle) {
        mark_custom();
        update_grade();
    }
}
void get_number(ModContext*, void* user, UiControlValue* out) {
    out->int_value = static_cast<NumberSetting*>(user)->value;
}
void set_number(ModContext*, void* user, const UiControlValue* in) {
    auto& setting = *static_cast<NumberSetting*>(user);
    const auto value = std::clamp(in->int_value, setting.min, setting.max);
    if (svc_config && setting.handle &&
        svc_config->set_int(mod_ctx, setting.handle, value) != MOD_OK) {
        warn("Could not save number setting.");
        return;
    }
    setting.value = value;
    if (setting.preset_value)
        mark_custom();
    update_grade();
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
    mark_custom();
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
    mark_custom();
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
void add_number(UiElementHandle panel, NumberSetting& setting) {
    UiControlDesc desc = UI_CONTROL_DESC_INIT;
    desc.kind = UI_CONTROL_NUMBER;
    desc.label = setting.label;
    desc.get = get_number;
    desc.set = set_number;
    desc.user_data = &setting;
    desc.min = setting.min;
    desc.max = setting.max;
    desc.step = 1;
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
void reset_neutral(ModContext*, void*) { apply_preset(presets::Snapshot{}, "Vanilla"); }
void get_preset(ModContext*, void*, UiControlValue* out) {
    out->int_value = static_cast<std::int64_t>(preset_index());
}
void set_preset(ModContext*, void*, const UiControlValue* in) {
    const auto index = in->int_value;
    if (index == 0)
        apply_preset(presets::Snapshot{}, "Vanilla");
    else if (index == 1) {
        apply_preset(custom_snapshot, "Custom");
    } else if (index == 2)
        apply_preset(presets::smoke_test(), SmokeName);
    else if (index >= 3 && static_cast<std::size_t>(index - 3) < saved_presets.size()) {
        const auto& preset = saved_presets[static_cast<std::size_t>(index - 3)];
        apply_preset(preset.snapshot, preset.name);
    }
}
void save_preset(ModContext*, void*) {
    if (selected_preset != "Vanilla" && selected_preset != "Custom" &&
        selected_preset != SmokeName) {
        for (auto& preset : saved_presets)
            if (preset.name == selected_preset) {
                preset.snapshot = capture();
                persist_presets();
                return;
            }
    }
    if (saved_presets.size() >= presets::Maximum) {
        warn("Preset limit reached (16).");
        return;
    }
    remember_custom();
    auto name = unique_name("Custom");
    saved_presets.push_back({name, capture()});
    selected_preset = name;
    persist_presets();
    persist_selection();
    refresh_preset_options();
}
void duplicate_preset(ModContext*, void*) {
    if (saved_presets.size() >= presets::Maximum) {
        warn("Preset limit reached (16).");
        return;
    }
    remember_custom();
    auto name = unique_name("Copy");
    saved_presets.push_back({name, capture()});
    selected_preset = name;
    persist_presets();
    persist_selection();
    refresh_preset_options();
}
void load_preset(ModContext*, void*) {
    const auto index = preset_index();
    if (index == 0)
        apply_preset(presets::Snapshot{}, "Vanilla");
    else if (index == 2)
        apply_preset(presets::smoke_test(), SmokeName);
    else if (index >= 3)
        apply_preset(saved_presets[index - 3].snapshot, saved_presets[index - 3].name);
}
void reset_timing(ModContext*, void*) { render::reset_timing_samples(); }
void register_presets() {
    saved_presets.clear();
    selected_preset = "Custom";
    presets_handle = selected_handle = custom_handle = 0;
    custom_snapshot = capture();
    if (!svc_config)
        return;
    ConfigVarDesc desc = CONFIG_VAR_DESC_INIT;
    desc.name = "preset_data";
    desc.type = CONFIG_VAR_STRING;
    if (svc_config->register_var(mod_ctx, &desc, &presets_handle) == MOD_OK) {
        size_t length = 0;
        if (svc_config->get_string(mod_ctx, presets_handle, nullptr, 0, &length) == MOD_OK) {
            if (length <= 8192) {
                std::string data(length + 1, '\0');
                if (svc_config->get_string(mod_ctx, presets_handle, data.data(), data.size(),
                                           nullptr) == MOD_OK) {
                    data.resize(length);
                    if (!presets::decode(data, saved_presets))
                        warn("Invalid saved presets; using current settings.");
                }
            } else
                warn("Saved presets exceed the size limit.");
        }
    }
    desc = CONFIG_VAR_DESC_INIT;
    desc.name = "custom_data";
    desc.type = CONFIG_VAR_STRING;
    if (svc_config->register_var(mod_ctx, &desc, &custom_handle) == MOD_OK) {
        size_t length = 0;
        if (svc_config->get_string(mod_ctx, custom_handle, nullptr, 0, &length) == MOD_OK &&
            length <= 8192) {
            std::string data(length + 1, '\0');
            if (svc_config->get_string(mod_ctx, custom_handle, data.data(), data.size(), nullptr) ==
                MOD_OK) {
                data.resize(length);
                std::vector<presets::Entry> custom;
                if (presets::decode(data, custom) && custom.size() == 1 && custom[0].name == "Live")
                    custom_snapshot = custom[0].snapshot;
                else if (!data.empty())
                    warn("Invalid Custom look; using current settings.");
            }
        }
    }
    desc = CONFIG_VAR_DESC_INIT;
    desc.name = "preset_selected";
    desc.type = CONFIG_VAR_STRING;
    desc.default_string = "Custom";
    if (svc_config->register_var(mod_ctx, &desc, &selected_handle) == MOD_OK) {
        size_t length = 0;
        if (svc_config->get_string(mod_ctx, selected_handle, nullptr, 0, &length) == MOD_OK &&
            length <= 32) {
            std::string name(length + 1, '\0');
            if (svc_config->get_string(mod_ctx, selected_handle, name.data(), name.size(),
                                       nullptr) == MOD_OK) {
                name.resize(length);
                if (name == "Vanilla" || name == "Custom" || name == SmokeName)
                    selected_preset = name;
                else
                    for (const auto& entry : saved_presets)
                        if (entry.name == name)
                            selected_preset = name;
            }
        }
    }
    if (selected_preset == "Custom")
        custom_snapshot = capture();
}
void refresh_status() {
    const auto data = render::diagnostics();
    char status[512];
    const bool detail_active = prepared.uniforms.detail_strength > 0.0f;
    const auto mode = visual::debug_mode(debug_mode_setting.value);
    std::snprintf(status, sizeof(status), "%s | %s | %u x %u",
                  master.value ? (prepared.neutral && !detail_active &&
                                          mode == visual::DebugMode::Final && !passthrough.value
                                      ? "Neutral"
                                      : "Processing")
                               : "Disabled",
                  data.status, data.width, data.height);
    if (status_element)
        check_ui(svc_ui->elem_set_text(mod_ctx, status_element, status));
    if (!detail_element)
        return;
    char detail[1024];
    if (diagnostics_toggle.value)
        std::snprintf(
            detail, sizeof(detail),
            "Preset: %s | Shader: %s | Detail: %s (%lld%%) | View: %s\n"
            "Queued: %llu | Encoded: %llu | Binds: %llu | Pipelines: %llu\n"
            "CPU stage: %.2f us (p50 %.2f/p95 %.2f) | disabled: %.2f us (p50 %.2f/p95 %.2f, %llu "
            "samples)\n"
            "Layout query: %.2f us | resolve call: %.2f us | config update: %.2f us\n"
            "Snapshots: %llu | neutral: %llu | GPU timing unavailable | Backend: %s",
            selected_preset.c_str(),
            passthrough.value || mode == visual::DebugMode::Passthrough
                ? "Passthrough"
                : (mode == visual::DebugMode::Final
                       ? (detail_active ? "Grade + detail" : "Grade")
                       : (detail_active ? "Diagnostic + detail" : "Diagnostic")),
            detail_active ? "ON" : "OFF", static_cast<long long>(detail_strength_setting.value),
            DebugLabels[static_cast<std::size_t>(mode)],
            static_cast<unsigned long long>(data.submitted_draws),
            static_cast<unsigned long long>(data.encoded_draws),
            static_cast<unsigned long long>(data.bind_groups),
            static_cast<unsigned long long>(data.pipeline_builds), data.callback_us,
            data.active_p50_us, data.active_p95_us, data.disabled_us, data.disabled_p50_us,
            data.disabled_p95_us, static_cast<unsigned long long>(data.disabled_samples),
            data.layout_us, data.resolve_us, config_update_us,
            static_cast<unsigned long long>(data.snapshot_requests),
            static_cast<unsigned long long>(data.neutral_samples), data.backend);
    else
        std::snprintf(detail, sizeof(detail), "Diagnostics disabled. GPU timing unavailable.");
    check_ui(svc_ui->elem_set_text(mod_ctx, detail_element, detail));
}
ModResult build_panel(ModContext*, UiElementHandle panel, void*, ModError*) {
    status_element = detail_element = 0;
    preset_control = 0;
    check_ui(svc_ui->pane_add_section(mod_ctx, panel, "MidnaFX"));
    add_toggle(panel, "Enable grading", master);
    add_toggle(panel, "Force passthrough comparison", passthrough);
    check_ui(svc_ui->pane_add_section(mod_ctx, panel, "Presets"));
    std::vector<const char*> labels{"Vanilla", "Custom", SmokeName};
    for (const auto& preset : saved_presets)
        labels.push_back(preset.name.c_str());
    UiControlDesc choice = UI_CONTROL_DESC_INIT;
    choice.kind = UI_CONTROL_DROPDOWN;
    choice.label = "Current preset";
    choice.get = get_preset;
    choice.set = set_preset;
    choice.options = labels.data();
    choice.option_count = labels.size();
    check_ui(svc_ui->pane_add_control(mod_ctx, panel, &choice, &preset_control));
    UiControlDesc button = UI_CONTROL_DESC_INIT;
    button.kind = UI_CONTROL_BUTTON;
    button.label = "Save current preset";
    button.on_pressed = save_preset;
    check_ui(svc_ui->pane_add_control(mod_ctx, panel, &button, nullptr));
    button.label = "Load selected preset";
    button.on_pressed = load_preset;
    check_ui(svc_ui->pane_add_control(mod_ctx, panel, &button, nullptr));
    button.label = "Duplicate current look";
    button.on_pressed = duplicate_preset;
    check_ui(svc_ui->pane_add_control(mod_ctx, panel, &button, nullptr));
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
    check_ui(svc_ui->pane_add_section(mod_ctx, panel, "Detail"));
    add_toggle(panel, "Enable sharpening", detail_toggle);
    add_number(panel, detail_strength_setting);
    check_ui(svc_ui->pane_add_section(mod_ctx, panel, "Visual diagnostics"));
    UiControlDesc debug = UI_CONTROL_DESC_INIT;
    debug.kind = UI_CONTROL_DROPDOWN;
    debug.label = "Debug view";
    debug.get = get_number;
    debug.set = set_number;
    debug.user_data = &debug_mode_setting;
    debug.options = DebugLabels.data();
    debug.option_count = DebugLabels.size();
    check_ui(svc_ui->pane_add_control(mod_ctx, panel, &debug, nullptr));
    add_number(panel, split_setting);
    check_ui(svc_ui->pane_add_section(mod_ctx, panel, "Diagnostics"));
    add_toggle(panel, "Enable CPU diagnostics", diagnostics_toggle);
    button.label = "Reset CPU timing samples";
    button.on_pressed = reset_timing;
    check_ui(svc_ui->pane_add_control(mod_ctx, panel, &button, nullptr));
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
    register_toggle(detail_toggle);
    register_number(detail_strength_setting);
    register_number(debug_mode_setting);
    register_number(split_setting);
    for (auto& effect : effects)
        register_effect(effect);
    register_presets();
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
std::int64_t split_percent() { return split_setting.value; }
grade::Prepared prepared_grade() { return prepared; }
void shutdown() { status_element = detail_element = preset_control = 0; }
} // namespace midnafx::settings
