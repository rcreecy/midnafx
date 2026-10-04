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

* Native replay correctness is not yet visually proven. The shape hook cannot
  invoke the enclosing material packet's complete draw call, so an authored state
  dependency could still differ despite replaying its observable setup.
* The diagnostic captures the first exact base candidate drawn each frame. It
  does not aggregate multiple simultaneously visible base shapes. Initial product
  work must use the validated exact allowlist and fail closed when aggregation is
  required.
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
* Gate 3 passes for the validated single-base-shape path. Multi-shape aggregation
  remains outside this gate and constrains the first product allowlist.

## Absorption prototype review

* The new setting is default off and persisted through the existing settings
  migration path. It does not mutate gameplay water, geometry, or native draw
  state.
* The shader rejects missing masks, background depth, invalid reconstruction,
  and foreground occlusion. Inputs are frame-scoped and resize-safe through the
  existing water attachment lifecycle.
* R_SP107 reached the absorption pipeline on D3D12 with no WebGPU validation
  errors or diagnostic fault colors. The captured pre-water scene color was
  near-black over the moving surface, so the visual result was near-black water.
  This is a source-boundary limitation; it is not a quality pass for absorption.
* Do not broaden the allowlist or enable the setting by default until a
  scene-color source containing useful submerged content is established.
