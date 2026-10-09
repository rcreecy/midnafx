# M13 anti-aliasing investigation

## Current prototype

The [correctness and optimization review](notes/m13-aa-correctness-review.md)
records the repaired grading/detail behavior, 25 GPU pixel cases, and the
user-requested default-on decision. Dated default-off checkpoints below are
historical; they do not describe the current default.

M13 uses a conservative FXAA-style fullscreen path as the first post-process
candidate.  It reuses the existing `resolve_pass` color snapshot and MidnaFX
fullscreen draw infrastructure; it creates no texture, sampler, bind group
layout, or pixel-sized intermediate owned by the mod.  Pipeline pairs remain
keyed by the host render-target layout and are retired with the existing
renderer lifecycle.

The `anti_aliasing` setting now defaults on, following the explicit user decision
after the correctness review below. It is available in Basic settings. Existing
saved false values are preserved. It is disabled for
diagnostic and passthrough views.  Unsupported layouts, failed resolve calls,
and non-single-sample targets retain the native frame through the existing
fail-closed renderer path.

The filter reads center plus four axial neighbours.  It leaves low-contrast
regions unchanged and caps edge blending at 40 percent.  This deliberately
avoids the aggressive whole-image softening associated with a high-strength
FXAA preset.  It has both ordinary and detail variants.  When enhanced water is
active, the corresponding water pipeline filters the resolved current scene
before applying the water's bounded optical response at the center pixel.

## Ordering

The implemented order is:

```text
current resolved scene, including native water
  -> FXAA edge filter
  -> optional detail on neighborhoods rejected by the AA edge detector
  -> optional enhanced-water optical response
  -> parametric grading
  -> optional DOF
  -> HUD
```

This is the safe one-snapshot ordering available in the current host API.  It
prevents sharpening before AA.  It is not yet a claim that enhanced-water
boundaries are visually ideal: that needs source-matched runtime A/B captures.
A two-pass final-water AA experiment is deferred unless those captures identify
a material issue, because it would add a second full-resolution snapshot and
pass break.

## SMAA disposition

SMAA remains unimplemented.  A correct SMAA 1x implementation needs search and
area textures, their package provenance and validation, plus edge, blend-weight
and neighbourhood passes.  No runtime comparison yet demonstrates that this
extra resource/pass complexity is justified over the conservative FXAA path.

## Static proof

The Windows release package builds successfully.  The shader compilation test
creates all FXAA and water-FXAA pipelines with Dawn, in addition to the existing
shader suite.  The complete 15-test CTest suite and package contract pass.

## Required runtime qualification

The prototype is not a production AA strategy yet.  It needs source-matched
Windows captures comparing off/on at identical camera state, covering outdoor,
interior, dungeon, water, camera movement, grading/detail, DOF, HUD, resize,
transition, feature disable/re-enable, mod unload, and shutdown.  Tests must
inspect WebGPU validation logs and determine whether water boundaries, fine
foliage, ropes, hair, and high-frequency TP textures remain acceptable.

## Runtime session recovery (2026-10-07)

The new chat successfully initialized Windows Computer Use through `@oai/sky`.
Native window discovery, launching the source-matched host, screenshots, mouse
navigation, and closing the host all worked. The earlier browser-only discovery
failure no longer blocks this session.

The installed test package has SHA-256
`EA8A2B073A8D17CD43478EE6332BBC4ADA1764CE378339C48C494BF162E3FA8B`.
The host is `build/runtime-host/RelWithDebInfo/dusklight.exe`, built against the
pinned SDK checkout with the existing camera-service changes. The D3D12 title
sequence loaded MidnaFX and enhanced water at 1216x896, then logged clean mod
unload. No WebGPU validation error appeared in this run. The host's update check
reported an unrelated inability to parse `UNKNOWN-VERSION`.

Evidence and the previous installed package/config are saved under
`build/m13-aa-runtime/`; `startup-shutdown.log` records this first run. This is
startup/shutdown evidence only: AA was not enabled and no matched comparison was
captured.

