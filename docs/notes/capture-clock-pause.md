# Deterministic capture clock

## Cause and scope

The native `freeze` command only sets `g_dComIfAc_gameInfo.mPause`. It does not
stop the game clock or all environment work. In the M13 Ordon capture attempt,
actors stayed fixed while lighting and shadows changed between screenshots.

The existing clock already supports a zero time scale: `advance()` emits zero
simulation ticks while the main loop continues presentation and native UI.
However, the console rejected zero and `set_sim_rate()` clamped it to one Hz.
`patches/dusklight-capture-clock-pause.patch` exposes that existing capability
through `rate 0`. Negative console values remain rejected; positive rates retain
the existing 1–480 Hz clamp. `rate 30` restores the normal simulation rate.

This is a local host qualification patch against Dusklight
`edf42c6a7202647b56dd2fcdef02d17671bc814b`, not a MidnaFX package change.
It does not change defaults or replace the separate actor-freeze command.
Apply it with `git apply` from the pinned host checkout. The menu focus patch
is also needed for the tested console-to-menu workflow.

## Verification on 2026-10-11

The patched RelWithDebInfo host builds successfully. Reverse-application checking
confirms the saved patch matches the local source. Live D3D12 evidence is under
`build/m13-aa-runtime/lut-clock-zero`, with package hash in `session.json`.

1. Launch Ordon (`F_SP103,0,27,0`) with grading enabled, zero exposure, 20% detail,
   Neutral, and AA disabled. No actor-freeze command is used.
2. Enter `rate 0` in the native console. The console reports `Sim rate: 0 hz`.
3. Open the native mod panel and disable enhanced water. DOF stays disabled.
4. Capture Neutral/AA-off twice, then Neutral/AA-on, then Natural/AA-on at 100%
   twice. Dismiss the menu before every scene capture.
5. Enter `rate 30`. The console reports `Sim rate: 30 hz`; Link and the chicken
   visibly resume animation. Close the test host normally.

Both unchanged repeat pairs have zero decoded RGB differences in the central
scene, upper geometry, character, and sampled HUD rectangle. Rectangles and
measurements are saved in `comparison-metrics.json`. This establishes repeatable
static captures for this scene and settings, not every scene or animated effect.

The capture provider returns JPEG. Files in this run use the `.jpg` extension;
earlier runs incorrectly used `.png` for the same encoded payload. Their decoded
statistics are screenshot diagnostics, not lossless render-target measurements.
Compression can alter pixels around HUD text and other edges even when the
underlying UI is unchanged. Exact LUT neutrality remains covered by GPU readback.

## Visual disposition

The matched Ordon pair shows AA softening both geometric edges and fine scene
detail, including clothing and vegetation. This is evidence of the tradeoff,
not sufficient evidence that the default improves the complete scene corpus.
Natural adds a restrained tonal change in this scene. Central-region mean
absolute decoded RGB differences are 2.084 code values for AA and 1.231 for
Natural; these are change magnitudes, not quality scores. The native HUD remains
readable. No claim of exact HUD pixel identity follows from JPEG captures.

AA remains experimental and default-on under the existing authorization.
Creative looks remain experimental and default-neutral. Next qualification work
is the same controlled comparison in representative additional scenes, followed
by motion and active-Twilight transition checks. Clock pause alone does not prove
wall-clock-driven mod effects stop; repeat controls are required per scenario.
