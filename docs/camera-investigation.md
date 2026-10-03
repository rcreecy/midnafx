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

## Desktop visual recheck (2026-09-26)

A source-matched Windows D3D11 launch returned directly to
`F_SP103,0,27,0` with an exactly quoted disc path. The native baseline, 110%
FOV-only candidate, and combined 110% FOV plus 6-degree lower candidate were
observed from the same spawn. The wider view clearly increased peripheral
scene coverage. The lower-angle change was subtler and moved the composition
forward without changing the player-relative center or producing a visible
jump. Runtime diagnostics reported the startup event inactive at native FOV,
then native chase active with `-6.00` degrees and the effective FOV converging
from native toward the configured 110% tangent-space scale.

This confirms the intended static framing in the live game. Collision-heavy
controller traversal, targeting transitions, confined rooms, and interactive
toggle coverage remain required before considering either control default on.

## Confined traversal recheck (2026-09-27)

A temporary host-only input harness supplied sustained forward and right-stick
input in `D_MN04,7,0,-1`. It moved Link from the room-7 spawn into the nearby
wall and rotated the native camera inside the narrow space. The 110% FOV and
6-degree lower-angle controls remained enabled throughout. Link stayed visible,
native collision stopped forward movement, and the view produced no wall
penetration, camera inversion, discontinuity, or unstable framing.

Diagnostics first reported startup camera type 40 as inactive with flags `0x07`.
Native mode-0 chase then became active for camera types 96 and 99 with flags
`0x13`. The lower-angle offset remained `-6.00` degrees. Effective FOV values
tracked changing native FOV values without becoming non-finite: `60.00/64.84`,
`55.17/59.77`, and `67.67/72.81` degrees. The source-matched Windows D3D12 run
closed gracefully and unloaded all mods. Its ignored runtime log is
`build/visual-validation-v090/logs/dusklight-20260927-103736.log`.

The harness modified only Aurora input for this test. It was removed afterward,
and the clean source-matched host was rebuilt. This closes one confined-room
collision traversal spot check. Targeting, combat, aiming, first person,
horseback, swimming, climbing, scene transitions, and interactive toggle tests
remain open before any default-on decision.

## Target-hold recheck (2026-09-27)

A temporary host-only input harness held TP's actual lock input: digital L plus
the analog left trigger. An earlier attempt used the displayed Z prompt and did
not enter the game's `dAttention_c` lock path; that run is not targeting
evidence. With L held in `F_SP103,0,27,0`, the production camera changed from
type/mode `41/0` to `41/1`. Diagnostics changed from flags `0x13`, active FOV
and `-6.00` degrees to flags `0x01`, inactive modifiers, native/effective FOV
`61.63/61.63`, and zero latitude offset. Spawned `E_BA` and `E_OC` actors loaded,
but their default command-console placement did not produce a stable target
selection from this spawn.

A follow-up in `F_SP116,3,13,2` spawned `E_BA` actor 490 at explicit coordinates
`3150,-650,5900`, then generated distinct L release/press edges after the actor
was active. The target arrow was visible over the bat. Diagnostics repeatedly
transitioned from type/mode `196/0` to `196/2`; every mode-2 sample reported
flags `0x01`, inactive modifiers, and identical native/effective FOV
`45.00/45.00`. This is positive live proof that stable enemy lock-on preserves
native framing. The ignored evidence is
`build/visual-validation-v090/lock-pulse.jpg` and
`build/visual-validation-v090/logs/dusklight-20260927-204720.log`.

The host predicate now also rejects explicit attention lock state, camera lock
targets, forced lock actors, and active L-lock state. These checks protect
transition frames where mode and chase algorithm values may lag the lock state.
The patch contract covers every predicate. Each input harness was removed and
the clean source-matched host rebuilt. This closes sustained target-button and
stable enemy-lock fallback. Aiming, broader combat, special camera modes, and
target-release recovery into active exploration remain open before any
default-on decision.

## Target-release recovery recheck (2026-09-28)

