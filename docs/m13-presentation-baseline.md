# M13 presentation pipeline baseline

## Repository state

Recorded 2026-10-07 before M13 implementation work:

* branch: `codex/m12-water`
* HEAD: `ca4b3cd93e822e85e753a67caee7614f8df2d20a`
  (`chore(release): prepare v1.1.0`)
* working tree: clean
* latest release/tag: `v1.1.0`, pointing at HEAD
* divergence from `main`: 46 commits ahead, 0 behind
* pinned Dusklight SDK: `edf42c6a7202647b56dd2fcdef02d17671bc814b`

## Current presentation path

`GFX_STAGE_SCENE_AFTER_OPAQUE` occurs after Twilight Princess's opaque world
lists and before the normal translucent lists.  MidnaFX water uses that boundary
to acquire its pre-water snapshots.  Water then retains its own resolved scene,
opaque depth, surface depth, and surface mask inputs for the current frame.

At `GFX_STAGE_FRAME_BEFORE_HUD`, MidnaFX resolves the current scene pass into a
single-sample borrowed snapshot and queues its fullscreen draw.  The normal draw
combines optional water absorption, detail, and parametric grading.  The optional
DOF component runs at the same pre-HUD boundary after renderer registration and
uses its own single-sample color/depth snapshot plus private half-resolution
intermediates.  HUD is drawn after these hooks and is therefore outside all
MidnaFX scene processing.

Renderer pipeline pairs are keyed by `GfxRenderTargetLayout`; they are created on
the game thread and held until Dusklight has drained callbacks at shutdown.
Borrowed resolve views never cross a frame.  DOF reallocates its private targets
when its resolved dimensions change.  Water inputs are likewise frame-scoped and
cleared at scene begin.  Settings use ConfigService descriptors, so saved values
override descriptor defaults.

The shipped renderer accepts only a scene-color attachment in `RGBA8Unorm` or
`BGRA8Unorm` at sample count one.  The normal grading/detail path and the DOF
composite both have explicit `sample_count == 1` guards.  Those guards are a
correctness boundary, not an accidental omission.

## Native MSAA investigation and disposition

Aurora has an internal MSAA implementation: `AuroraConfig::msaa` feeds scene
color and depth allocation, GX pipeline sample count, and a single-sample color
resolve target.  Aurora's normal color pass snapshots copy from that resolved
color target, so a color-only post-process input would be plausible.

However, Dusklight's public GfxService exposes the *current* sample count only.
It has no supported setting, restart contract, or service operation that lets a
mod select 2x/4x MSAA.  `m_Do_main.cpp` leaves `AuroraConfig::msaa` zero, which
Aurora normalizes to one.

More importantly, `resolve_pass(... depth = true)` depends on Aurora's depth
snapshot shader.  Its multisample WGSL path reads sample zero
(`textureLoad(..., 0)`) rather than constructing a documented resolved depth
value.  That makes the depth supplied to water thickness/refraction and DOF
sample-position dependent at geometry edges.  The unrelated CPU depth-peek path
also explicitly rejects multisampled EFB targets.  A raw depth copy path rejects
multisampled depth outright.

### M13.1 result: native MSAA stopped

Native 4x MSAA is not a safe MidnaFX experiment with the current host contract.
Removing MidnaFX's one-sample guards would only make water and DOF consume an
ambiguous depth input.  No runtime MSAA claim is made.

The smallest viable host change is a restart-scoped Dusklight video setting with
adapter capability validation, plus a documented multisample depth-resolve
policy suitable for scene snapshots (and tests for color, depth, resize, and
pass continuation).  That is host work, not a safe mod-local change.  M13 will
therefore evaluate a pre-HUD single-sample post-process AA path first.

## Post-process AA direction

SMAA 1x would require area/search texture assets, their ownership and package
contract, and at least three scene passes.  A conservative FXAA prototype can
reuse the existing resolved scene snapshot and fullscreen pipeline without a
new host API or pixel-sized allocation.  It remains developer-only and
default-off until source-matched runtime captures establish visual quality,
water/DOF behavior, resize handling, and lifecycle safety.

The first ordering hypothesis is:

```text
resolved scene (including enhanced water)
  -> conservative FXAA
  -> existing detail gate and parametric grade
  -> optional DOF
  -> native HUD
```

This avoids sharpening before AA.  It is an implementation hypothesis, not a
visual qualification; runtime A/B captures remain required.
