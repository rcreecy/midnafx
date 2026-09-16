# M6 camera checkpoint correctness review

Reviewed the uncommitted read-only camera probe and investigation against pinned Dusklight `edf42c6a7202647b56dd2fcdef02d17671bc814b`. This is a source and package-contract review, not an in-game camera test.

## Finding and resolution

1. **Stale camera values could appear live after the world-camera stage stopped.** The original `src/game/camera_probe.cpp` set `current.valid` on a successful `GFX_STAGE_SCENE_BEGIN` callback, but no callback runs in a menu or other frame without a perspective scene. The UI could then display the previous FOV/eye as current. This was resolved during review: the probe now records the last successful sample and returns `valid = false` from `snapshot()` when it is more than 500 ms old. Failed `get_camera` calls also clear `valid`. The UI suppresses old numeric values and displays its waiting state. Up to 500 ms of last-scene values may remain visible, an intentional freshness interval to tolerate irregular frames.

## Contract and implementation checks

- The pinned `sdk/include/mods/svc/camera.h` exposes CameraService 1.1. `get_camera` accepts a `view_class` pointer on the game thread and returns `MOD_UNAVAILABLE` for a non-perspective view. `CameraInfo` contains vertical FOV in degrees, aspect, near/far, eye, and six column-major matrices. The probe initializes `struct_size`, copies scalars, and never retains the borrowed game-view pointer. `GFX_STAGE_SCENE_BEGIN` passes `&camera_p->view` at `src/m_Do/m_Do_graphic.cpp:2334`; `gfx_run_stage` invokes stage callbacks on the game thread. UI calls are likewise restricted to the game thread. There is no cross-thread read/write in the current probe.
- The operator warning is correct. `src/d/d_camera.cpp:1060–1070` returns early from `dCamera_c::Run()` if `camera_run_operators(this)` accepts a state, and `src/dusk/mods/svc/camera.cpp:174–239` applies that state via `Reset`. Returning false discards edits. A continuous FOV-only operator would suppress the ordinary camera controller; no such operator is registered in this patch.
- The projection/depth description matches `sdk/include/mods/svc/camera.h` and `src/dusk/mods/svc/camera.cpp:53–119`: column-major matrix data, WebGPU depth convention, and Aurora's runtime reversed-Z selection. The investigation correctly avoids claiming that one stage-camera snapshot necessarily describes all later offscreen depth or presentation frames.
- The probe uses one stage-hook registration and unregisters its owned handle in shutdown. The SDK zeros the output handle before registration and checks ownership on unregister (`src/dusk/mods/svc/gfx.cpp:1299–1325`). It neither creates GPU resources nor imports an operator. CMake includes the new translation unit, and package manifest and contract test both use 0.5.1. I did not independently execute a build here; build and package results should be supplied by the implementation task.

## Limits

The source audit cannot validate actual camera values in targeting, scripted scenes, free camera, interpolation, resolution changes, or mod reload. The ordered target runtime checklist covers those cases. The proposed post-controller FOV modifier is a host API requirement, not implemented behavior; no Wide, Cinematic, or Custom camera profile is shipped. No further actionable source finding remains after the freshness fix.