A source-matched Windows D3D12 run returned to `F_SP103,0,27,0`, where native
mode-0 chase uses engine algorithm 1. A temporary Aurora harness held digital L
and the analog left trigger after an explicit `E_BA` spawn at
`1904,250,1500`, released L, and then supplied brief forward movement after the
enemy was deleted. Before lock, diagnostics reported type/mode `41/0`, flags
`0x13`, active modifiers, a `-6.00`-degree latitude offset, and FOV convergence
from `61.43` to `66.33` degrees. Stable enemy lock changed the camera to mode 2
with flags `0x01`; modifiers became inactive and native/effective FOV matched at
`62.00/62.00`.

Release recovery remained fail-closed through native transition state. The
camera first returned to mode 0 with flags `0x03` while the native L-lock flag
was still set, then briefly entered mode 1 with flags `0x01`. Both states kept
native framing. When all lock state cleared, the same type-41 algorithm-1 chase
camera returned to mode 0 with flags `0x13`; both modifiers became active again,
the latitude offset returned to `-6.00` degrees, and FOV converged from
`61.33` to `66.23` degrees. No stale widened or lowered frame occurred during
release.

The ignored runtime evidence is
`build/visual-validation-v090/logs/dusklight-20260928-053319.log`. Temporary
camera-gate logging classified the transition but did not change camera state.
The logging and input harness were removed afterward, and the clean
source-matched host was rebuilt. This closes target-release recovery into active
exploration. Aiming, broader combat, special camera modes, and interactive
toggle coverage remain open before any default-on decision.

## Detached-camera recheck (2026-09-28)

A source-matched Windows D3D12 run used Dusklight's command console to detach
and reattach the camera in `F_SP103,0,27,0`. Before detachment, type-41
algorithm-1 chase reported flags `0x13`, active modifiers, a `-6.00`-degree
latitude offset, and FOV convergence from `61.43` to `66.33` degrees. While
detached, the FOV callback reported flags `0x0b`, stayed inactive, and preserved
native/effective FOV at `61.63/61.63`. No native chase callback applied a
latitude change while the detached camera owned the view.

Reattachment returned flags to `0x13`. The FOV modifier restarted its blend
from native, moving from `61.63/62.11` to `61.63/66.54`, while native chase
resumed the configured latitude offset. The transition showed no stale detached
state or one-frame modified output during detachment. The ignored runtime log
is `build/visual-validation-v090/logs/dusklight-20260928-054739.log`.

This closes detached/free-camera fallback and recovery. Aiming, broader combat,
other special camera modes, and interactive toggle coverage remain open before
any default-on decision.

## Bow-aim fallback recheck (2026-09-28)

A source-matched Windows D3D12 run equipped the Hero's Bow on X and supplied a
temporary held-X input in `F_SP103,0,27,0`. Before aiming, type-41 mode-0 chase
reported flags `0x13`, active modifiers, a `-6.00`-degree latitude offset, and
FOV convergence from `61.43` to `66.33` degrees. Drawing the bow entered
type/mode `41/7`. The service reported flags `0x01`, MidnaFX became inactive,
and native/effective FOV matched at `60.68/60.68`. The live frame shows Link's
drawn bow, arrow, aiming reticle, 30-arrow count, and first-person framing.

The ignored evidence is
`build/visual-validation-v090/camera-bow-aim-final.jpg` and
`build/visual-validation-v090/logs/dusklight-20260928-110836.log`. The isolated
test user had no physical controller mapping, so this harness proves entry and
native fallback during bow aim, not an independent bow-release recovery path.
Target-release and detached-camera recovery remain separately proven above.
The temporary loadout and input harnesses were removed afterward and the clean
source-matched host rebuilt.

This closes bow-aim fallback. Broader combat, other special camera modes, scene
transitions, and interactive toggle coverage remain open before any default-on
decision.

## Locked melee-combat recheck (2026-09-28)

