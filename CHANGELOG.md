# Changelog

## 1.14.0 - 2026-10-29

- Add experimental sunlight shafts across supported outdoor scenes, without
  asset or stage lists. Remaster enables the independent Basic toggle; Vanilla
  disables it. Advanced lighting strength defaults to 10% with a 25% maximum.
  Earlier scene masks keep the feature off until profile selection.
- Read native sun position, color, alpha and visibility. Preserve native weather
  and daylight fades; bypass missing, off-screen, behind-camera, paused,
  underwater, Twilight and hidden-sky contexts. Use bounded screen-space depth
  sampling to interrupt rays around visible opaque silhouettes.
- Compose in linear light with bounded color headroom and foreground suppression,
  preserving alpha and downstream water, grading, exposure, bloom, LUT and HUD.
  Reuse the existing opaque-stage draw and snapshots. Shafts-only operation
  requires no AO/bounce compute dispatch or extra targets.
- Retain first-layer visibility limits: transparent/cloud occlusion, off-screen
  geometry, physical volumetric transport, whole-game motion quality and universal
  GPU cost remain unqualified. Mac gameplay remains unverified and best effort.

## 1.13.0 - 2026-10-28

- Add experimental reflections of visible opaque scenery on supported enhanced
  water. Remaster enables the independent Basic toggle; Vanilla disables it.
  Existing settings and earlier scene masks preserve the new feature off until
  profile selection. The existing reflection strength controls both scene color
  and the tint fallback.
- Trace bounded reflected rays through the pre-water depth/color snapshot using
  animated water normals. Reject invalid projection, background, near-plane,
  below-water, back-facing and recursive water sources. Hidden water behind an
  opaque bank does not exclude that bank. Preserve source alpha and existing
  shoreline, Fresnel, grading, exposure, bloom, LUT and HUD composition.
- Reuse existing water resources and passes with no new asset identities.
  Add analytic GPU fixtures for color transfer, depth ordering, fallback,
  reversed Z, clipping and small/odd targets. Screen-space visibility and shader
  cost remain limitations; Mac gameplay remains unverified and best effort.

## 1.12.0 - 2026-10-28

- Add gameplay motion clarity without asset or stage lists. Remaster enables the
  independent Basic toggle; Vanilla disables it. Advanced retained blur scales
  native blend alpha from 0–100%, defaulting to 0%.
- Preserve native blur in missing-player, pause, event, demo-camera and fade
  contexts. Borrow only the rate byte after opaque rendering, restore it before
  HUD rendering, and recover pending ownership at scene begin or unload.
- Preserve detectable intervening writes and native blur enable/history/matrix
  state. Add exhaustive rate/rounding and safety-gate checks plus ownership tests.
  Earlier product masks retain the new toggle off until profile selection.
- Add no rendering pass, target or host patch. Native blend draws can still
  execute with zero alpha; no GPU speedup or reconstructed motion blur is claimed.
  Mac gameplay remains unverified and support remains best effort.

## 1.11.0 - 2026-10-28

- Add bounded screen-space color bounce across visible opaque scenery and
  characters, without asset or stage lists. Remaster enables it; Vanilla disables
  it. An independent experimental Basic toggle and Advanced 0–30% strength
  default to 12%. Existing saved settings remain intact until profile selection.
- Gather nearby facing surfaces from the native opaque color/depth snapshot,
  reject invalid and interrupted depth segments, and filter separate color
  fields using the existing contact-shading geometry. Compose bounded linear
  light before atmosphere while preserving scene alpha and downstream water/HUD.
- Keep AO, contact shadows and bounce independent. Disabled shared operation uses
  1×1 binding placeholders and skips the gather dispatch. Resize/unload lifetime
  follows the existing pending-callback target ownership.
- Retain explicit LDR, visible-depth and lit-color limits: this does not provide
  material albedo, hidden surfaces or physically based global illumination.
  macOS gameplay remains unverified and support remains best effort.

## 1.10.0 - 2026-10-27

- Add movement-aware exploration framing across native chase-camera contexts,
  without asset or stage lists. Remaster enables it; Vanilla disables it.
  An independent Basic toggle and Advanced 0–12% boost default to 8%.
- Use native movement speed relative to the player's current maximum, smoothing
  expansion and a slower return at rest. Preserve native aiming and authored
  cameras, reset across context changes and ignore duplicate clock samples.
- Retain independent static camera tuning, existing saved profiles and the
  CameraService 1.3 compatibility boundary. No new render pass or host patch is
  required. macOS gameplay and whole-game camera comfort remain unverified.

## 1.9.0 - 2026-10-27

- Add scene-wide directional contact shadows using native environment light and
  visible opaque depth. Twelve projected samples trace up to 120 world units,
  without asset identities or stage lists. Remaster enables the effect; independent
  Basic controls default to 30% strength, while Vanilla disables it.
- Share AO's half-resolution targets, surface-aware filters and reconstruction.
  Keep AO/shadow strengths independent, cap combined darkening at 50%, retain
  alpha and independent fog, and preserve downstream water/translucent/HUD order.
- Bound nearby-light traces after normal bias and reject samples beyond the ray.
  Add focused directional geometry, zero-strength parity, filter, alpha/fog and
  range/depth checks. Existing profiles remain readable and keep the new toggle off
  until a new profile is selected.
- Keep macOS support best effort through native builds and portable checks. Hidden
  and off-screen casters, whole-game motion quality and GPU duration remain open.

## 1.8.0 - 2026-10-26

- Replace the five-tap contrast-edge blur with edge-directed spatial AA, using
  bounded endpoint searches and symmetric coverage for tied orientations.
  Preserve aligned edges and tested one-pixel ridges instead of softening them.
- Share the filter across ordinary and enhanced-water shaders. Keep alpha,
  detail gating, LUT/exposure/bloom order, existing AA controls/defaults and HUD
  ordering. No new target, pass or host patch is required.
- Add supersampled edge-coverage comparisons against the retired filter, exact
  transpose/brightness-complement checks, fine-detail preservation and inactive
  water parity. Record the higher bounded sampling cost; GPU timing, whole-game
  motion quality and native macOS gameplay remain unverified.

## 1.7.0 - 2026-10-26

- Add scene-wide LDR bloom without asset allowlists, using quarter-resolution
  highlight extraction and two Gaussian filters. Enable it in Remaster and expose
  independent Basic controls with 20% default strength; Vanilla disables it.
- Fuse glow into existing grading/water draws before exposure and LUTs, preserving
  alpha, the white endpoint and native HUD ordering. Retire resized targets after
  pending callbacks and retain other effects when bloom is unavailable.
- Add focused GPU extraction/filter and ordinary/water/AA/detail/LUT composition
  checks. Native Ordon toggle, resize, reload and close pass. Retain an unresolved
  black Goron Mines startup; full-game quality and GPU duration remain open.
- Continue best-effort macOS builds and portable checks without hosted Metal
  execution. Bloom is highlight treatment, not emissive classification or HDR.

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
