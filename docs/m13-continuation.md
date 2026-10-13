# M13 continuation

Starting commit: `3f39347b104cd73558c48841ff76b6072ab45e5a`, branch
`codex/m12-water`, clean main repository. The ignored Dusklight checkout retains
intentional camera and native-menu patches. Continue on this branch: `main`
does not contain the shipped M13 work. Do not reset or rewrite either release tag.

## Recovered evidence

| Workstream | Implemented | Verified | Remaining |
|---|---|---|---|
| Release/branch | v1.2.0 Windows and Intel Mac shipped | Asset hashes checked; source-reviewed tag delta | Future release authorization |
| AA correctness | Five-tap filter, alpha preservation, bounded detail | D3D12 GPU tests; 25 Metal cases repeated three times | No new demonstrated shader defect |
| AA visuals | Experimental default-on | Live smoke only | Matched state and motion corpus |
| AA/detail/water | Shared neighbor reads; water optics follow AA | Detail GPU tests, water shader compilation, live absorption log | Water optical readback and boundary comparison |
| AA lifecycle | Independent toggle; saved false preserved | Live toggles/reload/pond warp, source-reviewed ownership | Complete resize/restart matrix |
| Windows D3D12 | Shipped | 16 tests, live captures | Temporal/art qualification; GPU timing |
| macOS Metal | Shipped Intel artifact | CI 38017063663, 16 tests plus three pixel repeats | Physical gameplay unavailable; original failure unresolved |
| LUT color contract | No earlier implementation found | Source architecture reviewed | Contract and implementation below |
| Identity LUT | Not implemented at recovery | None | GPU neutrality gate |
| Natural/Twilight LUT | Not implemented at recovery | None | Only after identity gate |
| LUT settings | Not implemented at recovery | None | Default-neutral, independent persistence |
| Combined performance | Existing single color snapshot | CPU callback diagnostics only | LUT GPU cost and live combinations |

AA disposition: requires bounded visual qualification work, not a replacement
algorithm. Retain the explicitly requested default-on and experimental label;
do not describe it as visually qualified. No evidence justifies SMAA or TAA.

The original Metal failure has not been repaired: unchanged production code
passes later hosted runs. Diagnostics and an until-fail repeat gate preserve
future failures. Physical Mac access is unavailable, as confirmed by the user.
This is a qualification limitation, not a blocker to independent LUT work.

Existing artifacts: `build/m13-aa-runtime/release120-exposure-control`,
`release120-pond-transition`, and `release120-motion-pond`. The last run's input
capture failed; it is not motion evidence. `Water absorption active` proves the
enhanced-water path executes. Zero `marked_draws` measures diagnostic overrides.

## LUT contract

Work in the existing SDR scene-code domain: RGBA8/BGRA8 **UNORM**, not sRGB
texture formats. Preserve normalized values as supplied by the native snapshot;
do not assert a measured sRGB transfer function or insert a gamma conversion.
Built-in tables are authored for this numeric domain. External `.cube` import
is outside this change because its transfer/domain cannot be inferred safely.

Preserve current pass order: source AA/detail, existing water optical response
where enabled, existing parametric grading, creative LUT, existing downstream
DOF, native HUD. Do not move AA after water or introduce a full-resolution pass.
No separate final trim is added; existing independent grading controls remain
the technical adjustment before the look.

The LUT domain is RGB [0,1]. Clamp RGB to that domain at lookup; preserve source
alpha outside the LUT. RGBA16Float 33-cubed textures encode RGB and constant
one in the unused alpha channel. A linear clamp sampler uses coordinates
`(clamp(rgb,0,1) * 32 + 0.5) / 33`. Lattice values at multiples of 1/32 are
exactly representable in binary16. Identity acceptance allows at most one
RGBA8 RGB code value for interpolation/quantization, with exact alpha.
Disabled and zero-intensity paths must preserve the old output exactly.

Cache tables and GPU resources; generate/upload only during resource creation.
No CPU table generation per frame. Retain resources for queued callbacks and
release them with their owning renderer resources after callbacks are drained.
Diagnostics and passthrough bypass the creative transform. Grading disabled
also disables the look without overwriting selection or intensity.

Default Neutral remains an exact bypass. Creative looks stay opt-in and
experimental until representative art-direction captures exist. Twilight must
blend sampled results using existing semantic timing, not switch textures.

## Implemented candidate