Port 1 initially had no device assigned. Selecting Keyboard through the UI
succeeded. Automated Esc/F1 input did not produce the expected visible menu
response, including an F1 retry after explicit activation. The exact input
failure remains unconfirmed; a manual keyboard check is pending. The restarted
host was left at input settings at that checkpoint. The follow-up below
supersedes the pending manual check; AA qualification and the subsequent LUT
gate remain open.

## Unattended Windows smoke runs (2026-10-08)

Five isolated runs now provide runtime evidence for the unchanged `5d9ada3`
package. Each uses a separate user/mod/log directory beneath
`build/m13-aa-runtime/`, with autosave disabled, neutral grading enabled,
detail enabled at 20 percent, enhanced water enabled, no texture replacements,
and no other mods. The host reports the pinned revision, D3D12, Intel UHD
Graphics 630, and driver `31.0.101.2140`. Initial render size is 1216x896.
The local `launch.ps1` records stage, settings, executable, arguments, and
package hash in each run's `session.json`; `results.json` summarizes logs.

| Run directory | Stage | AA | DOF | Evidence |
| --- | --- | --- | --- | --- |
| `outdoor-off` | `F_SP103,0,27,0` | off | off | `baseline.png` |
| `outdoor-on` | `F_SP103,0,27,0` | on | off | `aa-on.png`, `aa-resized.png`, `aa-restored.png` |
| `dungeon-aa-dof` | `D_MN01,3,0,0` | on | on | `aa-dof.png` |
| `pond-aa` | `F_SP127,0,0,0` | on | off | `aa-on.png` |
| `pond-off` | `F_SP127,0,0,0` | off | off | `aa-off.png` |

All five runs reached scene rendering and logged `all mods unloaded` after
window close. Renderer diagnostics report submitted and encoded pre-HUD draws;
water logs confirm surface capture and absorption. The dungeon capture visibly
shows background DOF with a sharp HUD. The outdoor AA run maximized and restored
the window, producing a wider scene and then the original size without visible
corruption. The maximized window capture is 1614x974 including its title bar;
this is not a claim of 1440p or 4K validation.

No WebGPU validation or fatal error was found in these five stdout/stderr logs.
Every isolated run reports `Failed to open file: gczelda2` because its fresh user
directory has no save file. Both AA-off and AA-on runs also report buffer mapping
aborts and `Device lost: Device was destroyed` after clean mod unload during
host teardown. These messages are retained in the evidence; the result is not
an entirely warning-free host run.

The captures show plausible scene output, intact HUD, and no obvious gross
corruption. They do **not** qualify fine-detail retention or temporal quality:
off/on frames came from separate launches with different animation and lighting
times. No pixel-difference or GPU-performance claim follows from these captures.
The outdoor baseline was frozen through the native game console; the other
runs were not synchronized to that simulation state.

Keyboard access is partially functional rather than wholly unavailable. `/`
opens the native console, individual letter-key calls enter text, `Enter`
executes the console command, and `Shift+F1` toggles the developer menu.
`Return`, ordinary `F1`, and bulk `type_text` did not produce the expected
response in the observed cases. The cause remains unproven. No host input code
was modified to work around it.

Remaining gates include exact-state AA comparisons, camera movement and
shimmering, fine foliage/ropes/hair, live disable/re-enable, mod reload, and
in-process stage transitions. The five runs cover stage startup, not transitions
between stages. AA remains default-off and LUT work remains deferred. All test
hosts are closed; the previously installed package and application config were
restored from the recovery backups. Port 1's keyboard assignment from the setup
check is retained.

## Instrumented lifecycle checks (2026-10-08)

A temporary host-only hook extended the unattended checks without changing the
MidnaFX package. It ran on the game thread, changed the registered AA boolean
through its normal setter, queued the normal mod reload, and invoked native
stage/camera commands. These checks used isolated settings and log directories.
They supplement the uninstrumented smoke runs; they are not a replacement for
visual qualification on the restored host.

