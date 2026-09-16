# UI, configuration, presets and lifecycle research

Research revision: Dusklight `edf42c6a7202647b56dd2fcdef02d17671bc814b` (local checkout `upstream/dusklight`, 2026-09-16). Every path below is relative to that checkout; conclusions are pinned to this revision.

## Native host UI

Evidence: `docs/modding.md`, **UiService**; `sdk/include/mods/svc/ui.h`, `UiService`, `UiControlDesc`, `UiModsPanelDesc`; implementation `src/dusk/mods/svc/ui.cpp`, `ui_register_mods_panel`, `ui_pane_add_control`, `ui_remove_mod`.

The supported mod UI is host-owned RmlUi-style documents (RML and RCSS), exposed through a versioned C service, not an exported ImGui API. Current UI service is 2.3. Native Mods panel registration is:

```cpp
ModResult (*register_mods_panel)(ModContext*, const UiModsPanelDesc*);
using UiPaneBuildFn = ModResult (*)(ModContext*, UiElementHandle pane, void*, ModError*);
using UiPanelUpdateFn = ModResult (*)(ModContext*, void*, ModError*);
ModResult (*pane_add_control)(ModContext*, UiElementHandle, const UiControlDesc*, UiElementHandle*);
```

Initialize every descriptor with its `UI_*_INIT` macro. Build callbacks reconstruct controls when tabs/panels rebuild. All element handles become invalid on rebuild; reacquire them each time. Calls run on the game thread. Visible getters, predicates, and panel updates run every frame and must only read cheap cached state. A non-OK build/update result or callback exception fails the mod.

Controls include buttons, toggles, integer number steppers, strings, selects, colors, groups, file pickers, icon buttons and dropdowns. There is no float slider primitive. Config binding requires toggle=bool, number/select/dropdown=int, string/color/file-picker=string. Float settings must use callbacks with integer scaling (e.g. hundredths), or store explicitly scaled integer config values. `UiControlValue` carries bool, int64 and string, not double. Number `min == max` selects default bounds; step below 1 becomes 1.

Use a Mods panel with enable, preset, per-effect toggles, scaled controls and diagnostics. Optional tabbed windows provide contextual help. Do not store temporary user_data addresses; callbacks retain those pointers. `register_mods_panel` replaces the current panel; host teardown removes panel/windows/styles/tabs. `UiWindowDesc::on_closed` is explicitly not called during owning-mod teardown.

## Persistence

Evidence: `docs/modding.md`, **ConfigService**; `sdk/include/mods/svc/config.h`, `ConfigService`, `ConfigVarDesc`, `ConfigChangedFn`; `src/dusk/mods/svc/config.cpp`, `config_register_var`, `config_set_float`, `config_flush_if_dirty`, `config_remove_mod`.

ConfigService 1.0 persists bool/int64/double/string values in host config.json under `mod.<escaped mod id>.<name>`. Name fragments are 1–64 ASCII letters, digits, underscores or hyphens; `enabled` is reserved by the loader, so use `grading_enabled`. Relevant signatures:

```cpp
ModResult (*register_var)(ModContext*, const ConfigVarDesc*, ConfigVarHandle*);
ModResult (*get_float)(ModContext*, ConfigVarHandle, double*);
ModResult (*set_float)(ModContext*, ConfigVarHandle, double);
ModResult (*subscribe)(ModContext*, ConfigVarHandle, ConfigChangedFn, void*, ConfigSubscriptionHandle*);
ModResult (*get_string)(ModContext*, ConfigVarHandle, char*, size_t, size_t*);
```

Persisted and command-line values are applied silently during registration: explicitly read and sanitize every setting after registering, then subscribe. Changes notify synchronously on the game thread; identical writes do not notify, and recursively setting the same var applies without a second notification. Callback snapshots expire at return. Type mismatch yields MOD_INVALID_ARGUMENT; duplicate live names yield MOD_CONFLICT. Bounds are a MidnaFX responsibility: `config_set_float` directly calls `setValue` without range/finite validation.

Writes mark dirty; `config_flush_if_dirty(false)` runs at frame end with a two-second minimum interval since the last save; shutdown forces a save. This is a throttled flush in implementation, despite the API using the word debounced. No public flush method or transactional batch setter exists. Cache a validated complete grading snapshot and apply presets as a complete model update before publishing it to rendering; avoid rendering half-applied presets via subscription callbacks. Persisted values survive unregister/reload; var and subscription handles do not.

