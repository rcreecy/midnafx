#include "settings.hpp"
#include "config/presets.hpp"
#include "config/visual.hpp"
#include "game/camera_probe.hpp"
#include "game/geometry_probe.hpp"
#include "game/twilight.hpp"
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
    passthrough{"passthrough_test"}, detail_toggle{"detail_enabled"},
    auto_twilight{"auto_twilight"}, geometry_diagnostics{"geometry_diagnostics"},
    topology_diagnostics{"topology_diagnostics"}, geometry_mutation_test{"geometry_mutation_test"},
    geometry_smoothing{"geometry_smoothing"},
    geometry_skinned_smoothing{"geometry_skinned_smoothing"}, camera_toggle{"camera_enabled"},
    camera_lower_angle{"camera_lower_angle"}, atmosphere_depth_probe{"atmosphere_depth_probe"};
NumberSetting smoothing_angle_setting{
    "geometry_smoothing_angle", "Smoothing face angle (degrees)", 10, 90, 55, 55, false};
NumberSetting detail_strength_setting{
    "detail_strength", "Detail strength (%)", 0, 50, 20, 20, true};
NumberSetting debug_mode_setting{"debug_mode", "Debug view", 0, 6, 0, 0, false};
NumberSetting split_setting{"split_percent", "A/B split position (%)", 10, 90, 50, 50, false};
NumberSetting twilight_transition{
    "twilight_transition_cs", "Twilight transition (0.01 s)", 0, 300, 100, 100, false};
NumberSetting camera_fov_setting{
    "camera_fov_percent", "Exploration FOV scale (%)", 80, 140, 110, 110, false};
NumberSetting camera_transition_setting{
    "camera_transition_cs", "Camera transition (0.01 s)", 0, 200, 35, 35, false};
NumberSetting camera_angle_setting{
    "camera_angle_degrees", "Camera elevation reduction (degrees)", 0, 15, 6, 6, false};
