# LUT Twilight transition verification

## Evidence on 2026-10-12

The isolated Faron startup `F_SP108,1,0,0` displays the Twilight look selected at
100% intensity but reports a 0% Twilight blend. It enters native dialogue.
`build/m13-aa-runtime/lut-faron-state-check/look-diagnostics.jpg` records that
state. Selection and stage eligibility alone are not proof of active Twilight.

Kakariko startup `F_SP109,0,14,0` reports 100% Twilight blend with bypass off,
one cached layout, and advancing look draws. The diagnostic and scene images
are in `build/m13-aa-runtime/lut-kakariko-state-check`. Warping to Ordon works,
but F1 fails to reopen the native menu afterward, including after refocusing the
scene. The previous host focus repair does not cover this workflow. Its cause
is not established; it is not evidence of a shader failure.

## Independent transition diagnostics

The existing Twilight logs require a captured legacy grading target. Creative
LUT transitions deliberately do not require that target, leaving them unlogged.
The settings diagnostics now report creative state changes and blend endpoints
when CPU diagnostics are enabled and the Twilight look is selected. There is
no per-frame logging. The field is named `requested_blend` because grading-off
or diagnostic bypass can still suppress the actual look draw.

The Windows package rebuild succeeds. The nearest existing gates pass:
`presets_roundtrip`, `visual_parameters`, and `twilight_profile` (3/3).
No shader, lookup table, default, or blend calculation changes in this patch.
The earlier Windows/Metal GPU results remain evidence for the unchanged shader
implementation; this diagnostic-only revision has local Windows verification.

## Live round trip

`build/m13-aa-runtime/lut-transition-log` uses package SHA-256
`ABEE0EB4F0DE421A6AC8D0FA6CF6D0D497860800B7DBE724B331F03C14046AA6`.
AA, grading, enhanced water, and Twilight at 100% intensity are enabled; DOF is
disabled. No legacy grading target is captured. Native console commands perform
the following transitions:

1. Kakariko startup: unavailable at 0%, then Active Twilight at 3.2%, then 100%.
2. `warp F_SP103 27 0 0`: unavailable at 93.6%, then 0%, then Normal at 0%.
3. `warp F_SP109 14 0 0`: unavailable at 0%, then Active Twilight at 3.7%, then 100%.

Both destinations render normally after loading, with visible native HUD.
Shutdown records 3,296 look draws, one cached layout, 3,124 water captures with
zero failures, and all mods unloaded. Only nine creative transition lines are
emitted across the run, confirming sparse logging in steady scenes.

This verifies semantic activation, loading fallback, exit, and reentry without
a legacy target. It does not measure exact transition duration, establish smooth
frame-by-frame art quality, validate a save reload, or replace physical Mac
gameplay checks. Those qualification gates remain open.
