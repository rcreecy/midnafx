# M9 camera foundation

Source and runtime target: Dusklight
`edf42c6a7202647b56dd2fcdef02d17671bc814b`.

## Decision

CameraService 1.1 cannot safely implement a FOV-only feature. Its full-camera
operator runs before `dCamera_c::Run()` and skips the native controller when a
mod accepts. MidnaFX therefore does not use that operator.

M9 adds a candidate CameraService 1.3 host patch at
`patches/dusklight-camera-fov-modifier.patch`. The appended API runs after the
current camera result is stored and PC wide-screen correction is applied, but
before frame-interpolation recording and `camera_draw`. It can change only the
vertical FOV. Eye, center, bank, aspect, near/far, collision, input, and the
game's camera controller remain owned by Dusklight and TP.

The API exposes active, normal-mode, authored-event, detached, and can-modify
context flags plus camera type/mode. `CAN_MODIFY` is emitted only for the native
chase algorithm in mode 0; numeric mode 0 alone is insufficient because TP
reuses it across other camera styles. Modifiers run by priority and registration order;
the first accepted finite result wins. The host rejects output in blocked
contexts and clamps accepted FOV to 10–120 degrees. Callback exceptions fail
the owning mod. Handles are owner-scoped and are erased on detach.

`tools/camera-host-patch.py` verifies the exact Dusklight revision and applies
the patch. CI applies it before building MidnaFX. MidnaFX still imports
CameraService 1.1, uses `SERVICE_HAS` for the appended fields, and remains
compatible with an unpatched host: the camera feature simply reports
unavailable.

## First prototype

The UI contains **Modern exploration FOV**, default OFF. It scales the native
vertical FOV in tangent space from 80–140% (default 110%) and transitions over
0–2 seconds (default 0.35 seconds). The override is requested only when all of
these are true:

- the host reports that modification is safe;
- the native camera mode is 0;
- the user enabled the feature.

Demo, detached/free-camera, targeting, aiming, and every nonzero camera mode
fall back to the current native FOV immediately. Disabling the setting also
restores native FOV immediately. No camera position or orientation is changed.
The interpolation helper assumes TP's 30 Hz simulation clock and caps one
update to four ticks so a long stall cannot jump the transition unexpectedly.

The UI also contains **Lower exploration camera**, default OFF, with a 0–15
degree reduction (default 6 degrees). CameraService 1.3 applies this as an
offset to the native chase controller's near/far latitude targets after TP has
selected contextual values. Native smoothing, eye construction, and collision
then run normally. The host accepts the offset only during active mode-0
gameplay outside demo and detached-camera ownership. The feature does not write
the final eye or center and does not alter targeting, dialogue, cutscenes, or
controllers that do not use the chase algorithm.

## Runtime evidence

A source-matched Windows D3D11 host loaded `F_SP103,0,27,0` twice from the
official game image at 1216×896. With the feature disabled, diagnostics recorded
type/mode `40/0`, native/effective FOV `61.25/61.25` degrees. With a 130% scale
and zero transition, the same checkpoint recorded flags `0x13` (active, normal
mode, can modify) and `61.25/75.16` degrees. The
capture visibly includes more scene at all four edges while preserving the
camera's eye and direction. A later native type transition (`41/0`) remained
active and produced `61.38/75.31` degrees.

The process shut down normally in both runs. Device-destroyed warnings occur
after window closure and are also present in baseline runs. A direct
`D_MN08,0,0,-1` launch stayed in mode 0, so it did not supply the required
nonzero/demo runtime case.

A temporary, uncommitted host validation harness then forced only the detached
context input at the real post-controller callback. The same `F_SP103` launch
reported flags `0x0b` (active, normal mode, detached), `active=no`, and exact
native/effective pairs `61.25/61.25` and `61.38/61.38`. The process exited
normally. The harness was removed and the normal patched host rebuilt. This
proves the complete host rejection and MidnaFX fallback path; it does not
replace controller-driven proof that the production detach command supplies
the flag.

