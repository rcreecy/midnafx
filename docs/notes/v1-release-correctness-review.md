# v1 release correctness review

Reviewed the final v1 diff, service boundaries, runtime evidence, deterministic
tests, package contract, and release documentation on 2026-10-03.

## Findings resolved

1. Depth-of-field radius configuration initially changed the uniform without
   scaling the fixed Gaussian taps. Tap offsets now map the kernel to the
   selected 2-12 pixel full-resolution radius.
2. Foreground dilation could place near blur over an in-focus silhouette. The
   composite now requires negative full-resolution circle of confusion before
   applying the dilated near layer.
3. Autofocus changed immediately on target movement. It now uses bounded,
   frame-rate-independent exponential smoothing, resets on invalid or disabled
   input, and rejects non-finite values.
4. Camera special-mode coverage lacked a positive exit proof. Native first
   person now has live entry, fail-closed output, and ordinary chase recovery
   evidence.
5. Camera combat coverage lacked a real boss sequence. Fyrus lock, attack,
   boss-special, and authored-camera transitions all preserved native output.
6. Temporary first-person, boss-demo, and controller-input harnesses were
   removed, and the clean source-matched host was rebuilt before final checks.

## Final validation

- Windows Release package and all 13 tests passed.
- Dawn Null validation compiled every grading, depth, and depth-of-field shader
  entry point.
- A clean combined D3D12 run executed grading/detail, camera FOV and lower-angle
  modification, 6-pixel depth of field, and allowlisted pumpkin/beehive normal
  smoothing together at 1216x896.
- Combined-run grading recorded 256/256 submitted/encoded draws with 11.80/26.10
  microsecond median/p95 callback time. Geometry processed pumpkin in 517
  microseconds and beehive in 120 microseconds with zero index conflicts.
- Graceful shutdown restored both mutated normal arrays and unloaded all mods.
- Logs contained no MidnaFX, WebGPU, validation, fatal, or non-finite error.

## Residual risks

Live Intel Mac/Metal execution and GPU timing remain unavailable. The final
depth-of-field silhouette correction lacks a new matched image, although its
source-matched compute/composite path executed successfully and earlier visual
passes cover focus orientation, HUD exclusion, grading order, movement, and
resize. Both depth of field and geometry smoothing remain default off. These
limits are disclosed in the v1 release notes and do not weaken default behavior.
