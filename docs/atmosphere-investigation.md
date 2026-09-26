# M10 atmosphere foundation

Source baseline: Dusklight `edf42c6a7202647b56dd2fcdef02d17671bc814b` and
Aurora `7f2801cd0133c9333eadb4e2e6b24100c328d328`.

M10 starts with a depth/camera architecture gate. Visual tuning and matched captures are
deferred until a controllable desktop session is available. This checkpoint does not add fog,
change native environment state, or claim a visual improvement.

## Existing composition boundary

The public pre-HUD stage remains the only suitable portable fullscreen integration point.
`mDoGph_Painter` invokes `GFX_STAGE_FRAME_BEFORE_HUD` after native bloom and other scene
composition but before the ordinary HUD. At this point, material fog, translucent mist, sky,
weather particles, and bloom are already composited into scene color. A depth-aware pass can
add atmosphere but cannot recover or independently remove those contributors.

Native fog and bloom writes remain rejected. `d_kankyo.cpp` owns continuous environment
interpolation, while the SDK exposes no mod-scoped ownership and restoration contract for
those values. Direct writes would compete with game state and host controls.

## Depth and camera contract

`GfxService::resolve_pass` can request a raw, single-sample depth snapshot. Aurora converts
supported depth targets to `R32Float`; unsupported devices return no depth view. The snapshot
is frame-scoped and requires a pass break and full-resolution copy. Any atmosphere effect
using it therefore adds material bandwidth beyond the existing color snapshot.

`GFX_STAGE_SCENE_BEGIN` supplies `GfxStageContext::game_view`, and
`CameraService::get_camera` snapshots its camera on the game thread. The later pre-HUD
callback has a null `game_view` in the observed runtime, so MidnaFX retains the latest valid
scene-begin snapshot rather than calling the service there. `CameraInfo` supplies
column-major view, projection, inverse, and combined matrices in WebGPU clip convention,
plus eye position, vertical FOV, aspect, and near/far planes. Dusklight documents reversed Z
as depth 1 at the near plane and depth 0 at the far plane. `CameraInfo::world_from_proj`
directly supports world reconstruction from a depth texel.

These two public services are sufficient for a portable depth-aware fullscreen prototype.
They do not supply object identity, material class, sky classification, or semantic masks.
Atmosphere must therefore fail closed when depth or a valid perspective camera is absent and
must avoid treating background depth as ordinary geometry.

## One-shot runtime probe

The developer-only **Run one-shot depth/camera probe** setting requests one depth snapshot
after a valid perspective camera appears. It logs depth availability, dimensions, reversed-Z
mode, FOV, aspect, near/far planes, eye position, and finite-matrix validation. It performs no
draw and changes no persistent renderer state. Toggling it off and on rearms the probe.

The probe may add one extra pass break when grading is active. It is default off and has no
steady-state cost after completing. Its purpose is to prove that depth and camera metadata are
available at the exact MidnaFX interception point before any atmosphere shader is written.

## Gate and next implementation

Gate 1 **passes**. A source-matched Windows D3D11 run on Dusklight
`edf42c6a7202647b56dd2fcdef02d17671bc814b` returned one non-null depth snapshot at
1216x896 with a finite matching camera: reversed Z, 61.25-degree vertical FOV, 1.357 aspect,
1.0 near plane, and 160000.0 far plane. The process remained responsive and logged no
MidnaFX, WebGPU, or validation error during the observation interval. It was force-stopped,
so graceful shutdown was not tested by this run.

The next narrow prototype is a default-off depth reconstruction diagnostic that visualizes
linear distance and rejects background, invalid, and non-finite samples. Actual fog color,
density, height falloff, environment automation, and default-on behavior remain out of scope
until matched visual inspection is available.