The `lifecycle-live` log records AA disable/re-enable acknowledgements, mod
deactivation and reactivation, and `1/1 mod(s) active` after reload. Subsequent
renderer diagnostics report 256 submitted and 256 encoded pre-HUD draws.
In-process transitions from `F_SP103` to `F_SP127` and then `D_MN01` reached
stage-specific water classification in the renderer. The persisted AA setting
remained true. A three-second native camera move changed the reported eye from
approximately `(0, 1677.77, 3020.69)` to `(100, 1677.77, 3020.69)`.

Desktop capture failed during this run with access-denied and monitor-capture
errors. Therefore these are log-based lifecycle results only: there is no
verified screenshot after the live toggles, reload, transitions, or camera
move. The log contains the expected missing-save error but no WebGPU validation
or fatal error. The task-owned process was terminated for cleanup after desktop
access failed; this run does not establish clean shutdown. Earlier smoke runs
provide the clean mod-unload evidence.

Two attempted methods did not establish identical-state comparisons. In
`matched-live`, native `freeze` held actors but lighting continued to change;
even consecutive AA-off captures differed. Those images are unsuitable for
pixel-identity or AA-only difference claims. In `held-live`, an experimental
host change that suppressed simulation ticks ended without clean unload. That
experiment is excluded from product qualification, and its failure is not
attributed to MidnaFX without reproduction on the normal host.

The temporary hook and simulation experiment were removed. The original host
source was restored byte-for-byte from its backup, preserving the pre-existing
camera-service edits. The restored host rebuilt successfully, all test hosts
are closed, and the package hash remains unchanged. Local evidence remains
under `build/m13-aa-runtime/`.
The instrumented lifecycle executable had SHA-256
`A5BF346D4713CCB25ECE7ABA6EE69DAE4E2A018E2C91B2D9A8211526A78586DF`.

AA remains default-off. Exact-state off/on comparisons, temporal shimmering,
fine-detail retention, and visual validation of the live lifecycle operations
remain open. LUT work is still deferred pending the AA qualification decision.

## Headless GPU pixel checks (2026-10-08)

The next phase exercises the production grading WGSL directly on a real GPU,
without a window, RDP session, game process, or host instrumentation. The
`aa_pixels` CTest renders generated RGBA8 inputs into offscreen textures and
reads back the results. Unlike `shader_compile`, which uses Dawn's Null
backend, this test uses D3D12 on Windows, Metal on macOS, or Vulkan elsewhere.
No available adapter is a reported skip, not a successful pixel check. Device
creation, validation, readback, and comparison failures fail the test.

The Windows run used Intel UHD Graphics 630 with D3D12 and passed all 15 cases:
three patterns at 1x1, 1x17, 19x1, 17x13, and 65x31. These cover colored flat
fields, low-contrast variation, diagonal/step edges, varying alpha, clamped
image boundaries, and padded readback rows. Assertions establish that:

- Neutral grading reproduces the source bytes exactly.
- AA preserves flat and low-contrast inputs and uniform neighborhoods exactly.
- High-contrast edges change, with channel changes bounded by the 40% blend cap.
- Alpha is preserved exactly in every case.
- The AA/detail entry point with zero detail strength equals the AA-only output.

All 16 CTest tests pass, including shader compilation and package validation.
The mod package and production source are unchanged. Build `aa_pixels` and run
`ctest --test-dir build/windows-release -C Release -R aa_pixels -V` to repeat the
pixel checks. The test target copies Dawn's required runtime/compiler DLLs using
the existing Aurora CMake helper.

This proves synthetic shader behavior, not game-scene art quality, temporal
stability, HUD ordering, enhanced-water composition, active detail behavior, or
performance. It does not close the remaining visual gate or qualify Metal;
only the Windows D3D12 execution was performed in this phase.
