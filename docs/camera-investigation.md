# M9 camera foundation

Source and runtime target: Dusklight
`edf42c6a7202647b56dd2fcdef02d17671bc814b`.

## Decision

CameraService 1.1 cannot safely implement a FOV-only feature. Its full-camera
operator runs before `dCamera_c::Run()` and skips the native controller when a
mod accepts. MidnaFX therefore does not use that operator.

M9 adds a candidate CameraService 1.2 host patch at
`patches/dusklight-camera-fov-modifier.patch`. The appended API runs after the
current camera result is stored and PC wide-screen correction is applied, but
before frame-interpolation recording and `camera_draw`. It can change only the
vertical FOV. Eye, center, bank, aspect, near/far, collision, input, and the
game's camera controller remain owned by Dusklight and TP.

The API exposes active, normal-mode, demo, detached, and can-modify context
flags plus camera type/mode. Modifiers run by priority and registration order;
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

Live targeting, aiming, dialogue/cutscene activation, production detached-camera
activation, live resize, and the real UI/config setting toggle remain validation
work before this can become a default-on camera profile. Automated runtime
evidence now covers normal exploration, forced detached, forced demo, forced
non-normal, forced in-process disable/re-enable, and two startup aspect ratios.

## Scope and next step

This checkpoint intentionally contains only a vertical-FOV foundation. It does
not change distance, height, pitch, lateral framing, collision, targeting,
camera shake, or authored cameras. Next, validate every blocked camera context
with controller input and matched captures. Only then design a separate native
controller-parameter API for distance/height; final-eye offsets are not an
acceptable substitute because they would bypass collision policy.