Implementation commit: `fb84ba2dd2fdc5a684ccf8216747590213a0f316`.
The identity path passed its initial GPU neutrality gate before creative tables
were added. The expanded Windows suite subsequently passed all 16 tests on
2026-10-10, including 25 existing AA patterns, 288 identity comparisons across
four dimensions, eight ordinary/water entry points, three exposure gains and
three strengths. Additional cases cover exact black/white/primary/secondary
identity in RGBA8 and BGRA8, low code values, binary16 rounding, monotonic
creative ramps, endpoints, alpha, zero-strength bypass and the Twilight midpoint.
The synthetic water fixture has a nonzero surface mask and separated depths;
it exercises absorption rather than an empty-water fallback. It does not qualify
refraction, shoreline, or authored lighting visually.

Local test artifact: `build/m13-aa-runtime/lut-gpu-tests.log` (42.35 seconds total;
41.83 seconds for GPU tests). These are test execution times, not shader timings.
The tested package SHA-256 is
`AC031DACAE1403002348A497D0186A1AE600922463334D7F180F94D7114D19E9`.

[CI run 38082338298](https://github.com/rcreecy/midnafx/actions/runs/38082338298)
builds this exact implementation commit and passes all 16 tests on both Windows
and Intel Mac. Metal additionally passes three consecutive complete pixel suites
on Apple Paravirtual hardware, including all new LUT cases; no GPU test is skipped.
The release job is skipped because this is a branch workflow run. Local archived
CI output is `build/m13-lut-ci.log`. This validates hosted Metal execution, not
physical Intel Mac gameplay or a fix for the historical intermittent failure.

Natural uses a small endpoint-preserving tonal curve. Twilight applies restrained
channel offsets to the Natural table. Both are candidates, not artistically
qualified looks. Natural samples one table; active Twilight samples two and
blends their results. The semantic weight advances independently of the older
user-captured Twilight grading target. Normal, spot, and unavailable states move
toward Natural. Existing transition duration controls the change.

Basic exposes Color Look and Look Intensity. Their integer configuration values
are independent of legacy saved grading profiles, including custom profiles.
Profile switching therefore preserves the chosen creative look, while a profile
that disables grading also bypasses it. Older configuration files acquire Neutral
and 100% intensity defaults without altering existing settings. Invalid look IDs
resolve to Neutral; intensity is clamped to 0–100. There is no preset-schema
migration and no external LUT parser. Developer controls expose bypass, resource
counts, selected table, domain, format, and semantic blend.

## Resources and performance

Each cached render-target layout owns two 33³ RGBA16Float textures: 574,992
uncompressed texel bytes, excluding allocation overhead. Eight look pipelines
extend the existing fourteen, for twenty-two compiled pipelines per layout.
Resources are created with the existing layout error scopes, reused per frame,
and released after unregistering callbacks during unload. The initialization
tradeoff is explicit: even Neutral prebuilds these variants and tables. Neutral
uses the existing entry points and performs no LUT lookup. No extra scene copy
or fullscreen pass is added by the LUT.

No GPU timestamp measurement is available through the current GfxService.
CPU callback timings must not be interpreted as LUT GPU cost. Resource counters
measure cached layout sets, not total driver memory; an allocation soak and
GPU-duration comparison remain open.

## Live evidence

`build/m13-aa-runtime/lut-natural-water-dof` contains the isolated configuration,
package hash, host logs, and three screenshots. Fishing Pond executes Natural,
AA, enhanced-water absorption and DOF together on Intel UHD 630 / D3D12 at
1216×896. Basic controls display the saved look and intensity. Switching to
Twilight and reloading preserves the selection. Reload logs record 6,714 and
906 LUT draws with one cached layout each. Disabling grading, reloading, and
closing records zero LUT draws with Twilight and 100% still saved. Water capture
reports zero failures across these reload intervals; no GPU validation error was
found. These captures establish operation and visible HUD preservation, not
frame-matched art quality. The package in this first live run predates the final
diagnostic-label and subnormal-rounding corrections; its exact hash is in
`session.json`.

`build/m13-aa-runtime/lut-invalid-id` tests the final package. A saved ID of 99
displays Neutral while preserving 37% intensity. Selecting Natural works, and
maximizing/restoring the window continues rendering. The final log records
535,645 LUT draws and one cached layout, followed by all mods unloaded; water
capture reports 538,285 captures and zero failures. This demonstrates extended
execution without layout-cache growth, not a driver-memory leak measurement.
Restarting the same saved configuration succeeds (`build/m13-aa-runtime/lut-restart`).
Native `freeze` succeeds when entered as individual key events; the Unicode
text-entry method does not populate this host input. The host confirms "Global
freeze on" and subsequent captures keep the same actor/camera state.

The resulting F1 failure was isolated to host focus routing, including after an
empty console was closed without executing `freeze`. Event tracing showed the
inactive console retained the key target. Host patch commit `b4e535b` routes only
the menu command from that inactive console (or the context root) while MenuBar
is the top document. Rebuilt-host checks confirm active-console behavior remains
unchanged and F1 can reopen/dismiss the menu after `freeze`. See
[the host repair record](notes/menu-root-focus-repair.md). This patch is separate
from the MidnaFX package.

`build/m13-aa-runtime/lut-menu-console-fixed` contains Natural/AA-on,
Neutral/AA-on, and Neutral/AA-off Ordon captures with enhanced water disabled.
The camera and actors remain fixed, but lighting/shadows still vary. Two
unchanged Neutral/AA-off captures differ by a mean 14.403 RGB code values within
the central scene rectangle (x=400..899, y=180..799), excluding the HUD and cursor.
That exceeds either effect comparison in this region, so these images cannot
establish attributable AA or LUT differences. `comparison-metrics.json` also
records upper-geometry and character regions. These are comparison diagnostics,
not quality scores. The native command sets `mPause`; it does not establish a
fully deterministic rendered frame. Lighting/time control and another unchanged
repeat check are required before accepting matched visual evidence.
The repaired host closes cleanly with 437,349 LUT draws, one cached layout, and
all mods unloaded. No test host remains running.

### Controlled clock follow-up, 2026-10-11

The [capture-clock host patch and procedure](notes/capture-clock-pause.md) expose
the existing zero-speed clock through `rate 0`, preserving UI and presentation.
Unlike actor freeze, it produces identical decoded repeat captures in the tested
Ordon regions, both with Neutral/AA-off and Natural/AA-on. `rate 30` resumes
animation. The native mod panel works throughout. Evidence is in
`build/m13-aa-runtime/lut-clock-zero`.

Matched comparisons show the AA edge/detail-softening tradeoff and a restrained
Natural tonal change. This closes the initial static-capture control prerequisite
for Ordon, not the complete visual corpus or temporal gate. The capture provider
encodes JPEG, including older artifacts misnamed `.png`; screenshot differences
must not be treated as lossless shader readback or exact HUD identity evidence.

## Current release disposition

The [Pond follow-up](notes/capture-clock-pause.md#fishing-pond-follow-up) adds a
second repeatable static scene with enhanced water disabled. Enhanced-water
animation continues on its own clock, so combined matched captures remain open.
The [Twilight round-trip record](notes/lut-twilight-transition.md) verifies live
activation to 100%, fallback to 0% during loading and in Ordon, and reentry to
100% in Kakariko without a captured legacy target. Sparse independent LUT logs
close that observability gap. The subsequent
[host focus repair](notes/menu-root-focus-repair.md#hidden-panel-recovery-after-console-warp)
also restores the hidden Mods panel after console warp. A controlled trace,
rebuilt-host reproduction, and repeated F1 toggles verify that workflow.
The [combined lifecycle smoke check](notes/lut-combined-lifecycle.md) additionally
passes Natural, AA, enhanced water, and DOF together through maximize/restore and
mod reload in Fishing Pond on Windows D3D12. This covers one combined case;
it does not close the full lifecycle matrix or driver-memory gate.
The same record includes a subsequent creative GPU control matrix: all eight
shader entry points in both color formats pass zero-intensity bypass, alpha,
and blend/intensity midpoint checks on local D3D12. Hosted
[CI run 38409881321](https://github.com/rcreecy/midnafx/actions/runs/38409881321)
then passed 16/16 tests on Windows and Intel macOS at `8d66a35`, plus three
consecutive expanded Metal pixel runs without skips. The release job was skipped.

Conditional development candidate; not a production-qualified LUT release.
No release tag or version bump is part of this work. The published v1.2.0 assets
remain unchanged. AA retains its previously authorized experimental default;
no new evidence warrants an algorithm change or claims full visual acceptance.

Outstanding qualification gates:

- Frame-matched AA and LUT comparisons across the requested scene corpus,
  including temporal shimmering, fine geometry, characters, underwater and HUD.
- Frame-by-frame Twilight transition quality and timing, save/load, a complete combination matrix,
  broader combined-water/DOF resize coverage and driver-memory growth measurements.
- Physical Intel Mac gameplay. Hosted Metal execution is a separate evidence
  level; the original pre-LUT intermittent failure still has no proven cause.
- Measured GPU duration for baseline versus combined effects.

Extend the verified clock-pause procedure to the remaining scene corpus, with
unchanged-repeat controls per scene, then complete motion, transition/lifecycle
coverage and GPU instrumentation. Do not expand into
SMAA, TAA, external LUT import, or new water effects to substitute for these gates.
