# Performance budget and measurement

No GPU runtime measurement has been made. The proposed M2 target is under 0.25 ms at
3840×2160 for basic fused grading, excluding sharpening; it is a target to test on the
actual Intel Mac, not a guarantee.

M2 uploads one 32-byte uniform block and uses one scene `textureLoad` and one fused fragment
invocation per pixel. Each effect toggle substitutes a neutral parameter on the CPU; there
is no per-effect shader branch or runtime pipeline compilation. Fully neutral settings
bypass the snapshot and draw. Forced passthrough uses the separate cached M1 pipeline.

M1 records one color snapshot and one fullscreen draw when enabled. Aurora implements the
snapshot as a full-size `CopyTextureToTexture` after a pass break; M1 then reads and writes
one texel per pixel. At 4K, a four-byte snapshot is about 31.6 MiB. Its copy reads and
writes about 63.3 MiB, and the draw adds about 63.3 MiB of nominal texel traffic, before
MSAA, tile load/store and cache effects. This is an estimate from pixel counts, not measured
GPU bandwidth. One frame-local WebGPU bind group is created per active frame. Persistent
shader/pipeline objects are created once. Disabled state requests no snapshot/draw.
The active stage also calls `GfxService::get_scene_target_layout`, whose implementation
uses `AuroraGXSync()` (`src/dusk/mods/svc/gfx.cpp` in the pinned host). This is a GX
recording synchronization boundary, not evidence of a GPU queue wait. Its CPU cost is
included in the callback measurement and should be isolated during runtime profiling.

The UI reports a CPU stage-callback duration only while diagnostics is enabled. It does not
include later render-worker encoding or GPU execution. Counters distinguish queued and
encoded draw callbacks; they are not proof of completed GPU work. The SDK has no public
timestamp-query service. Pipeline creation count, bind-group count, resolution and status
are exposed in diagnostics. Host snapshot pool memory and device allocations are outside
the public mod API; the UI shows only a byte-size estimate. No frame allocations claim is
made until Dawn and allocator captures are taken.

On the Intel Mac, record OS version, GPU, driver/backend, host and mod revisions, texture
pack/Dawnlight versions, bloom mode and render scale. Use a fixed save, camera and time.
Capture 1080p, 1440p if available, and 4K with (1) mod disabled, (2) M1 passthrough,
(3) future M2 neutral, and (4) future grading. Warm pipelines and snapshot pool before
sampling. Obtain repeated median and high-percentile GPU frame timings, plus a GPU trace
that separates pass break, color copy, fullscreen draw and host composition. Report
enabled-minus-disabled distribution with uncertainty; do not infer per-pass time from total
frame time alone. Repeat Dawnlight and texture packs on/off if time permits.

Investigate any unexpected copy/resolve, per-frame heap allocation, pipeline rebuild, GPU
queue wait, texture transition or resize allocation. Compare M2 direct fused arithmetic
against a LUT only if profiling suggests the extra lookup/storage is worthwhile. Optional
sharpening needs a separate cost budget and can use nearby texels in the same pass only
after confirming its quality and bandwidth effects.
