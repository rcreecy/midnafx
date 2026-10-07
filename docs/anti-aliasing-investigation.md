# M13 anti-aliasing investigation

## Current prototype

M13 uses a conservative FXAA-style fullscreen path as the first post-process
candidate.  It reuses the existing `resolve_pass` color snapshot and MidnaFX
fullscreen draw infrastructure; it creates no texture, sampler, bind group
layout, or pixel-sized intermediate owned by the mod.  Pipeline pairs remain
keyed by the host render-target layout and are retired with the existing
renderer lifecycle.

The developer-only `anti_aliasing` setting is default off.  It is disabled for
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
  -> optional detail gate
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
