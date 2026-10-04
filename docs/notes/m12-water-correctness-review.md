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

## Remaining risks

* Native replay correctness is not yet visually proven. The shape hook cannot
  invoke the enclosing material packet's complete draw call, so an authored state
  dependency could still differ despite replaying its observable setup.
* Only the first exact classified shape is captured per frame. The current proof
  target has one relevant shape; multi-shape surfaces need aggregation.
* Shoreline/depth meaning is not visually proven. Runtime logs establish execution
  and valid resources, not correct optical output.
* Other water classes, underwater transitions, resize, reload, and diagnostic
  disable need direct visual/lifecycle exercises with this new path.

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
* Gate 3 remains provisional until the remaining multi-class visual checks pass.
