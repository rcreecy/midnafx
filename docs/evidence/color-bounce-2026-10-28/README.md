# Screen-space color bounce: 2026-10-28

## Artifacts and scope

The final local Windows Release package is MidnaFX 1.11.0, SHA-256
`E4DFA3590BFFB95E4CA03BAD720F372B4092829DD64C12BC9D2D51EF79B10DF6`.
The existing CameraService-patched host has SHA-256
`444D8EE33ED78BA4A427810E35A62731F4D845CBAC7343B37B16A5C20F3C34A4`.
Native checks use Intel UHD 630, D3D12, Ordon `F_SP103,0,27,0`, initially
960×720. External texture replacements and auto-save are disabled. No test driver
or game/texture content is included in the product package.

An earlier local package, SHA-256
`1470CED63A012832D7C9D726415B3F837B3347385E7446BBDDA549CDEE73E688`,
has the same rendering implementation but a longer Basic label. Native inspection
found that the label clipped the toggle value. The final package shortens it to
**Color bounce (experimental)**. This is a concrete UI repair, not a native
visual-quality pass. Both isolated native processes close with exit code zero.

## Deterministic GPU and compatibility proof

[32 GPU cases](ambient-pixels.log) pass on D3D12 using the production shaders.
They retain all 20 existing AO/shadow cases and add twelve reports covering:

- Source-color transfer between independently constructed facing walls, including
  red and blue donors at the actual default 12% strength on even and odd targets.
- Exact zero-strength parity with AO and shadows active, including poisoned
  actual 1×1 disabled-field placeholders.
- Radius rejection, reversed-Z projection, receiver energy/headroom bounds,
  black/white endpoints, scene alpha and analytical fog composition.
- Coplanar, parallel-facing and background rejection; surface-aware filtering,
  mean-energy retention, silhouette separation and small/odd targets.

The complete Release build passes. [Three focused tests](focused-tests.log)
also pass for shader compilation, profile round trips and package layout.
Profile tests exercise scene masks through 255 and preserve earlier mask 127
with bounce off. Product flags retain their existing range. Read-only review
found no concrete binding, target lifetime, feedback or composition defects.

These fixtures establish bounded color transfer and composition at the opaque
effect output. They do not qualify native art direction or whole-game motion.

## Native integration and lifecycle

[Final native receipts](native-r2/) retain package/host identities, configuration,
diagnostic excerpts, empty-card error and normal process exit. Basic toggles work
with other effects off and with AO held active. The diagnostic reports the
16-source bounce path active; no shader validation error is observed.

Selecting **Profile / Remaster** enables bounce at 12% alongside the existing
effects. Module reload reactivates the complete stack. At that reload, contact
compute/draw counts balance at 26,812 with one retained target set. Fullscreen
resize changes the camera aspect from 1.333 to 1.565 and produces the retained
1748×1116 window capture. Final shutdown balances 858 contact computes/draws,
retains one current target set, restores owned geometry exactly and logs
`all mods unloaded`, followed by process exit code zero. The screenshot is a
rendering/lifecycle observation, not a matched art comparison or frame-rate result.

The empty card reports `Failed to open file: gczelda2`. After mod unload the host
reports aborted buffer mappings and `Device lost: Device was destroyed.` These
known teardown warnings remain recorded. No clean native frame-time or GPU
duration claim is made.

## Native comparison did not isolate the new effect

The original captures are retained rather than presented as a successful quality
comparison. [Measured comparisons](native-comparisons.json) can be reproduced by
[the analysis script](retain-evidence.py). Pixel coordinates refer to decoded
JPEG window captures, not direct GPU buffer readbacks.

The first attempt used simulation rate zero. This replays an already recorded
frame, so UI setting changes cannot establish newly encoded shader behavior.
`final-off.jpg` and `final-on-candidate.jpg` therefore are not an on/off proof.
The initial package's `off.jpg` and `on-candidate.jpg` also precede the label
repair; its attempted toggle did not establish an enabled state.

At simulation rate 30, actor freeze preserves poses while rendering continues.
Native camera detachment then holds the camera. The `live-*` and `locked-*`
captures still show large spatial color/contrast differences beyond the direct
opaque shader's energy bound. Bounded alignment analysis did not explain those
changes as camera translation. Keeping AO positively enabled on both sides
holds snapshot/composite topology active, but the `shared-*` pair still varies.
Crucially, a repeated **unchanged AO-on/bounce-off** control also changes: the
world rectangle `[260,40,700,600)` has 150,386 pixels differing by more than three
codes and an 86-code maximum channel difference. These captures cannot isolate
the new effect's contribution or prove exact native pixel restoration.

Source inspection identifies remaining native mechanisms rather than a confirmed
cause: environment time/lighting continue outside actor pose freeze, and native
motion blur and bloom execute after this opaque composite. Resolve continuations
preserve existing attachments, and custom draws restore native bindings and
viewport/scissor; no concrete light-global mutation was found in that path.
The direct blend bound is not a bound on final pixels after native postprocessing.

Native visual quality, motion comfort/stability, mounted/combat/dungeon coverage,
replacement-pack quality, hidden-surface contribution and performance remain
open. Mac gameplay and Metal execution are unavailable; support remains best
effort through native builds and portable checks. No hosted Metal checks are
required for this release.
