# Immutable instance upload — November 12, 2026

**CPU-05 retained:** reuse the existing complete GPU instance buffer when the
assembled geometry and all CPU trail values are unchanged. This follows the
user's next instruction to proceed and preserves the earlier CPU optimizations.
[Baseline identity](workspace-baseline.json), [isolated integration diff](implementation.diff),
[preservation checks](preservation.json), [delivered package/source hashes](delivery.json).
No upstream source, host configuration, host patch, remote or release changes.

## Mechanism and ownership

The previous path called `push_verts` every frame, including when the preceding
geometry cache had retained all instance attributes. The candidate compares live
trail components by IEEE bits while refreshing near grass and plants. The first
changed component marks the frame as changed; all trails still refresh. Even
sub-threshold response changes and signed-zero differences force fallback.

After eight consecutive unchanged frames, a mapped-at-creation WebGPU vertex
buffer receives a byte-for-byte snapshot of the existing clump vector. Later
unchanged draws bind that buffer at offset zero. Geometry/trail changes invalidate
the snapshot and use the original frame-local stream immediately. Empty batches,
unsupported sizes, missing device, allocation/map failure and pool backpressure
also fall back. Failed creation does not retry every unchanged frame.

The original WGSL bytes, single-buffer vertex layout, field offsets, stride,
instance count, draw call and uniform preparation are unchanged. Wind, actor
contact, lighting and projection remain current through the original uniforms.
There is no new quality setting or reduction in vegetation density or motion.

This uses existing GfxService device information and public WebGPU APIs. Native
GX draw input is storage-based; RMLUI rebinds its vertex ranges. The host invalidates
custom-draw pipeline/bind-group state as before. No host patch or new service is
needed to bind the owned vertex buffer.

Each queued payload pins its snapshot. The render callback releases its pin on
all exits, including missing context/pipeline/bind-group failure. Failed
`push_draw` releases the producer pin. Retired snapshots stay alive until all
pending callbacks finish. Encoded WebGPU commands retain their buffer references:
retirement releases references and never destroys an in-flight buffer. Mod
shutdown clears the pool after the host drains/cancels callbacks, matching the
existing bloom ownership pattern. Stage/toggle clear invalidates current data.

At most four snapshots of at most 8 MiB each are owned by the pool. Backpressure
uses streaming rather than overwriting a queued generation. This is a bound on
mod-owned references, not total physical GPU residency: encoded commands may
retain backend references until completion. The normal frame stream can retain
its prior capacity, so persistent snapshots add memory rather than establish a
net memory reduction.

## Controlled native comparison

Three reversed pairs use the round-two settled scenario and a predeclared window:
384 captured stages, discard the first 288, analyze all 96 ready samples. Runs
last 55 seconds with isolated copies of the same starting config and packages.
Hardware is i7-10700/UHD 630, D3D12, 1216x896 scale 1, original textures,
`F_SP121,6,2,4`, Balanced power policy. Manifests preserve hardware/driver,
host/package/config hashes, order, close status and every raw row.

All six processes exited normally with code 0. Each candidate used its immutable
buffer in 96/96 settled samples. Every pair matched the complete instance-data
hash, including trail values, at an immutable-frame proof with matching inputs.
Core geometry hashes also matched. No trial was omitted.
[Raw runs](native/manifest.json), [analysis](native-summary.json).

| Pair/order | Submission p50 before → after (us) | Five captured spans p50 before → after (us) | Summed-span p95 before → after (us) | Summed-span p99 before → after (us) |
|---|---:|---:|---:|---:|
| 1 / AB | 191.80 → 4.40 | 353.95 → 173.15 | 478.25 → 194.40 | 581.20 → 204.90 |
| 2 / BA | 189.15 → 4.50 | 345.20 → 173.15 | 432.73 → 194.25 | 470.65 → 205.67 |
| 3 / AB | 202.30 → 4.60 | 360.55 → 169.95 | 433.33 → 194.98 | 617.57 → 212.55 |

