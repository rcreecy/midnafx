# M3 persistence and presets design checkpoint (implemented)

The existing eight values and eight effect toggles are persisted individually by
ConfigService. M3 added a versioned, bounded preset collection and a selected-preset
name through the same official service. This avoids custom filesystem writes in the first
preset milestone; `HostService::data_dir` remains available when users need import/export
or larger profile files later. No new third-party parser is required.

Ship one immutable built-in **Vanilla** preset with neutral values. **Custom** represents
the current editable settings; Save stores their snapshot, Load restores an existing
snapshot, Duplicate creates a stable unique copy of the selected preset, and Reset restores
Vanilla. Other named environment profiles will only be added after visual/runtime evidence.
Changes to a live setting select Custom without touching GPU pipeline state. Selecting a
preset applies its complete validated model on the game thread before the next render
stage and writes ConfigService values after the model is complete. Only a bounded number
of presets and a bounded name/serialized size are accepted. A malformed persisted value
falls back to Vanilla/Custom with one diagnostic log entry, never a partial preset.

The host's `UiService::control_set_options` supports updating dropdown labels after a
duplicate or save without rebuilding the whole panel. Element handles are refreshed on
panel rebuild. All disk/config work remains on UI actions or initialization, never in
visible getters or the render callback.
