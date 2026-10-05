# M12 Gate 3 correctness review

Date: 2026-10-04

Scope: the default-off water optical-thickness diagnostic only. Product water
effects remain out of scope.

## Findings resolved

* Removed an early actor-level duplicate-render design. It could redraw unrelated
  shapes, effects, or actor state. The retained hook targets an exact classified
  shape only.
* Corrected attachment sizing from logical GX dimensions (608x448) to the actual
  resolved scene target (1216x896), preventing depth/color extent mismatch.
* Restored the native replay setup after the diagnostic draw: material load,
  packet preparation, shape pre-draw state, and packet display list are applied
  before the original shape draw.
* Borrowed scene-depth views are valid for one frame only and are cleared at the
  next scene boundary. Private attachments are recreated on resize and released
  on disable/shutdown.
* Missing services, targets, depth, matrices, unsupported formats, and failed
  allocations bypass the diagnostic. The shader rejects background depth,
  non-finite reconstruction, and opaque samples in front of the water surface.
* Fixed visible grayscale/native-color flicker on stage water. Classification was
  refreshed only on simulation ticks but cleared on every interpolated
  presentation. Capture modes now rescan active stage actors at the pre-water
  render stage on every presentation frame; classification-only mode retains its
  simulation-tick scan.
* Prevented current/projected overlay shapes from owning optical thickness.
  Explicit actor paths select material zero only on primary/surface models;
  validated stage paths select their exact base-water material. Target identity
  is logged once for audit.
* Rejected opaque depth at or in front of the captured water plane. This removes
  banks, actors, and other non-water foreground from the diagnostic instead of
  displaying them as thickness or an error color.
* Split malformed depth reconstruction from normal occlusion in the diagnostic:
  invalid reconstruction remains red, while expected foreground occlusion is
  black and fails closed.

## Remaining risks

* Post-native auxiliary replay is visually proven on generic moving water,
  Fishing Pond, and the visible Lake Hylia patch. The shape hook still cannot
  invoke the enclosing material packet's complete draw call, so untested water
  classes could depend on additional authored state.
* Exact packets are retained only until the same frame's pre-HUD stage, then
  replayed into one combined mask/depth pass and cleared. Collection is capped at
  64 unique packets; overflow fails the frame closed. Multi-packet pixels remain
  visually unproven even though Lakebed exercises the path successfully.
* Lakebed room 3 has execution proof but no useful surface-facing spawn for an
  unattended visual proof. Underwater transitions and additional story-layer
  states remain product-validation work.

## Verification

* Windows Release package builds successfully.
* All 14 tests pass, including shader compilation and package contract checks.
* Source-matched D3D12 runtime reaches mask, surface-depth, and thickness passes at
  Fishing Pond and shuts down cleanly without WebGPU/D3D12 validation errors.
* A 347-capture run reported zero capture failures and about 217 microseconds of
  CPU hook wall time per captured frame; GPU duration is not yet measured.
* Lake Hylia, generic moving/swimmable water, and Lakebed Temple each completed
  259-260 captures with zero failures, clean unload, and 94-100 microseconds of
  CPU hook wall time per capture. These runs prove execution, not pixels.
* Fishing Pond visual proof shows a stable grayscale thickness field with live
  HUD after the cadence fix. A 10,006-capture run had zero failures, no visible
  flicker, clean unload, about 24 microseconds of classification scan time, and
  about 139 microseconds of capture/replay CPU wall time per frame.
* Generic moving water selected the exact `cc_MA06_nigori_v_x` base surface after
  an overlay-selection failure was diagnosed. The corrected view contains a
  continuous grayscale field with Link and stone banks rejected by foreground
  depth, no red/magenta reconstruction faults, 6,471 successful captures, zero
  failures, and clean unload.
* Lake Hylia produced a stable water-local grayscale field without reconstruction
  fault colors, then unloaded cleanly after 688 successful captures and zero
  failures.
* Gate 3 passes for the validated exact path. Multi-packet aggregation is bounded
  and implemented, but broader material coverage remains outside this gate and
  constrains the first product allowlist.

## Absorption prototype review

* The new setting is default off and persisted through the existing settings
  migration path. It does not mutate gameplay water, geometry, or native draw
  state.
* The shader rejects missing masks, background depth, invalid reconstruction,
  and foreground occlusion. Inputs are frame-scoped and resize-safe through the
  existing water attachment lifecycle.
* Visual review found that normal grade selection overwrote the absorption
  pipeline kind. The active log was therefore misleading. The selection guard
  now preserves `WaterAbsorption` and `WaterAbsorptionDetail`.
* A zero-strength control then proved that the original capture order replaced
  native water with an incomplete replay. Capture now leaves the original draw
  untouched and performs the white mask/depth replay in a private pass afterward.
* R_SP107 zero-strength A/B preserves native transparency and animated
  distortion. Default strength produces a restrained depth tint without losing
  those details. The final D3D12 run completed 1,449 captures with zero failures,
  clean unload, about 88 microseconds of CPU hook time per frame, and no WebGPU
  validation errors.
* Fishing Pond zero/default A/B preserves waves, transparency, shoreline, and
  background geometry while the default pass adds a restrained depth tint. The
  runs completed 1,171/1,143 captures with zero failures at about 138/147
  microseconds per frame and unloaded cleanly without validation errors.
* Lake Hylia zero/default A/B remained stable on the small visible water patch
  and retained native detail. The runs completed 1,334/1,038 captures with zero
  failures at about 86/85 microseconds per frame and unloaded cleanly.
* Lakebed Temple exercised four exact surface packets per frame. It completed
  1,464 combined captures with zero failures at about 52 microseconds of CPU
  replay work per frame, clean unload, and no validation errors. The spawn does
  not expose water, so this proves execution/lifecycle rather than pixels.
* Do not broaden the allowlist or enable the setting by default until equivalent
  dungeon and underwater visual validation passes.
