# Stable release qualification

Status on 2026-10-09: release candidate evidence, not a stable qualification.
The published v1.2.0 Windows package remains unchanged and AA remains experimental.

## Completed evidence

- All 16 Windows and Intel Mac CI tests pass.
- Three additional consecutive Metal executions pass all 25 pixel cases on
  Apple Paravirtual hardware. The original hosted failure remains unexplained.
- Windows live AA toggling, native reload, +1 EV grading bypass with AA enabled,
  and Ordon-to-Fishing Pond transition pass smoke checks.

## Remaining acceptance checks

1. On Windows, compare AA off/on while walking and panning past fences,
   foliage, thin geometry, and high-contrast silhouettes. Record the same route
   and settings for both states. Check added shimmer, lost thin features,
   excessive softening, and HUD changes. Still captures alone do not pass this.
2. Compare shoreline and intersecting geometry with AA off/on and enhanced
   water off/on. Confirm diagnostics show the intended enhanced-water path is
   exercised before judging its boundaries. The pond logs confirm
   `Water absorption active`; zero `marked_draws` counts diagnostic overrides,
   not the production enhanced-water path. Activation is established, while
   boundary quality still requires the matched visual check.
3. On a physical Mac, verify package loading, AA off/on, grading bypass, reload,
   scene transition, and the motion/water checks above. Preserve hardware,
   backend, package hash, logs, and captures with the result. Hosted pixel tests
   do not substitute for live gameplay.

## Current input limitation

`build/m13-aa-runtime/release120-motion-pond` uses an isolated configuration and
the published package on the local patched D3D12 host. Native settings confirmed
Port 1 had no device and all stick directions were unbound. Assigning Keyboard
succeeded; restoring defaults left the directions unbound. Forward binding
capture stayed at “Press a Key or Mouse Button...” after injected W, mouse click,
and drag input. No movement or temporal-quality pass is claimed. The cause of
the input capture failure is not established.

`binding-capture.png` preserves the UI state. The host was closed and all mods
unloaded. Existing buffer-mapping/device-destruction warnings remain. User
settings outside the isolated test directory were not modified.

The user subsequently authorized Mac release without a physical Mac available.
The passing Intel Mac CI artifact was added to v1.2.0 with the untested
gameplay limitation disclosed. This does not change the stable qualification
status or turn the Mac gameplay check into a pass.

Source review explains two input details: Aurora's default keyboard mappings
are all `PAD_KEY_INVALID`, so restoring defaults intentionally does not assign
movement keys. The controller UI polls `SDL_GetKeyboardState` and
`SDL_GetMouseState` during updates, while Aurora drains queued events. A press
and release processed before that poll can be missed. This is a plausible
mechanism for the injection failure, not a demonstrated diagnosis of every
attempt (including drag). A held-input test or event-level trace is still needed
before changing the host capture implementation.

Resume motion checks with a working input binding or physical controller.
Do not remove the experimental label solely because automated pixel tests pass.
