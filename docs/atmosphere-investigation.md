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

## Depth reconstruction diagnostic

The default-off **Visualize reconstructed depth** setting now replaces scene color with
grayscale radial camera distance. It samples the resolved `R32Float` depth texture, maps the
pixel center into WebGPU NDC, multiplies by `view_from_proj`, divides by homogeneous W, and
scales distance by the configurable diagnostic range. Background is dark blue; a magenta
sample means the reconstruction was non-finite, had an invalid W, or fell outside the
camera's axial near/far interval. Axial depth is used for clip validation because radial
distance at the corners legitimately exceeds the projection's far-plane distance.

The view replaces MidnaFX grading rather than stacking a second fullscreen effect. Each
active frame performs one depth resolve/copy, one 80-byte uniform upload, and one fullscreen
draw. It performs no color copy. Disabling the setting restores the ordinary grading path;
unsupported depth and submission failures preserve the original frame and log once.

A source-matched D3D11 run in `F_SP103,0,27,0` queued and encoded 258/258 diagnostic draws at
1216x896, with reversed Z and a 5000-unit display range. Median/p95 pre-HUD CPU callback time
was 13.40/32.00 microseconds; the latest depth resolve call took 15.50 microseconds. The process
remained responsive, closed gracefully, unloaded the mod, and logged no MidnaFX, WebGPU, or
validation error. This proves the shader pipeline, depth binding, camera uniform, and draw
submission path in the live renderer. Visual interpretation of the grayscale output remains
deferred by request until a controllable desktop session is available.

Actual fog color, density, height falloff, environment automation, and default-on behavior
remain out of scope until that visual inspection is complete.

## Correctness review

The pass validates the C++/WGSL uniform size, camera finiteness, homogeneous W, reconstructed
coordinates, texture dimensions, render-target layout, frame-scoped snapshot, and draw result.
One review finding was fixed: radial distance was originally compared directly with the
projection far plane, which could falsely mark valid corner pixels magenta. Clip validation
now uses absolute view-space Z while grayscale output remains radial distance. Unsupported
depth, stale/invalid camera data, and service failures fail closed without latching the normal
grading renderer into a failed state.

The diagnostic adds one pipeline at renderer initialization even while the view is off; it
adds no depth copy or fullscreen draw until enabled. Remaining proof is visual inspection for
orientation, monotonic distance, background classification, and absence of magenta samples.
Metal compilation is covered by CI, but live Metal output remains untested.