Two further temporary, uncommitted host harnesses exercised the remaining
machine-reachable ownership guards at the real callback. Forcing demo ownership
produced flags `0x07` (active, normal mode, demo), `active=no`, and exact
native/effective pairs `61.25/61.25` and `61.38/61.38`. Forcing a non-normal
classification produced flags `0x11` (active, can modify), `active=no`, and the
same exact native/effective pairs. Both processes exited normally. Each harness
changed only its context classification, was removed after capture, and was
followed by a normal patched-host rebuild. These runs prove fail-closed handoff
behavior for demo and non-normal contexts, but controller-driven production
transitions still need interactive confirmation.

Separate 1024×768 and 1600×720 launches reported aspect `1.333` and `2.221`.
Both retained the same native/effective tangent-space scale
(`61.25/75.16` degrees), remained finite, and exited normally. This validates
startup aspect changes; live resizing still needs an interactive pass.

A temporary MidnaFX harness also forced an enabled, disabled, then re-enabled
sequence within one process while the native camera stayed in mode 0. Runtime
diagnostics recorded `61.38/75.31`, then immediate native fallback at
`61.59/61.59`, then resumed modification at `61.63/75.58`. The process exited
normally. The harness was removed, the release package rebuilt, and a final
launch again recorded normal active output. This proves callback state handoff
without stale output; the real UI/config control still needs an interactive
toggle pass.

The final ownership pass found that `dDemo_c::getCamera()` alone did not cover
ordinary authored events. In the real `F_SP103,0,27,0` startup event, TP entered
its event camera type while the original host still reported modification safe.
The host now also checks `dComIfGp_event_runCheck()` and restricts
`CAN_MODIFY` to native chase algorithm 1. A source-matched D3D11 rerun recorded
event type 40 as flags `0x07`, inactive, native/effective FOV `61.25/61.25`, and
zero latitude offset. When the event ended, type 41 recorded flags `0x13`,
active, `61.38/66.28` at 110%, and `-6.00` degrees. This is production event
state, not a forced classification harness.

The algorithm gate rejects lock-on, talk/conversation, subject/aim, fixed
position, fixed frame, ride/horseback, manual, event, hookshot, colosseum,
observe, magnetic boots, rail, para-rail, one-sided, and test camera engines.
Detached ownership and every nonzero mode remain independent fail-closed guards.
The patch contract test preserves the authored-event, chase-algorithm, and
detached predicates. Controller-driven spot checks and live resize remain useful
compatibility coverage, but are no longer required to establish that unrelated
native camera algorithms retain ownership.

The first matched lower-angle experiment used the same `F_SP103,0,27,0` scene,
1216×896 window, native FOV, and startup timing. Baseline diagnostics reported
native chase latitudes `10.00/25.00` degrees with offset `0.00`; the enabled run
reported the same native inputs with offset `-6.00`. Both processes exited
normally. The enabled capture shows less foreground ground and more forward
scene while retaining TP's native player-relative center and collision path.
Evidence is stored locally as `build/m9-evidence/angle-baseline.png`,
`angle-low.png`, and corresponding logs. Traversal near walls, ceilings, slopes,
doors, and confined rooms remains required before broader use.

A combined candidate using 110% tangent-space FOV and the 6-degree reduction
also ran cleanly. Diagnostics recorded `61.25/66.14` degrees for FOV and the
same `-6.00` chase offset. `build/m9-evidence/modern-camera.png` shows the
intended wider, more forward-facing exploration composition without the
aggressive 130% FOV used for the earlier isolation proof.

## Completion decision

M9 camera foundation is complete as a conservative opt-in feature. The shipped
candidate is 110% tangent-space FOV plus a 6-degree lower chase latitude. Both
remain default OFF. Native smoothing and collision receive the changed latitude
before constructing the eye, while every other camera algorithm and authored
event retains native framing. The feature does not change distance, lateral
framing, collision policy, targeting, camera shake, or authored cameras.

A default-on profile remains a product-validation decision: it needs extended
controller-driven traversal near walls, ceilings, slopes, doors, and confined
rooms plus live-resize and user-setting interaction checks. Those checks may
change the recommended values, but no further camera architecture milestone is
required. Any future distance/height work needs a separate native-controller
parameter API; final-eye offsets remain unacceptable because they bypass
collision policy.