A source-matched Windows D3D12 run equipped the Master Sword, spawned one
`E_BA` at the previously validated `F_SP103,0,27,0` target position, held L,
and supplied repeated B attacks. Exploration began in type/mode `41/0` with
flags `0x13`, active modifiers, a `-6.00`-degree latitude offset, and effective
FOV converging to `66.33` degrees. Enemy lock entered mode 2 with flags `0x01`;
MidnaFX became inactive and preserved native/effective FOV at `62.00/62.00`.

Sword attacks killed the enemy and created its native disappearance actor. The
camera then passed through fail-closed mode-0 flags `0x03` and mode-1 flags
`0x01`, preserving native FOV at `57.97/57.97` and `58.82/58.82`. After combat
and lock state cleared, mode-0 chase returned with flags `0x13`; the latitude
offset returned to `-6.00` degrees and FOV blended from `60.81/61.29` to
`60.82/65.70`. Captures show the sword drawn during combat and the stable
post-combat frame.

The ignored evidence is `build/visual-validation-v090/camera-combat-active.jpg`,
`build/visual-validation-v090/camera-combat-recovery.jpg`, and
`build/visual-validation-v090/logs/dusklight-20260928-113250.log`. The temporary
enemy, loadout, and input harnesses were removed afterward and the clean host
rebuilt.

This closes one locked melee-combat lifecycle spot check. Bosses,
effects-heavy combat, other special camera modes, scene transitions, and
interactive toggle coverage remain open before any default-on decision.

## Scene-transition recheck (2026-09-28)

A source-matched Windows D3D12 run began in `F_SP103,0,27,0`, then used the
native next-stage path to load `D_MN04` room 7, point 0. Before the warp,
type-41 mode-0 chase reported flags `0x13`, active modifiers, a
`-6.00`-degree latitude offset, and FOV convergence to `66.33` degrees. During
the new room's authored establishing view, type-40 mode-0 reported flags
`0x07`; MidnaFX stayed inactive and native/effective FOV matched at
`57.83/57.83`.

When the establishing view released control, native type-96 mode-0 chase
reported flags `0x13`. The latitude offset resumed at `-6.00` degrees and FOV
started a fresh blend from `60.00/60.47` to `60.00/64.84`. Captures show the
authored quarry overview and the later player-controlled room view. No stale
FOV or lower-angle output appeared in the authored view, and no stale camera
ownership survived the stage unload/load cycle.

The ignored evidence is
`build/visual-validation-v090/camera-scene-transition.jpg`,
`build/visual-validation-v090/camera-scene-recovery.jpg`, and
`build/visual-validation-v090/logs/dusklight-20260928-120412.log`. The temporary
warp harness was removed afterward and the clean host rebuilt.

This closes one cross-stage transition and authored-view recovery check.
Bosses, effects-heavy combat, other special camera modes, and interactive
toggle coverage remain open before any default-on decision.

## In-process toggle recheck (2026-10-02)

A source-matched Windows D3D12 run changed both camera configuration variables
through Dusklight's `ConfigService`, the same path used by MidnaFX's settings
controls. The fixed `F_SP103,0,27,0` view began with both controls enabled.
Turning both controls off immediately changed active type-41 chase to inactive:
latitude offset became `0.00` degrees and native/effective FOV matched at
`61.63/61.63`.

Enabling only exploration FOV kept the latitude modifier inactive and restarted
the FOV blend from native, first reporting `61.63/62.11` and then
`61.63/66.54`. Enabling the lower-angle control afterward restored the
`-6.00`-degree latitude offset without resetting or corrupting FOV. Matched
captures show native framing, FOV-only framing, and combined wide/lower
framing. The process then shut down cleanly and unloaded all mods.

The ignored evidence is `build/visual-validation-v090/camera-toggle-off.jpg`,
`build/visual-validation-v090/camera-toggle-fov-only.jpg`,
`build/visual-validation-v090/camera-toggle-both.jpg`, and
`build/visual-validation-v090/logs/dusklight-20261002-181250.log`. The timed
validation driver was removed afterward, and the clean package was rebuilt.

This closes in-process disable, independent FOV enable, and combined re-enable
coverage. Bosses, effects-heavy combat, other special camera modes, and wider
gameplay tuning remain open before any default-on decision.
