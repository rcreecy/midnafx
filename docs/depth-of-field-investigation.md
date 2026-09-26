# M11 depth of field foundation

Source baseline: Dusklight `edf42c6a7202647b56dd2fcdef02d17671bc814b` and
Aurora `7f2801cd0133c9333eadb4e2e6b24100c328d328`.

M11 begins with a focus-mask gate. The first checkpoint does not blur scene color or claim a
visual improvement. It proves that the depth reconstruction established for M10 can classify
pixels relative to a camera-space focus plane before MidnaFX commits to a blur algorithm.

## Narrow design

The default-off **Visualize focus mask** view reconstructs absolute view-space Z from the raw
depth snapshot and `CameraInfo::view_from_proj`. Manual developer controls select focus distance
and transition range in game units. Pixels nearer than focus become cyan, farther pixels become
orange, and the in-focus band approaches black. Background remains dark blue. Invalid,
non-finite, or out-of-frustum reconstruction becomes magenta.

The focus view overrides the atmosphere depth view when both developer toggles are enabled. It
replaces grading for that frame, requests depth only, uploads one 96-byte uniform block, and
queues one fullscreen draw. It never samples or blurs scene color. Unsupported depth, invalid or
stale camera data, and service failures preserve the original frame and fail closed.

## Why blur is gated

A single-pass full-resolution gather can create foreground color bleeding, unstable silhouettes,
and poor bokeh. A higher-quality separable or half-resolution design needs owned intermediate
textures, resize lifecycle handling, near/far separation, edge-aware compositing, and measured GPU
cost. Fixed manual focus is also unsuitable as a default gameplay feature. `CameraInfo` exposes no
gameplay focus target, and synchronous depth readback would add stalls.

Do not implement production blur until visual inspection confirms focus-mask orientation,
monotonic depth, near/far classification, background handling, and absence of magenta samples.
After that gate, compare manual focus, camera-target focus through a small CameraService extension,
and asynchronous center-depth autofocus. Prefer a stable gameplay target over frame-delayed depth
readback when the host can expose it safely.

## Acceptance and non-goals

This checkpoint passes when the shader compiles on Windows and Intel macOS, the source-matched
D3D11 host queues and encodes the focus diagnostic without validation errors, and the default-off
path performs no per-frame depth resolve or draw. Matched visual inspection remains deferred by
request.

This checkpoint does not add blur, bokeh shape controls, autofocus, camera overrides, motion blur,
temporal accumulation, atmosphere, anti-aliasing, or default-on behavior.

## Runtime evidence

A source-matched Windows D3D11 run in `F_SP103,0,27,0` used reversed Z, a 1200-unit
focus distance, and a 500-unit transition range. MidnaFX queued and encoded 257/257 focus-mask
draws at 1216x896. Median/p95 pre-HUD CPU callback time was 14.10/36.60 microseconds; the latest
depth resolve call took 6.40 microseconds. The process remained responsive, exited gracefully,
unloaded the mod, and logged no MidnaFX, WebGPU, or validation error.

A separate default-off run recorded 259 disabled samples, zero submitted/encoded draws, and zero
resolve time. This confirms no steady-state depth copy or fullscreen draw when grading and the
focus diagnostic are disabled.

## Correctness review

The review checked reversed-Z background handling, WebGPU NDC reconstruction, axial focus math,
near/far bounds, NaN/Inf rejection, texture dimensions, frame-scoped view lifetime, failure paths,
diagnostic precedence, and C++/WGSL uniform layout. One finding was fixed before the final runtime
pass: a WGSL `vec3f` padding member would align the uniform block to 112 bytes while C++ supplied
96 bytes. Three scalar padding fields now give both sides the verified 96-byte layout.

Remaining gate is visual only: confirm screen orientation, cyan/orange direction, black focus
band, background classification, and no magenta samples. No blur implementation is justified
until that evidence exists.

## Camera-target autofocus checkpoint

The next source-only checkpoint adds CameraService 1.4 `get_camera_target`. It snapshots the
rendered `view_class::lookat.center` and computes its positive axial view-space distance on the
game thread. This keeps MidnaFX out of camera ownership and avoids GPU readback. MidnaFX samples
the value beside `CameraInfo` at scene begin and can use it for the default-off focus-mask view.
When camera-target focus is requested but unavailable or stale, the diagnostic skips the frame
instead of silently using manual focus.

Dusklight's AO mod establishes the required future blur ownership pattern: half-resolution
storage textures, one queued compute chain, render-size recreation, and four-frame retirement of
old targets whose views may remain in render-worker payloads. A production blur should reuse that
pattern with separate near and far color targets before an edge-aware full-resolution composite.
Implementation remains blocked on the deferred focus-mask visual gate; this checkpoint adds no
blur and performs no extra work while the focus diagnostic is disabled.

A source-matched Windows D3D11 run then exercised the new service and opt-in diagnostic in
`F_SP103,0,27,0`. MidnaFX reported `source=camera-target`, an axial focus distance of 300 game
units, 256/256 submitted/encoded draws at 1216x896, and 11.60/23.20 microseconds median/p95 CPU
callback time. The process remained responsive for the 25-second bounded run, and logs contained
no MidnaFX, WebGPU, or validation error. Visual capture remains deferred.
