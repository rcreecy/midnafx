# M4 performance instrumentation and measurement

The native Windows package, portable tests, and a source-matched Windows runtime have
been run. The target Intel Mac GPU is not available, so **GPU timings at 1080p, 1440p,
and 4K have not been measured**. The basic grading target remains less than
0.25 ms GPU time at 4K, excluding optional sharpening. It is a target, not a result.
No 4K performance claim should be made until a trace on the target hardware supports it.

## Cost model from the pinned source

An active, nonneutral frame uploads one 48-byte uniform block, requests one scene
snapshot, and queues one fullscreen draw. Ordinary fused grading uses one
`textureLoad` per pixel, while enabled detail uses five in the same draw; debug modes
reuse that snapshot and draw. No control edit recompiles a pipeline. Aurora's
`resolve_pass` encodes a full-size color copy after a pass break
(`upstream/dusklight/extern/aurora/lib/gfx/recording.cpp`); the mod has no zero-copy
feedback path. The draw creates a frame-local WebGPU bind group because the snapshot
view and streamed uniform slice are frame-scoped. The mod owns no full-resolution target.

| Resolution | RGBA8 snapshot | Copy read + write | Nominal copy + grade read/write |
| --- | ---: | ---: | ---: |
| 1920×1080 | 7.91 MiB | 15.82 MiB | 31.64 MiB |
| 2560×1440 | 14.06 MiB | 28.13 MiB | 56.25 MiB |
| 3840×2160 | 31.64 MiB | 63.28 MiB | 126.56 MiB |

These are byte counts from `width × height × 4`, not observed memory traffic. Tile
loads/stores, compression, cache reuse, host snapshot-pool behavior, and GPU scheduling
can change physical traffic. The 4K nominal total divided by 0.25 ms is about
531 GB/s, which makes the target worth testing carefully on the Intel Mac. Shader
arithmetic and pass transitions add further time. A LUT would add a texture lookup
and storage, so it is deferred until a trace indicates arithmetic is limiting.

## Changes made for M4

- Master-disabled and fully neutral grading with detail off and Final view return before the layout query, snapshot,
  uniform upload, and draw. CPU timing is optional. When diagnostics are off, the
  stage takes no clock readings or timing-window writes.
- Pipeline collections are cached by layout key. After an unseen supported layout, the stage
  bypasses one frame and `mod_update` builds the pair outside the render-stage callback.
  Parameter changes do not rebuild shaders or pipelines. Old collections stay alive
  until the host drains draw callbacks during shutdown.
- Diagnostics show latest active and disabled stage CPU time, rolling p50/p95 over 256
  samples, latest layout-query and `resolve_pass` **CPU call** time, latest grading
  configuration-update time, snapshot request count, and pipeline/bind-group counts.
  The reset button clears timing windows before each test. Enabling diagnostics starts a
  fresh window and emits one machine-readable log summary after 256 active, disabled, or
  neutral samples, including effective controls, counters, resolution, and CPU timings.
  This also works when the settings panel is closed. Percentile sorting never runs in the
  render-stage callback.
- There is no per-frame heap allocation in MidnaFX's steady-state stage path. A new
  layout allocates a cached pair during `mod_update`. The host's stage dispatcher and
  WebGPU bind-group implementation may allocate internally; this needs an allocator
  capture to characterize. The mod does not call a GPU queue wait in steady state.
- Optional detail adds four neighbor reads per pixel to the ordinary source read,
  with explicit bounds clamping. The original half of A/B Split returns after the
  center read. See `detail.md` for read counts and the chosen limiter.

`get_scene_target_layout`, `push_uniform`, `resolve_pass`, and `push_draw` each enter
the pinned host's `AuroraGXSync()` boundary in
`upstream/dusklight/src/dusk/mods/svc/gfx.cpp`. This synchronizes GX command
recording; it is not evidence of a GPU completion wait. The CPU timings include host
service-call time, while draw encoding happens later on the render worker. Public
GfxService 1.2 has no timestamp-query API; GPU pass/copy duration requires a platform
GPU capture. The disabled callback timer excludes the host's dispatch and surrounding
`AuroraGXSync()` calls, so total disabled-state overhead still needs an external CPU
trace. The counters report queued/encoded work, not completed GPU work.

