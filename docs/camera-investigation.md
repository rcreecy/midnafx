# M6 camera investigation and architecture gate

Inspected pinned Dusklight `edf42c6a7202647b56dd2fcdef02d17671bc814b`.
The [DeepWiki camera page](https://deepwiki.com/igawa6/dusklight/3.2-camera-system)
was used only as a navigation map; every conclusion below is checked against
the pinned source. No camera override or in-game validation is claimed.

## Public CameraService and exact timing

`sdk/include/mods/svc/camera.h` defines CameraService 1.1. `get_camera`
snapshots a supplied `view_class` on the game thread and returns column-major
view, inverse view, WebGPU projection, inverse projection, combined matrices,
eye, vertical FOV in degrees, aspect, and near/far. Its operator API registers
callbacks with priority and registration order. The first callback returning
true wins; an output state supplies eye, center, FOV, and bank. Service
registration and ownership are in `src/dusk/mods/svc/camera.cpp:131–169`;
ordering and application are at `:174–239`. Unregistration erases the owned
handle, and mod detachment erases any remaining operators (`:241–242`).

The decisive constraint is `src/d/d_camera.cpp:1060–1070`:
`dCamera_c::Run()` calls `camera_run_operators(this)` before its native
controller logic and returns immediately if an operator accepts the state.
The host then calls `camera->Reset(...)` from the operator state. A continuously
active FOV-only operator would therefore skip TP's follow, collision, input,
and camera-mode updates; copied eye/center values would go stale. Returning
false ignores all changes to the copied operator state. This public API is
suited to a full replacement camera, not a composable FOV modifier.

`camera_execute` in `src/d/d_camera.cpp:11383–11442` runs the controller,
stores the view, applies PC wide-zoom correction, records/interpolates its
camera snapshot, then sets up the view. `camera_draw` at `:11495` constructs
perspective from `view.fovy`, `view.aspect`, near, and far. Native camera
initialization uses 60 degrees at `:596`, but this is not a fixed runtime FOV:
camera styles and authored sequences set different values, and PC wide-zoom
correction at `:11371–11372` changes vertical FOV with aspect/trim. The
ordinary field is vertical FOV, in degrees. Near/far are chosen by the game
and stage around `:11282–11302`; a FOV implementation must preserve them.

`GFX_STAGE_SCENE_BEGIN` fires in `src/m_Do/m_Do_graphic.cpp:2334` with a game
view, after camera execution/draw has made matrices. This checkpoint adds a
read-only probe through CameraService `get_camera` there. It copies only
FOV, aspect, near/far, eye, and callback duration to UI diagnostics. It does
not retain `game_view`, create GPU resources, or register a camera operator.
The probe reports native and effective FOV as equal because no override exists.
If no successful scene sample arrives for 500 ms, the UI marks the reading
unavailable rather than presenting stale FOV as current.

## Capability classification

| Feature | Class | Source-grounded decision |
|---|---|---|
| Read FOV, aspect, matrices, eye | A | `CameraService::get_camera` is read-only and valid from a world camera stage. Read-only scalar diagnostics implemented. |
| Change FOV while preserving native controller | D | Public operator accepts state only by skipping `dCamera_c::Run`; no post-controller setter exists. No override shipped. |
| Distance and height | C/D | `dCamera.cpp:2841–3354` resolves wall/ground constraints and selects eye. Moving the final eye would bypass collision and player-visibility policy. No stable pre-collision service input exists. |
| Pitch or lateral framing | C/D | Changing final eye/center after native resolution would also alter aim, clipping, and authored composition. No supported controller parameter API. |
| FOV interpolation | D | Numerically simple, but lacks a safe post-controller write point and semantic exploration gate. |
| Semantic exploration detection | C/D | `dCamera_c::nextMode` (`d_camera.cpp:1718–1844`) separates multiple target/aim states, but mode 0 alone does not exclude horseback, swimming, climbing, scripted styles, or interiors. Public CameraService does not expose mode/type/event context. |

There are currently no camera profiles in the UI. Vanilla is the only actual
behavior. Wide, Cinematic Adventure, and Custom would suggest an override
that cannot be safely applied. No FOV postprocess scaling was substituted.

## Native states and handoff

`nextMode` selects target/attention, projectile, first-person, and other
modes; event camera selection occurs in `dCamera_c::Run` around
`d_camera.cpp:1420–1429`. Authored talk/event routines set their own FOV,
for example at `:5372` and `:6118`. The debug fly camera and detached free
camera bypass the native controller before mod operators (`:1061–1067`);
Dusklight's free-camera setting is in `src/dusk/ui/settings.cpp:969`.
Mouse-camera controls are configured in the same settings file around `:1027`.
The public service does not provide a reliable classification for combat,
conversation, scripted scenes, item-get, horse, swim, climb, or confined
interiors. All are treated as unknown/native. No transition is run because
there is no safe state to transition toward. An immediate bypass would be
safer than blending across an authored camera transition if a future API
exposes reliable context.

## Projection, depth, and future Atmosphere

`CameraService::get_camera` builds projection and inverse in
`src/dusk/mods/svc/camera.cpp:53–119`. Matrices are column-major for WGSL;
view space is right-handed with -Z forward. It converts the game's
projection to WebGPU [0,1] depth and uses Aurora's actual reversed-Z mode
(near 1, far 0 by default). The service includes `world_from_proj` for
unprojection. `src/dusk/interp/camera.cpp:49–106` reconstructs the
presentation view and projection during frame interpolation. The
`GFX_STAGE_SCENE_BEGIN` snapshot is the view exposed before that scene's
depth rendering, but this source pass does not prove every later depth texel
or offscreen pass uses exactly that camera. A target capture must correlate
the stage snapshot, depth snapshot, interpolation step, viewport, and pass
ordering before Atmosphere uses its inverse projection.

## Required host API change for FOV

A safe implementation needs a new public post-controller camera modifier,
called after native `Run` and `store` but before interpolation records the
view and before `camera_draw` constructs projection. It should expose a
bounded vertical-FOV output while preserving native eye, center, bank,
aspect, and near/far. The host must identify when a demonstration, debug/free
camera, targeting, or other special state owns the view, or expose a
documented semantic context for the mod to decline. It must specify operator
ordering, priority, lifecycle, failure validation, and whether the result is
recorded into interpolation snapshots. CameraService's minor version would
need to advance, and the target Dusklight runtime would need that host change.
Only after that contract exists can MidnaFX offer enabled FOV controls and
camera profiles without suppressing native camera behavior.
