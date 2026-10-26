# Changelog

## 1.6.0 - 2026-10-26

- Replace the 16-tap full-resolution AO kernel with a 32-tap half-resolution
  contact field, separable surface-aware filtering and edge-aware reconstruction.
  The shared path covers opaque scenery and characters without asset allowlists.
- Preserve native full-precision depth through filtering to avoid distant contact
  errors. Scale the sample-radius cap with resolution and retain scene alpha,
  translucent/water ordering and the native HUD.
- Retire resized AO resources after their last queued callback; validate resource
  creation and preserve independent atmosphere when AO is unavailable.
- Keep existing AO controls and Remaster defaults. Add focused GPU contact,
  surface isolation, fog fallback and degenerate-dimension checks. Native Mac
  gameplay and whole-game motion/performance qualification remain unavailable/open.

## 1.5.0 - 2026-10-26

- Add scene-wide adaptive exposure without asset identities or material allowlists.
  A fixed 256-sample GPU meter gradually adjusts existing grading and water draws,
  improving dark-view readability while retaining authored lighting differences.
- Bound exposure to -0.5 through +0.75 stops; preserve black fades, avoid highlight clipping,
  RGB ratios and alpha. Reset history on room/stage/layout changes, pause and disable.
- Enable adaptive exposure in Remaster and disable it in Vanilla; preserve existing
  settings and support the independent Basic control with grading off.
- Validate GPU adaptation, fades, colored light and ordinary/water/LUT/AA/detail
  composition, plus native Windows day/night toggles. macOS gameplay remains unverified.

## 1.4.1 - 2026-10-26

- Extend Enhanced Link shading to Hero's Clothes and repair re-enable using original
  source draws with their matching descriptors. Preserve exact normal restoration.

## 1.4.0 - 2026-10-25

- Add reversible Enhanced Link shading for the original Ordon outfit to Basic and
  Remaster, preserving uncertain regions, animation, geometry and authored boundaries.

## 1.3.0 - 2026-10-25

- Add the combined Remaster profile, scene-wide ambient contact shading and depth
  atmosphere based on native fog parameters. Vanilla restores profile effects.

- Add opt-in Neutral, MidnaFX Natural, and contextual Twilight color looks with
  persistent intensity controls. Neutral remains the default and bypasses LUT sampling.
- Fuse cached 33-cubed RGBA16Float LUT sampling into existing grading and water
  draws, preserving alpha, AA/detail behavior, downstream DOF, and native HUD order.
- Blend Natural and Twilight results using the existing semantic transition timing.
  Grading-off and developer diagnostic modes bypass the creative transform.
- Log creative Twilight state changes and blend endpoints independently of legacy
  grading targets when diagnostics are enabled.
- Add identity readback coverage across ordinary/water/AA/detail paths, exposure
  extremes, odd dimensions, RGB/BGRA boundaries, and creative blend invariants.
- Creative art qualification, physical Mac gameplay, and GPU duration measurements
  remain incomplete. Creative looks remain opt-in.

## 1.2.0 - 2026-10-08

- Enable experimental pre-HUD AA by default for new configurations; preserve saved off choices and expose the toggle in Basic.
- Prevent disabled grading/detail/Twilight adjustments from leaking into AA or enhanced-water draws.
- Reuse AA neighborhood samples for detail and prevent detail from counteracting filtered edges.
- Add 25 headless GPU pixel cases; all 16 local Windows tests pass. Live visual quality and Metal game validation remain incomplete.

## 1.1.0 - 2026-10-07

- Add a default-on, exact-allowlist enhanced-water pipeline with depth-based
  absorption, animated surface detail, bounded refraction, Fresnel/environment
  response, shoreline treatment, and authored-light specular.
- Preserve TP water gameplay, geometry, animation, interaction effects, and
  unrelated translucency through frame-scoped scene/depth captures and
  fail-closed exact classification.
- Enable enhanced water by default for new installations while preserving saved
  user choices. Unknown, ambiguous, and unsupported water remains native.
- Validate exact classification across outdoor, moving, dungeon, boss, and
  submerged scenes, with optical-path proof at waterfall-adjacent and irregular
  shores and clean lifecycle behavior.
- Keep screen-space reflections deferred after the first prototype could not
  produce trustworthy on-screen intersections.

## 1.0.0 - 2026-10-03

- Complete the opt-in modern camera qualification with native first-person,
  lock-on, boss-special, authored-camera, recovery, toggle, and lifecycle proof.
- Improve experimental depth of field with adjustable blur radius, temporal
  camera-target autofocus, and silhouette-safe near-layer compositing.
- Close the v1 geometry scope around exact fail-closed static and skinned model
  allowlists with topology, lifecycle, restoration, and performance evidence.
- Record M6 native environment controls as safely deferred because the pinned
  host lacks mod-scoped ownership and restoration APIs.
- Pass the final combined Windows runtime, 13 portable tests, package contract,
  Dawn shader compilation, and Windows/Intel macOS CI builds.

## 0.9.1 - 2026-09-27

- Make camera-target autofocus the default focus source when the still
  default-off depth-of-field blur is enabled.
- Record live Windows D3D11 visual validation for the focus mask, autofocus
  blur, atmosphere depth, Natural / Vivid Realism, modern camera framing, HUD
  exclusion, grading order, and resize lifecycle.
- Keep manual focus available while documenting its scene-dependent tuning
  requirement and the prototype's remaining silhouette limitations.

## 0.9.0 - 2026-09-26

- Add a default-off half-resolution depth-of-field blur prototype with separate
  near and far layers and full-resolution depth-aware compositing.
- Add manual and camera-target focus sources. Camera-target focus requires the
  source-matched CameraService 1.4 host extension and fails closed when absent.
- Add resize-safe intermediate target retirement and shader validation for the
  compute and composite pipelines.

## 0.8.0 - 2026-09-26

- Add default-off atmosphere depth-reconstruction diagnostics with one-shot
  camera/depth probes and load-time timing evidence.
- Add a default-off depth-of-field focus-mask diagnostic with manual focus
  distance and range controls.
- Harden frustum-corner validation and GPU uniform layout correctness.

## 0.7.1 - 2026-09-26

- Add the built-in Natural / Vivid Realism preset with restrained saturation,
  neutral white balance, highlight rolloff, and subtle detail enhancement.

## 0.7.0 - 2026-09-25

- Complete the opt-in modern exploration camera with 110% tangent-space FOV,
  a 6-degree lower native chase angle, and fail-closed event/algorithm ownership.

## 0.6.0 - 2026-09-25

- Add fused pre-HUD grading, optional detail enhancement, presets, comparison views,
  and runtime diagnostics.
- Add a default-off automatic Twilight profile with live Windows validation for
  normal and active Twilight states.
- Add default-off adaptive normal smoothing for an exact allowlist of validated
  rigid, environmental, and skinned models.
- Add topology, smoothing, lifecycle, package, shader, and grading tests.
- Publish separate Windows AMD64 and macOS x86_64 packages. The macOS package is
  build-tested but still awaits an Intel Mac runtime pass.