## Windows runtime checkpoint

A source-matched Dusklight build at `edf42c6a7202647b56dd2fcdef02d17671bc814b`
ran the Windows package on D3D11 at 1216x896 in `F_SP103,0,27,0`. The host used an
Intel Core i7-10700 and Intel UHD Graphics 630. Both 12-second runs stayed responsive
and logged no MidnaFX, WebGPU, or validation error. Processes were force-stopped after
sampling, so this checkpoint does not prove graceful shutdown.

With the Natural / Vivid Realism values forced (`0,0,100,102,108,25,0,0`, detail 12%),
259 active frames produced 20.20 us p50 and 51.40 us p95 stage time. The latest layout
query took 3.50 us and the `resolve_pass` CPU call took 18.20 us. Submitted and encoded
draw counts both reached 259. With grading disabled, 268 frames produced 0.10 us p50
and 0.30 us p95 callback time, with zero submitted draws. A third run recorded 267
neutral-bypass frames with zero submitted draws. These CPU measurements do not measure
copy, shader, draw, or whole-frame GPU time.

## Target Mac measurement procedure

Use a fixed save, camera, time of day, bloom mode, Dusklight revision, MidnaFX revision,
texture pack, and Dawnlight revision. Record Mac model, Intel GPU, macOS version,
native graphics backend, and render scale. Warm the scene and pipeline for at least
several seconds. At 1920×1080, 2560×1440 if available, and 3840×2160:

1. Enable CPU diagnostics, disable grading, press **Reset CPU timing samples**, wait for
   at least 256 frames, and record disabled p50/p95 and total-frame p50/p95.
2. Enable grading with neutral values, reset samples, and record the neutral bypass.
   Snapshot count must remain zero after reset.
3. Apply a nonneutral saved preset, reset samples after warm-up, and record active
   p50/p95, latest layout/resolve-call time, snapshot count, and pipeline count.
4. Repeat with **Force passthrough comparison** to separate shader arithmetic from
   the common copy/draw path. Restore the preset afterward.
5. Capture a Metal GPU trace. Measure scene copy, pass break/transition, MidnaFX
   fullscreen draw, and whole-frame GPU time independently. Check whether a resize
   allocates a new snapshot-pool texture, whether a format change builds a new pair,
   and whether bind groups or host callbacks allocate each frame.
6. Repeat with Dawnlight and the high-resolution pack both off and on. Compare
   enabled-minus-disabled distributions and report sample count, median, p95, and
   uncertainty; do not infer a pass time from whole-frame deltas alone.

| Resolution | Disabled CPU p50/p95 | Active CPU p50/p95 | Copy GPU p50/p95 | Draw GPU p50/p95 | Frame delta p50/p95 |
| --- | --- | --- | --- | --- | --- |
| 1080p | Awaiting Intel Mac | Awaiting Intel Mac | Awaiting GPU trace | Awaiting GPU trace | Awaiting Intel Mac |
| 1440p | Awaiting Intel Mac | Awaiting Intel Mac | Awaiting GPU trace | Awaiting GPU trace | Awaiting Intel Mac |
| 4K | Awaiting Intel Mac | Awaiting Intel Mac | Awaiting GPU trace | Awaiting GPU trace | Awaiting Intel Mac |

If GPU copy dominates, the next optimization decision belongs at the Dusklight/Aurora
integration boundary; changing grading arithmetic will not remove the copy. If draw
dominates, inspect texture-fetch bandwidth and the grading shader before adding a LUT
or sharpening. If CPU layout/resolve calls dominate, use a host trace to identify the
specific GX synchronization cost before altering call order.