constexpr const char* RealismName = "Natural / Vivid Realism";
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
grade::Prepared twilight_prepared = grade::prepare({});
presets::Snapshot twilight_target;
bool has_twilight_target = false;
ConfigVarHandle twilight_target_handle = 0;
twilight::State twilight_state = twilight::State::Unavailable;
float twilight_weight = 0.0f;
twilight::State logged_twilight_state = twilight::State::Unavailable;
int logged_twilight_endpoint = -1;
bool twilight_log_initialized = false;
UiElementHandle status_element = 0, detail_element = 0, camera_element = 0;
std::chrono::steady_clock::time_point next_refresh{};
bool warned_ui = false;
double config_update_us = 0.0;
bool diagnostics_log_written = false;
bool diagnostics_sampling_enabled = false;
void warn(const char* message);
void check_ui(ModResult result);
void update_grade();
void update_twilight_prepared();
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
    if (selected_preset == RealismName)
        return 3;
    for (std::size_t i = 0; i < saved_presets.size(); ++i)
        if (saved_presets[i].name == selected_preset)
            return i + 4;
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
    options.push_back({sizeof(UiControlOption), RealismName, true});
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
void update_twilight_prepared() {
    grade::Controls controls;
    for (unsigned i = 0; i < grade::Count; ++i) {
        controls.value[i] = static_cast<float>(twilight_target.values[i]) / 100.0f;
        controls.active[i] = twilight_target.active[i];
    }
    twilight_prepared = grade::prepare(controls);
    twilight_prepared.uniforms.detail_strength =
        visual::detail_strength(twilight_target.detail_enabled, twilight_target.detail_strength);
}
void on_toggle_config(ModContext*, ConfigVarHandle, const ConfigVarValue* value,
                      const ConfigVarValue*, void* user) {
    if (value && value->type == CONFIG_VAR_BOOL) {
        static_cast<Toggle*>(user)->value = value->bool_value;
        if (user == &geometry_mutation_test && !value->bool_value)
            geometry_probe::restore_mutation();
        if ((user == &geometry_smoothing || user == &geometry_skinned_smoothing) &&
            !value->bool_value)
            geometry_probe::restore_smoothing(user == &geometry_skinned_smoothing);
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
    if (&toggle == &geometry_mutation_test && !toggle.value)
        geometry_probe::restore_mutation();
    if (&toggle == &geometry_smoothing && !toggle.value)
        geometry_probe::restore_smoothing(false);
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
    else if (index == 3)
        apply_preset(presets::vivid_realism(), RealismName);
    else if (index >= 4 && static_cast<std::size_t>(index - 4) < saved_presets.size()) {
        const auto& preset = saved_presets[static_cast<std::size_t>(index - 4)];
        apply_preset(preset.snapshot, preset.name);
    }
}
void save_preset(ModContext*, void*) {
    if (selected_preset != "Vanilla" && selected_preset != "Custom" &&
        selected_preset != SmokeName && selected_preset != RealismName) {
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
    else if (index == 3)
        apply_preset(presets::vivid_realism(), RealismName);
    else if (index >= 4)
        apply_preset(saved_presets[index - 4].snapshot, saved_presets[index - 4].name);
}
void capture_twilight_target(ModContext*, void*) {
    const auto snapshot = capture();
    const auto encoded = presets::encode({{"Twilight", snapshot}});
    if (encoded.empty() ||
        (svc_config && twilight_target_handle &&
         svc_config->set_string(mod_ctx, twilight_target_handle, encoded.c_str()) != MOD_OK)) {
        warn("Could not save Twilight target.");
        return;
    }
    twilight_target = snapshot;
    has_twilight_target = true;
    update_twilight_prepared();
}
void clear_twilight_target(ModContext*, void*) {
    if (svc_config && twilight_target_handle &&
        svc_config->set_string(mod_ctx, twilight_target_handle, "") != MOD_OK) {
        warn("Could not clear Twilight target.");
        return;
    }
    has_twilight_target = false;
    twilight_weight = 0.0f;
}
void reset_timing(ModContext*, void*) {
    render::reset_timing_samples();
    diagnostics_log_written = false;
}
void register_twilight_target() {
    has_twilight_target = false;
    twilight_target_handle = 0;
    twilight_weight = 0.0f;
    twilight_state = twilight::State::Unavailable;
    logged_twilight_state = twilight::State::Unavailable;
    logged_twilight_endpoint = -1;
    twilight_log_initialized = false;
    if (!svc_config)
        return;
    ConfigVarDesc desc = CONFIG_VAR_DESC_INIT;
    desc.name = "twilight_target";
    desc.type = CONFIG_VAR_STRING;
    if (svc_config->register_var(mod_ctx, &desc, &twilight_target_handle) != MOD_OK) {
        warn("Could not register Twilight target storage.");
        return;
    }
    size_t length = 0;
    if (svc_config->get_string(mod_ctx, twilight_target_handle, nullptr, 0, &length) != MOD_OK ||
        length == 0)
        return;
    if (length > 8192) {
        warn("Saved Twilight target exceeds size limit.");
        return;
    }
    std::string data(length + 1, '\0');
    if (svc_config->get_string(mod_ctx, twilight_target_handle, data.data(), data.size(),
                               nullptr) != MOD_OK)
        return;
    data.resize(length);
    std::vector<presets::Entry> entries;
    if (presets::decode(data, entries) && entries.size() == 1 && entries[0].name == "Twilight") {
        twilight_target = entries[0].snapshot;
        has_twilight_target = true;
        update_twilight_prepared();
    } else
        warn("Invalid saved Twilight target; automatic profile inactive.");
}
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
                if (name == "Vanilla" || name == "Custom" || name == SmokeName || name == RealismName)
                    selected_preset = name;
                else
                    for (const auto& entry : saved_presets)
                        if (entry.name == name)
                            selected_preset = name;
            }
        }
    }
}
void refresh_status() {
    const auto data = render::diagnostics();
    char status[512];
    const auto active_grade = prepared_grade();
    const bool detail_active = active_grade.uniforms.detail_strength > 0.0f;
    const auto mode = visual::debug_mode(debug_mode_setting.value);
    std::snprintf(status, sizeof(status), "%s | %s | %u x %u",
                  master.value ? (active_grade.neutral && !detail_active &&
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
            "Snapshots: %llu | neutral: %llu | GPU timing unavailable | Backend: %s\n"
            "World: %s | Auto Twilight: %s | Target: %s | Blend: %.0f%%",
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
            static_cast<unsigned long long>(data.neutral_samples), data.backend,
            twilight::label(twilight_state), auto_twilight.value ? "ON" : "OFF",
            has_twilight_target ? "captured" : "unset", 100.0f * twilight_weight);
    else
        std::snprintf(detail, sizeof(detail), "Diagnostics disabled. GPU timing unavailable.");
    check_ui(svc_ui->elem_set_text(mod_ctx, detail_element, detail));
    if (camera_element) {
        const auto camera = camera_probe::snapshot();
        char camera_text[768];
        if (!camera.registered)
            std::snprintf(camera_text, sizeof(camera_text),
                          "Camera probe unavailable | MidnaFX override: OFF");
        else if (!camera.valid)
            std::snprintf(camera_text, sizeof(camera_text),
                          "No recent perspective scene | FOV API: %s | Chase API: %s | "
                          "Overrides: OFF",
                          camera.modifier_registered ? "ready" : "unavailable",
                          camera.chase_modifier_registered ? "ready" : "unavailable");
        else
            std::snprintf(camera_text, sizeof(camera_text),
                           "Camera type/mode: %d/%d | FOV API: %s | Override: %s\n"
                           "Native/requested/observed vertical FOV: %.2f / %.2f / %.2f deg | "
                           "Aspect: %.3f | Near/far: %.2f / %.2f\n"
                           "Chase API: %s | Lower angle: %s | Native latitude far/near: "
                           "%.2f / %.2f deg | "
                           "Offset: %.2f deg\n"
                           "Eye: %.2f, %.2f, %.2f | Probe: %.2f us | Samples: %llu / %llu / %llu",
                          camera.camera_type, camera.camera_mode,
                          camera.modifier_registered ? "ready" : "unavailable",
                          camera.modifier_active ? "ON" : "OFF", camera.native_fovy,
                           camera.effective_fovy, camera.observed_fovy, camera.aspect,
                           camera.near_plane, camera.far_plane,
                           camera.chase_modifier_registered ? "ready" : "unavailable",
                           camera.chase_modifier_active ? "ON" : "OFF",
                           camera.native_latitude_far, camera.native_latitude_near,
                           camera.latitude_offset,
                           camera.eye[0], camera.eye[1], camera.eye[2], camera.callback_us,
                           static_cast<unsigned long long>(camera.samples),
                           static_cast<unsigned long long>(camera.modifier_samples),
                           static_cast<unsigned long long>(camera.chase_modifier_samples));
        check_ui(svc_ui->elem_set_text(mod_ctx, camera_element, camera_text));
    }
}
ModResult build_panel(ModContext*, UiElementHandle panel, void*, ModError*) {
    status_element = detail_element = camera_element = 0;
    preset_control = 0;
    check_ui(svc_ui->pane_add_section(mod_ctx, panel, "MidnaFX"));
    add_toggle(panel, "Enable grading", master);
    add_toggle(panel, "Force passthrough comparison", passthrough);
    check_ui(svc_ui->pane_add_section(mod_ctx, panel, "Presets"));
    std::vector<const char*> labels{"Vanilla", "Custom", SmokeName, RealismName};
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
    check_ui(svc_ui->pane_add_section(mod_ctx, panel, "Twilight prototype"));
    add_toggle(panel, "Automatic Twilight profile", auto_twilight);
    add_number(panel, twilight_transition);
    button.label = "Capture current look as Twilight target";
    button.on_pressed = capture_twilight_target;
    check_ui(svc_ui->pane_add_control(mod_ctx, panel, &button, nullptr));
    button.label = "Clear Twilight target";
    button.on_pressed = clear_twilight_target;
    check_ui(svc_ui->pane_add_control(mod_ctx, panel, &button, nullptr));
    check_ui(svc_ui->pane_add_section(mod_ctx, panel, "Camera prototype"));
    add_toggle(panel, "Modern exploration FOV", camera_toggle);
    add_number(panel, camera_fov_setting);
    add_number(panel, camera_transition_setting);
    add_toggle(panel, "Lower exploration camera", camera_lower_angle);
    add_number(panel, camera_angle_setting);
    check_ui(svc_ui->pane_add_text(
        mod_ctx, panel,
        "Default off. Changes vertical FOV only in native camera mode 0; demo, detached, "
        "targeting, aiming, and other nonzero camera modes keep native framing. The angle "
        "control changes native chase-controller latitude before smoothing and collision.",
        nullptr));
    check_ui(svc_ui->pane_add_section(mod_ctx, panel, "Atmosphere research"));
    add_toggle(panel, "Run one-shot depth/camera probe", atmosphere_depth_probe);
    check_ui(svc_ui->pane_add_text(
        mod_ctx, panel,
        "Developer diagnostic only. Records depth availability and matching camera metadata once; "
        "it does not change the image. Toggle off and on to run again.",
        nullptr));
    check_ui(svc_ui->pane_add_section(mod_ctx, panel, "Diagnostics"));
    add_toggle(panel, "Enable CPU diagnostics", diagnostics_toggle);
    button.label = "Reset CPU timing samples";
    button.on_pressed = reset_timing;
    check_ui(svc_ui->pane_add_control(mod_ctx, panel, &button, nullptr));
    check_ui(svc_ui->pane_add_text(mod_ctx, panel, "", &status_element));
    check_ui(svc_ui->pane_add_text(mod_ctx, panel, "", &detail_element));
    check_ui(svc_ui->pane_add_section(mod_ctx, panel, "Camera research diagnostics"));
    check_ui(svc_ui->pane_add_text(mod_ctx, panel, "", &camera_element));
    check_ui(svc_ui->pane_add_section(mod_ctx, panel, "Geometry research"));
    add_toggle(panel, "Log model catalog on resource load", geometry_diagnostics);
    add_toggle(panel, "Log topology for loaded models (developer)", topology_diagnostics);
    add_toggle(panel, "Mutation test: metal box only", geometry_mutation_test);
    add_toggle(panel, "Experimental smoothing: validated static objects",
               geometry_smoothing);
    add_number(panel, smoothing_angle_setting);
    check_ui(svc_ui->pane_add_text(
        mod_ctx, panel,
        "Developer test only. Enable before loading the target scene; "
        "enabling requires a scene reload. Disabling restores source normals.",
        nullptr));
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
    register_toggle(auto_twilight);
    register_toggle(geometry_diagnostics);
    register_toggle(topology_diagnostics);
    register_toggle(geometry_mutation_test);
    register_toggle(geometry_smoothing);
    register_toggle(geometry_skinned_smoothing);
    register_toggle(camera_toggle);
    register_toggle(camera_lower_angle);
    register_toggle(atmosphere_depth_probe);
    register_number(smoothing_angle_setting);
    register_number(detail_strength_setting);
    register_number(debug_mode_setting);
    register_number(split_setting);
    register_number(twilight_transition);
    register_number(camera_fov_setting);
    register_number(camera_transition_setting);
    register_number(camera_angle_setting);
    for (auto& effect : effects)
        register_effect(effect);
    register_presets();
    register_twilight_target();
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
bool geometry_diagnostics_enabled() { return geometry_diagnostics.value; }
bool topology_diagnostics_enabled() { return topology_diagnostics.value; }
bool geometry_mutation_test_enabled() { return geometry_mutation_test.value; }
bool geometry_smoothing_enabled() { return geometry_smoothing.value; }
bool geometry_skinned_smoothing_enabled() { return geometry_skinned_smoothing.value; }
float geometry_smoothing_angle() { return static_cast<float>(smoothing_angle_setting.value); }
bool camera_enabled() { return camera_toggle.value; }
float camera_fov_scale() { return static_cast<float>(camera_fov_setting.value) / 100.0f; }
float camera_transition_seconds() {
    return static_cast<float>(camera_transition_setting.value) / 100.0f;
}
bool camera_lower_angle_enabled() { return camera_lower_angle.value; }
float camera_angle_reduction() { return static_cast<float>(camera_angle_setting.value); }
bool atmosphere_depth_probe_enabled() { return atmosphere_depth_probe.value; }
bool passthrough_test() { return passthrough.value; }
std::int64_t split_percent() { return split_setting.value; }
grade::Prepared prepared_grade() {
    if (!auto_twilight.value || !has_twilight_target || twilight_weight <= 0.0f)
        return prepared;
    auto result = twilight::blend(prepared, twilight_prepared, twilight_weight);
    result.uniforms.debug_mode = prepared.uniforms.debug_mode;
    result.uniforms.difference_gain = prepared.uniforms.difference_gain;
    return result;
}
void update_diagnostics() {
    if (!diagnostics_toggle.value) {
        if (diagnostics_sampling_enabled)
            render::reset_timing_samples();
        diagnostics_sampling_enabled = false;
        diagnostics_log_written = false;
        return;
    }
    if (!diagnostics_sampling_enabled) {
        render::reset_timing_samples();
        diagnostics_sampling_enabled = true;
        diagnostics_log_written = false;
        return;
    }
    if (diagnostics_log_written || svc_log == nullptr || render::timing_sample_count() < 256)
        return;
    const auto data = render::diagnostics();
    const auto current = capture();
    unsigned active_mask = 0;
    for (unsigned i = 0; i < grade::Count; ++i)
        if (current.active[i])
            active_mask |= 1u << i;
    char message[1024];
    std::snprintf(
        message, sizeof(message),
        "Diagnostics sample: preset=%s grading=%s values=[%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld] "
        "active=0x%02x detail=%s/%lld%% status=%s size=%ux%u "
        "samples(active/disabled/neutral)=%llu/%llu/%llu draws(submitted/encoded)=%llu/%llu "
        "cpu_us(active_p50/p95/latest)=%.2f/%.2f/%.2f "
        "cpu_us(disabled_p50/p95/latest)=%.2f/%.2f/%.2f layout_us=%.2f resolve_us=%.2f "
        "config_us=%.2f",
        selected_preset.c_str(), master.value ? "on" : "off",
        static_cast<long long>(current.values[0]), static_cast<long long>(current.values[1]),
        static_cast<long long>(current.values[2]), static_cast<long long>(current.values[3]),
        static_cast<long long>(current.values[4]), static_cast<long long>(current.values[5]),
        static_cast<long long>(current.values[6]), static_cast<long long>(current.values[7]),
        active_mask, current.detail_enabled ? "on" : "off",
        static_cast<long long>(current.detail_strength), data.status, data.width, data.height,
        static_cast<unsigned long long>(data.snapshot_requests),
        static_cast<unsigned long long>(data.disabled_samples),
        static_cast<unsigned long long>(data.neutral_samples),
        static_cast<unsigned long long>(data.submitted_draws),
        static_cast<unsigned long long>(data.encoded_draws), data.active_p50_us, data.active_p95_us,
        data.callback_us, data.disabled_p50_us, data.disabled_p95_us, data.disabled_us,
        data.layout_us, data.resolve_us, config_update_us);
    svc_log->info(mod_ctx, message);
    diagnostics_log_written = true;
}
void update_twilight(twilight::State state, float elapsed_seconds) {
    twilight_state = state;
    if (!auto_twilight.value || !has_twilight_target) {
        twilight_weight = 0.0f;
        twilight_log_initialized = false;
        return;
    }
    twilight_weight = twilight::advance(twilight_weight, state, elapsed_seconds,
                                        static_cast<float>(twilight_transition.value) / 100.0f);
    if (!diagnostics_toggle.value) {
        twilight_log_initialized = false;
        return;
    }
    const int endpoint = twilight_weight <= 0.0f ? 0 : (twilight_weight >= 1.0f ? 1 : -1);
    if (svc_log != nullptr &&
        (!twilight_log_initialized || state != logged_twilight_state ||
         (endpoint >= 0 && endpoint != logged_twilight_endpoint))) {
        char message[160];
        std::snprintf(message, sizeof(message),
                      "Twilight automation: state=%s blend=%.0f%% target=captured",
                      twilight::label(state), 100.0f * twilight_weight);
        svc_log->info(mod_ctx, message);
        logged_twilight_state = state;
        logged_twilight_endpoint = endpoint;
        twilight_log_initialized = true;
    }
}
void shutdown() { status_element = detail_element = camera_element = preset_control = 0; }
} // namespace midnafx::settings
