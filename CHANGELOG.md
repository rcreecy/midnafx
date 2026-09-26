# Changelog

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