## Presets and filesystem

Evidence: `sdk/include/mods/svc/host.h`, `HostService::data_dir`, `mod_dir`; `src/dusk/mods/svc/host.cpp`, `host_data_dir`; `sdk/include/mods/svc/file.h`, `FileService`; `docs/modding.md`, **ResourceService**.

```cpp
ModResult (*data_dir)(ModContext*, const char** out_path); // HostService 2.2
const char* (*mod_dir)(ModContext*);                       // temporary scratch
ModResult (*read_all)(ModContext*, const char* location, FileBuffer*);
ModResult (*write_all)(ModContext*, const char* location, const void*, size_t);
ModResult (*create_child)(ModContext*, const char* folder, const char* name, const char**);
```

`data_dir` lazily creates persistent `ConfigPath/mod_data/<mod id>`, returns an absolute path valid until shutdown returns, and can fail (out pointer null). `mod_dir` survives reload/disable within a session but is wiped at game startup: it is unsuitable for user presets. `native_dir` is read-only packaged runtime location. ResourceService reads immutable bundle `res/` files; paths are relative, cannot be absolute or contain `..`.

FileService 1.0 is for user-picked locations, which are opaque strings: preserve them and use `join`, never concatenate manually. Calls are synchronous on the game thread except native picker completion (also delivered on the game thread); avoid all file I/O in frame/UI getters. Pick cancel is MOD_UNAVAILABLE, another pending picker is MOD_CONFLICT. `write_all` truncates an existing location and is explicitly non-atomic; `create_child` does not replace an existing child. Check close/flush results. `join`/`create_child` result storage expires on the next such call; copy immediately. Free FileBuffer with the service.

Suggested design: ship named builtin presets as fixed data; persist selected name and current scalar values with ConfigService. Store optional versioned custom preset documents under HostService data_dir using bounded parsing and temporary-file replacement. User-selected import/export can use FileService later. The SDK provides no dedicated preset manager API.

## Logging, errors, optional services and reload

Evidence: `sdk/include/mods/svc/log.h`, `LogService`; `sdk/include/mods/svc/host.h`, `HostService::fail`, `get_service`; `sdk/include/mods/api.h`, lifecycle function typedefs; `src/dusk/mods/loader/loader.cpp`, `ModLoader::deactivate_mod`, `reload_runtime_mod`; `docs/modding.md`, **Importing Services**.

LogService 1.0 exposes `void (*write)(ModContext*, LogLevel, const char*)` plus trace/debug/info/warn/error; UTF-8 messages are copied and attributed to the mod ID. Log capability failures once and show a cached UI status, not per frame. Host `fail(ModContext*, ModResult, const char*)` schedules disable at the next safe point and immediately stops resolving the mod's services; reserve it for unrecoverable failures.

Lifecycle exports have `ModResult mod_initialize(ModError*)`, `mod_update(ModError*)`, and `mod_shutdown(ModError*)`. Loader teardown first notifies services of deactivation, calls shutdown if initialized, removes exported services, detaches per-service state, then unloads the library. Reset local handles and release owned resources on shutdown; UI/config host cleanup is automatic. Packaged mods can reload; an in-place native library returns an explicit cannot-be-reloaded failure in `reload_runtime_mod`. Do not promise reload for every development loading mode.

Default `IMPORT_SERVICE` requires the header's latest minor. Use `IMPORT_OPTIONAL_SERVICE_VERSION` with a chosen minimum for optional UI and feature-check `SERVICE_HAS` before later members. `HostService::get_service` is a dynamic lookup and has no initialization-order guarantee. A missing optional UI should leave defaults/config rendering functional; missing persistent data_dir should disable custom preset storage while retaining builtin presets.

## Game-state service distinction

Evidence: `sdk/include/mods/svc/game.h`, `GameService`; `sdk/include/mods/svc/stage.h`, `StageService`; `sdk/include/mods/svc/game_mode.h`, `GameModeService`.

GameService 2.0 contains only a ServiceHeader: it is a game-struct ABI epoch check automatically imported for `FEATURES game`, not a scene-state getter API. StageService edits stage actor records; it does not detect the current Twilight environment. GameModeService registers game modes; it is not a generic environment-change notification API. Automatic profile selection must use evidence-based game symbols/hooks from the separate environment research, isolated from core settings/UI; service-only manual grading need not require game ABI access.
