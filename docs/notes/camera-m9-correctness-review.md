# M9 camera correctness review

Reviewed the CameraService 1.3 patch, MidnaFX callbacks, configuration, tests,
runtime logs, and the pinned Dusklight camera controller at
`edf42c6a7202647b56dd2fcdef02d17671bc814b`.

## Findings resolved

1. **Authored events were not fully excluded.** The first implementation used
   only `dDemo_c::getCamera()` as its demo predicate. A live `F_SP103` startup
   event entered TP's event camera type with no JStudio demo camera, so both
   modifiers remained active. The host now also checks
   `dComIfGp_event_runCheck()`. The same live transition reports flags `0x07`,
   native FOV, and zero latitude offset until the event ends.
2. **Mode 0 did not uniquely identify free exploration.** TP's fixed, ride,
   authored, and other camera styles can reuse numeric mode 0. The host now
   emits `CAN_MODIFY` only when `dCamParam_c` selects engine algorithm 1, the
   native chase controller, while mode is 0. Lock-on, talk, subject, fixed,
   ride, manual, event, rail, and other algorithms therefore fail closed.

## Review result

No unresolved memory, ownership, arithmetic, or restoration defect was found.
Callbacks validate structure sizes and finite values. Host outputs are finite-
checked and clamped. Handles are owner-scoped and removed on detach. MidnaFX
uses versioned optional service fields, so an unpatched host leaves both camera
features unavailable. Disabling FOV immediately returns native output; chase
changes stop at the next native chase sample. The source retains native camera
smoothing, eye construction, collision, shake, interpolation, and rendering.

The remaining risk is visual tuning across the full game. The 110% / 6-degree
candidate is therefore default OFF. Extended controller traversal and live
resize are required before a later release may enable it by default, but they do
not block the opt-in M9 architecture or implementation.
