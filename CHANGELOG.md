# Changelog

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
