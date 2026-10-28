# Movement framing: 2026-10-27

## Artifact and configuration

The local Release package is MidnaFX 1.10.0, SHA-256
`A0BBB4EEB4FE954D8CB647714E73028136E6DB042C998A491AD23FEF094DF1CB`.
It runs on the existing CameraService-patched Windows host, SHA-256
`444D8EE33ED78BA4A427810E35A62731F4D845CBAC7343B37B16A5C20F3C34A4`,
using D3D12 at 960×720. Native stage is `F_SP103,0,27,0` (Ordon).
Static camera modification is off; movement framing is on at 8%, with the
default transition. Grading, detail, exposure, AO, bloom, atmosphere, DOF and
geometry smoothing are off. AA and enhanced water are on. Auto-save is off.
No external texture pack is loaded in the replay trials.

The route uses an isolated SDK validation mod, not a product feature. Its
[source](driver.cpp) and [build definition](driver-CMakeLists.txt) are retained;
the binary does not ship. It temporarily owns an otherwise unused native pad
replay pointer, restores it after completion/unload and leaves native game speed,
camera, animation and movement calculations in control. It records actual camera
service output and native player speed, not just requested settings. Session
receipts retain the driver hash separately from the product hash.

## Results

- The complete Release build and [four focused tests](focused-tests.log) pass:
  camera envelope/clock/reset properties, profile compatibility, existing camera
  host patch contract and package contract. Review repaired static FOV pre-warming
  while only movement framing was enabled, then closed that finding.
- Final on/off routes use the same 275-tick input program: idle, 35 ticks of
  movement, 90 ticks of released input, 30 ticks of targeting, then release.
  Native movement reaches 23 native speed units and later enters the nearby water;
  its native maximum changes to 8. Both trials complete, unload all mods and
  return process exit code zero.
- A separate product-only native trial displays the independent Basic toggle,
  switches it off/on, reloads the module, remains active and closes with exit
  zero. [Lifecycle excerpts and final configuration](native-ui-final/) retain
  the result; no replay driver is present in that trial.
- [260 paired non-event observations](summary.json) have identical native speeds
  across enabled/disabled routes. Observed FOV's tangent ratio ranges from 1.0
  to 1.079999998. Maximum observed enabled FOV is 65.715431 degrees; the disabled
  route reaches 61.855637 degrees during that phase. Released input includes
  native deceleration; it does not imply instantaneous zero speed.
- At the final rest sample (tick 214), speed is zero and FOV equals the disabled
  route exactly. Every targeting sample also has a tangent ratio of exactly 1.
  Native camera diagnostics report mode 1, flags `0x01`, and no modification.
- A separate forward route reaches running and full expansion, then triggers an
  authored conversation. Native camera type 40, flags `0x07`, immediately reports
  identical native/effective FOV and zero movement contribution. The route waits
  for the conversation and times out rather than completing. Its normal close
  returns zero; this is event-gate evidence, not a completed gameplay route.

Raw rows and diagnostic excerpts remain under [enabled](route-on-final/),
[disabled](route-off-final/) and [conversation](route-replay/). Screenshots are
unmatched observations, not pixel-quality comparisons. `route-observed.jpg`
shows the conversation; `route-final.jpg` shows the final water position.

## Retained unsuccessful attempts and limits

The first native trial could not accept input while the desktop was unavailable
and was terminated explicitly; no clean exit is claimed. After desktop access
returned, a mouse-bound trial remained stationary and closed with exit zero.
`idle.jpg` and `after-input.jpg` retain that negative result.

The first SDK driver package had an invalid hyphenated mod ID and did not load;
that host closed normally. Three subsequent virtual-pad trials loaded but
produced no movement. A diagnostic trial established that raw `PADRead` saw
forward input while the game's processed stick remained zero. Source inspection
explained the gap: touch synchronization clears virtual port 0 immediately before
the game reads it, including when touch controls are disabled. The native replay
fixture runs downstream of that clearing. These attempts do not establish camera
movement quality and are not counted as passes.

The completed routes establish bounded expansion, settling, targeting bypass and
native game-state parity for this short route. They do not establish whole-game
camera comfort, mounted/wolf/combat behavior, every cutscene, or GPU/frame-time
cost. No Metal execution or Mac gameplay was performed. Mac support remains
best effort through native packaging and portable checks.

The empty card reports `Failed to open file: gczelda2`. After all mods unload,
the host reports aborted buffer mappings and `Device lost: Device was destroyed.`
These known teardown warnings remain in the retained excerpts; the observed
native processes exit zero. They are not shader or camera validation failures.
