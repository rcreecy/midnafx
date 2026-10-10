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
   exercised before judging its boundaries. The pond smoke runs report zero
   marked draws, so visible water alone does not qualify optical composition.
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

Resume with a working input binding or physical controller and a Mac test host.
Do not remove the experimental label solely because automated pixel tests pass.
