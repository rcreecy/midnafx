# M3 correctness review

Reviewed against the pinned Dusklight SDK and host at
`edf42c6a7202647b56dd2fcdef02d17671bc814b`, especially
`sdk/include/mods/svc/{config,ui}.h` and
`src/dusk/mods/svc/{config,ui}.cpp`.

**Finding, fixed:** Selecting Vanilla or a saved preset overwrote the live Custom
settings. Switching back to Custom then kept the selected preset's values. The UI now
saves a separate Custom snapshot through ConfigService before leaving Custom, restores
it when selected again, and treats persisted live values as authoritative when Custom
was active at startup. The snapshot uses the same bounded versioned format as saved
looks. `presets_roundtrip` exercises the encoded Custom entry.

The review also checked the 16-entry and 8192-byte limits, strict row parsing,
duplicate-name rejection, value ranges, effect-toggle mask, and all-or-nothing decode.
The host copies dropdown option labels in `ui_control_set_options`; the temporary
option array is safe. `ConfigService::get_string` requires a buffer with room for the
NUL byte, which the loader provides. Writes and subscriptions run on the game thread;
batch preset application suppresses intermediate grade publications and publishes one
prepared model after all eight settings have been applied. No other actionable M3
correctness issue was found by source review.

In-game UI interactions and restart persistence still require a Dusklight runtime.
