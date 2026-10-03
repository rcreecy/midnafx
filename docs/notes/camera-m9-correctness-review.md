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
3. **Lock state was implicit in mode and algorithm classification.** The host
   now rejects modification when `dAttention_c` reports lock-on, when either
   camera target pointer is populated, or while the camera's L-lock state is
   active. This prevents a transition frame from inheriting exploration output
   if mode or camera-style values have not changed yet. A live L-trigger run
   entered mode 1 and kept native FOV and latitude. A follow-up explicit enemy
   spawn produced a visible target arrow and repeated mode-2 samples with flags
   `0x01`, inactive modifiers, and native/effective FOV `45.00/45.00`.
4. **Target release needed live transition proof.** An explicit `E_BA` lock in
   `F_SP103,0,27,0` moved from active algorithm-1 chase to mode 2, then released
   through mode-0 and mode-1 transition states while native L-lock state
   lingered. Those transition samples remained inactive. After lock state
   cleared, flags returned to `0x13`, both modifiers reactivated, and FOV and
   latitude resumed configured exploration output. This confirms the explicit
   lock guards fail closed without preventing normal recovery.
5. **Detached-camera recovery needed live proof.** Dusklight command-console
   detachment changed flags from `0x13` to `0x0b`, disabled FOV modification,
   and preserved native framing. Reattachment restored `0x13` and restarted
   the FOV blend from native. The native chase callback did not modify latitude
   while the detached controller owned the camera.
6. **Bow aiming needed live exclusion proof.** A temporary deterministic
   loadout and input harness equipped the Hero's Bow and held X in
   `F_SP103,0,27,0`. The camera changed from active type/mode `41/0`, flags
   `0x13`, to bow-aim mode `41/7`, flags `0x01`. MidnaFX preserved native FOV at
   `60.68/60.68`; the captured frame visibly shows the drawn bow and reticle.
   The harness had no physical controller mapping, so bow-release recovery was
   not claimed from this run.
7. **Locked melee combat needed live lifecycle proof.** A deterministic harness
   equipped the Master Sword, spawned one `E_BA`, held L, and supplied repeated
   B attacks. Lock mode 2 used flags `0x01` and native FOV `62.00/62.00`. Enemy
   death passed through inactive mode-0 and mode-1 transition states. Native
   chase then recovered flags `0x13`, the `-6.00`-degree latitude offset, and a
   fresh FOV blend. No stale modified frame appeared during combat exit.
8. **Stage changes needed live ownership proof.** A native next-stage warp from
   `F_SP103` to `D_MN04` unloaded and loaded scene state. The destination's
   authored type-40 establishing view used flags `0x07` and native FOV
   `57.83/57.83`. Type-96 chase later recovered flags `0x13`, the configured
   latitude offset, and a fresh FOV blend. No stale callback state survived the
   transition.
9. **Independent toggles needed live state-change proof.** Runtime
   `ConfigService` writes disabled both controls, enabled FOV alone, then enabled
   both. Disable restored native FOV and zero latitude immediately. FOV-only
   restarted its blend while latitude stayed inactive. Combined enable restored
   the latitude offset without corrupting FOV state. Shutdown remained clean.
10. **Native special modes needed entry and recovery proof.** A forced native
    first-person interval changed type-41 chase from mode 0/flags `0x13` to mode
    4/flags `0x01`. MidnaFX preserved native FOV throughout mode 4. Releasing
    the force returned to mode 0/flags `0x13` and restarted both modifiers from
    native state.
11. **Boss combat needed a real high-pressure camera sequence.** The source-
    matched host loaded Fyrus's `D_MN04A` arena and used the game's own demo-skip
    initialization before applying native lock and attack input. Lock mode 1,
    boss-special type 75, and the following authored type-40 camera all remained
    fail-closed with native/effective FOV equal and zero latitude offset. No
    unsafe transition sample, non-finite output, crash, or validation error was
    observed.

## Review result

No unresolved memory, ownership, arithmetic, or restoration defect was found.
Callbacks validate structure sizes and finite values. Host outputs are finite-
checked and clamped. Handles are owner-scoped and removed on detach. MidnaFX
uses versioned optional service fields, so an unpatched host leaves both camera
features unavailable. Disabling FOV immediately returns native output; chase
changes stop at the next native chase sample. The source retains native camera
smoothing, eye construction, collision, shake, interpolation, and rendering.

The remaining risks are visual tuning across the full game, additional boss and
special-camera classes, live Metal behavior, and wider gameplay tuning.
The 110% / 6-degree candidate is therefore default OFF. Extended controller
traversal and live resize are required before a later release may enable it by
default, but they do not block the opt-in M9 architecture or implementation.