Median paired submission p50 reduction is **97.7%**, and summed-span p50 reduction
is **51.1%**. Trail comparison has a measured cost: assembly p50 rises from
132.55–134.35 us to 142.85–144.90 us. The submission saving exceeds that displaced
CPU cost in all pairs.

Reference frames request 2,387,220 bytes of instance-stream data each, versus zero
in every settled candidate frame. Across each 96-sample window, that removes
229,173,120 requested stream bytes. The pool owns one 2,387,220-byte snapshot
(about 2.28 MiB) in these runs. The log field `uploaded_bytes` counts the original
service stream only; it excludes the one-time mapped snapshot copy. These are
observed API payload/owned-capacity counts, not physical memory traffic, PCIe
bandwidth, allocator overhead or measured video-memory residency.

### Cold and changing frames

Snapshot activation first occurs at sample 285 in all candidates, before the
declared settled window. Maximum submission duration over all 384 samples is
627.6–812.3 us in candidates versus 342.7–486.2 us in references. This one-time
allocation/copy cost is retained in the [transition summary](transition-summary.json).
The corresponding maximum summed spans across the entire capture are
4.94–5.31 ms for candidates and 4.85–5.10 ms for references. These maxima include
generation/camera settling and scheduling effects; they are not isolated GPU
allocation times or proof of improved motion tails.

Changing geometry or trail values keep the original stream and still pay the
small trail-comparison bookkeeping. Native traversal, rapid alternating pauses
and camera motion need broader tail qualification. No speedup for those cases,
whole-game frame pacing or FPS is claimed.

## Correctness and lifecycle evidence

Focused CTest passes **9/9**: existing grass reach/view, trails, assembly, terrain
index, performance evidence and package checks, plus the new immutable-buffer
fixture and extended native-record validation. [Final test receipt](tests.xml).

The new D3D12 fixture runs on the UHD 630 and checks:

- Exact opaque buffer bytes through GPU copy/readback, including arbitrary IEEE
  bit patterns, using the actual production snapshot pool.
- GPU execution after the last pool reference is retired, with the copy already
  encoded; the encoded command retains the source buffer correctly.
- Multiple queued pins, changing generations, four-snapshot backpressure, warmup,
  empty/misaligned/oversized data, missing device, invalidation and shutdown clear.
- Vertex-only production usage as well as copy-source usage required for readback.

GPU validation scopes report no errors. Waits and readback exist only in this
fixture; the mod adds no GPU completion wait. The test is registered only on
Windows, avoiding an unavailable Metal CI gate. Mac package paths are unchanged
but were not built or executed here.

The shader string is byte-identical to the local baseline, and its vertex layout
is unchanged. This buffer-byte proof and native instance hashes are focused data
evidence, not a frozen complete image comparison. Normal native shutdown is
exercised in every pair; actual mod-manager hot reload, resize and scene transition
UI tests remain unperformed in this continuation. Pool retirement/invalidation
is exercised on the real D3D12 device.

Runtime stderr retains the existing missing `gczelda2` card-file error. Post-unload
mapping/device-destroyed warnings also remain; no clean-log claim is made. No new
WGSL or GPU validation error was found. The earlier two surface-map test failures
are preexisting; the full suite was not represented as passing in this round.

## Disposition and remaining work

Retained: reproducible CPU savings, zero steady repeated instance-stream payload,
exact instance data, unchanged shader/layout, bounded ownership and real-device
retirement proof justify the small persistent-storage and activation tradeoffs.
No product optimization was reverted or published. The delivered Windows package
matches the package exercised in all candidate runs.

The five spans remain producer wall durations, excluding discovery, proof hashing,
logging, simulation, render-worker work, GPU completion and presentation. No GPU
execution improvement or FPS gain is inferred. Dynamic attribute splitting is
deferred: the remaining measured submission cost is now only a few microseconds.
GPU profiling and unmeasured discovery/cold/moving scenarios should rank subsequent
work before more steady-state CPU caching complexity.
