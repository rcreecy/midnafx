# Water scene reflections: 2026-10-28

## Artifact and configuration

The local MidnaFX 1.13.0 Release package has SHA-256
`D0BD539F10BC30435F0FA815DA20CEC79C5E6FA817A963E905CF68F9845C32B6`.
The native Windows host has SHA-256
`444D8EE33ED78BA4A427810E35A62731F4D845CBAC7343B37B16A5C20F3C34A4`.
The [session receipt](native-on/session.json) identifies the package, host and
temporary route fixture. The fixture does not ship.

The [launch script](launch-native.ps1) creates an isolated user directory, disables
replacement textures and auto-save, and enters Ordon `F_SP103,0,27,0` with D3D12
debug logging. Enhanced water and scene reflections are initially enabled;
grading, camera changes, AA and contact effects are initially disabled. The
[temporary route](driver.cpp), with its [build definition](driver-CMakeLists.txt),
uses native JUT replay to move backward into nearby water and releases the replay
after completion. Raw [route rows](native-on/route.csv) end with `# complete`.

## Automated proof

The Release ALL build succeeds. The [focused tests](focused-tests.log) pass:
shader compilation, profile round trips and package contract. The shader test
uses Dawn's Null backend and does not establish native Metal execution.

The full existing `aa_pixels` executable exits zero with 400 PASS reports and no
FAIL reports; its [complete GPU output](gpu-tests.log) is retained. Existing
water, AA/detail, LUT, exposure and bloom cases remain active. New fixtures use
independently intersected geometry: a camera pitched down 15 degrees, a horizontal
water plane, an opaque wall and near/far planes at 1/100 units. Separate pre-water
source colors distinguish actual donor transfer from a changed tint.

- At 96×80, red and blue donors affect 1,762 and 1,724 receivers respectively;
  at 65×49 they affect 689 and 656. Color changes remain within the existing
  Fresnel bound and source alpha remains exact.
- Disabled reflections, zero reflection strength, inactive masks and missing
  visible wall depth retain exact previous paths or tint fallback.
- A water-mask fragment only 0.03 units behind an opaque donor preserves donor
  reflection. Water actually in front of that donor is rejected.
- Underwater viewing, opaque foreground and near-plane crossings preserve the
  original scene or tint. The clipped-hit fixture checks 2,784 receivers.
- Matched reversed-Z geometry retains reflected color within one output code
  value and preserves alpha. Targets 1×1, 3×5 and 17×13 remain bounded.

The direct GPU run takes approximately 118 seconds on this host, including
pipeline compilation. The existing 120-second CTest limit is raised to 240
seconds for compilation variance. This is not a game GPU frame-time measurement.

## Native integration

The [runtime log](native-on/runtime.log) records a qualified R32Float water surface
capture at 960×720, with `reversed_z=yes scene_reflections=yes`. That flag confirms
the settings and supported projection path, not a GPU reflection-hit count.
The [initial capture](native-water.jpg) shows the route's native water context.

Selecting Profile / Remaster enables Enhanced water and Water scene reflections;
the [controls capture](remaster-water-controls.jpg) records both On. The independent
toggle changes to [Off](reflections-disabled.jpg) and back to On. Editing the
independent toggle marks the profile Custom, as intended. The package reloads
through native UI and its qualified water path activates again. Fullscreen resize
continues rendering; the runtime diagnostics report a 1748×1021 target and the
[resized capture](native-resized.jpg) retains native HUD composition. Contact
shading, adaptive exposure and bloom execute after selecting Remaster.

Normal window close exits zero, recorded in the [exit receipt](native-on/exit.json).
The runtime log records all mods unloaded and zero water capture failures. No
shader or bind validation errors appear. The isolated empty memory card reports
`Failed to open file: gczelda2`; this known card diagnostic is retained in
[stderr](native-on/stderr.log).

## Limits

These native captures are integration observations, not matched visual-quality
comparisons. Native environment time, water animation and downstream effects are
not synchronized. Analytic GPU fixtures establish reflected color transfer;
native logging does not instrument GPU hit counts. Whole-game coverage, motion
stability, silhouette quality and GPU cost remain unqualified. Hidden, off-screen,
sky and translucent sources retain the previous tint. No new water identities
are established. macOS gameplay and Metal execution remain unverified; support
is best effort through native builds and portable checks.
