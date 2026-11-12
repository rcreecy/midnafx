# Performance optimization receipt — November 12, 2026

The user's subsequent instruction to proceed led to a
[native CPU continuation](round2/README.md): exact reach reuse and stationary
instance-geometry reuse, with fresh paired native measurements. The evidence
below remains the original terrain-query/vector-storage receipt.

The next instruction to proceed produced
[unchanged instance-upload reuse](round3/README.md), with native paired timing,
complete instance-data parity and D3D12 buffer-retirement evidence.

The [fourth pass](round4/README.md) adds measured airborne-source reuse and records
the qualification fixes preceding the requested full-working-tree merge.

Two CPU optimizations are retained in the current local MidnaFX implementation:
an owned terrain-bounds index for grass tile requests, and reusable visible-tile
vector storage. The [architecture, execution graph, ranked backlog and future
measurement matrix](../../performance-program.md) describe the broader program.

## Authoritative baseline

Branch `codex/m12-water`, HEAD `2cd2858` (full identity in the JSON manifest), with
substantial local grass, material and color work. The
[initial manifest](workspace-baseline.json) records preexisting paths and hashes.
[Preservation verification](preservation.json) confirms that only
`src/render/ground_grass.cpp` and `tests/CMakeLists.txt` among those already-edited
files changed during this work. Their exact starting contents were reconstructed
and verified against the initial SHA-256 hashes before producing the
[isolated implementation diff](implementation.diff). New index, tests, runner and
documentation files are additional to that diff. All other preexisting edits,
including host patches, remain byte-identical. Neither upstream checkout was
modified, built, patched, reset or committed by this program.

## Optimization ledger

| ID / disposition | Baseline and mechanism | Measured benefit | Correctness / lifecycle / risk |
|---|---|---|---|
| CPU-01 / retained | Camera/revision-triggered grass request rebuild used `any_of` over every surface for each tile. Build an XZ bounds hierarchy once with each RoomCover and visit overlapping leaves only. | Spread synthetic terrain: paired optimized/baseline median 0.03635, approximately 27.5x faster for the isolated query batch. | Same inclusive overlap and unchanged per-surface frustum callback. Source surface order, request insertion order, stable distance sort, tile caps, seed consumption, placement and GPU rendering unchanged. Extra CPU memory and cold build work; degenerate overlap can be slower. |
| CPU-02 / retained | Three populated vectors reserved and freed temporary tile-pointer storage every frame. Retain their capacity, return by const reference, clear pointers on all stage exits. | Allocation model, 1000 frames: 3000 allocations/3000 frees become 3 allocations/3 final frees; zero warmed allocations for stable sizes. | Up to 6656 bytes of pointer capacity at configured caps on x64, plus vector objects. No borrowed pointer remains an active element between callbacks. Cover invalidation clears lists; shutdown releases capacity. |

No product optimization was reverted. Pilot measurements were superseded for the
specific methodological reasons below; the raw evidence is retained. No shader or
quality-mode changes were made. This is a measured CPU algorithm improvement,
not a measured whole-game speedup.

Final review also gated index construction on Ground cover being enabled:
water/atmosphere can discover the same RoomCover while grass is disabled, and
do not need that index. The existing toggle path invalidates all cover before
reenabling grass. This final integration guard does not change the benchmarked
index algorithm. [Delivery hashes and second smoke receipt](delivery.json)
identify the final sources/package separately from the benchmark's source hashes.
The final package was rebuilt and passed all five focused checks, then repeated
the same Hyrule Field launch/normal-close procedure with mod unload logged.
[Delivery stdout](delivery-stdout.log), [stderr](delivery-stderr.log), and
[package build log](delivery-build.log) preserve that result. The original full
test run and first smoke receipt above remain evidence of the preceding build;
the unchanged shaders were not redundantly retested after this CPU-only guard.

## Final controlled CPU benchmark

Reference machine: Intel Core i7-10700, Windows build 26200, x64 MSVC
19.50.35730.0 Release `/O2`, Balanced power policy. Intel UHD 630 driver
31.0.101.2140 is present but unused by the CPU benchmark. See
[hardware metadata](hardware.json) and [complete final report](terrain-query-final.json)
for binary/source hashes, dirty-worktree identity and all trials.

Each batch contains 24,300 synthetic tile overlap/frustum queries at three layer
sizes; these are not native-scene or frame-time samples. The synthetic boxes use
fixed heights and an uncomplicated camera; they isolate spatial search rather
than reproducing complete grass-request CPU cost. Two untimed warm paths precede
twelve alternating AB/BA pairs. The final benchmark ran without the build, CTest
or game processes competing for resources. The baseline uses `std::any_of` and
has no timed surface-test counter. Hits agree for every pair.

| Workload | Surfaces | Baseline batch p50 / p95 / p99 (ms) | Indexed batch p50 / p95 / p99 (ms) | Paired median ratio / observed range |
|---|---:|---:|---:|---:|
| Tiny | 8 | 0.217 / 0.243 / 0.266 | 0.184 / 0.188 / 0.191 | 0.847 / 0.677–0.872 |
| Spread | 8192 | 132.306 / 135.067 / 135.897 | 4.827 / 4.948 / 4.951 | 0.03635 / 0.03616–0.03740 |
| Fully overlapping | 8192 | 2426.648 / 2520.261 / 2540.218 | 2465.648 / 2550.062 / 2571.164 | 1.02265 / 0.95880–1.04554 |

The fully overlapping adversarial workload has no useful spatial separation and
shows a 2.3% median paired slowdown. Its paired range crosses parity; no statistical
significance or universal improvement is claimed. Retention is justified by the
large reproducible reduction for separated terrain and the limited observed
degenerate overhead. If native terrain traces show that degenerate case matters,
evaluate a measured linear fallback before extending the index.

The 8192-surface index holds 230,984 bytes of vector capacities, excluding its
object, allocator bookkeeping and transient build stack. The eight-surface index
holds 188 bytes. Index objects also add storage to each RoomCover (three vectors
and a flag, including padding). The single observed build times were 1.313 ms
for spread terrain and 0.399 ms for overlapping terrain, versus 0.0006 ms for the
tiny fixture. These are single cold observations, not tail distributions. Native
total process memory, heap allocations and GPU residency were not measured.

The allocation model retains 6656 pointer bytes at maximum list sizes. It counts
the real allocator calls of instrumented vectors with the production
reserve/append/clear sequence; it does not intercept production host allocations.

### Superseded trials

- [Initial pilot](terrain-query-initial.json): timed a baseline-only surface-test
  counter and used a manual loop. Useful for selecting the candidate, superseded
  by the original `std::any_of` form without that counter.
- [Concurrent pilot](terrain-query-concurrent.json): overlapped the native test
  suite and retained that counter. Not used for acceptance.
- [Counted sequential pilot](terrain-query-counted.json): no competing test/game
  workload, but still contained the baseline-only counter. Not used for the final
  performance ratio.

## Validation evidence

- Native Release build and `midnafx_package` succeeded. Compiler warnings include
  existing native headers and preexisting ground-cover signedness warnings; no
  new index warning or build error was observed.
- [Full native CTest receipt](tests.xml): **40/42 passed**. Passing checks include
  index correctness, grass camera math, trails, shader compilation, LUT palette,
  material-service fallback, ambient/bloom pixels, the complete main D3D12 pixel
  fixture, and package contract. This run preceded registration of the new Python
  runner test.
- [Focused receipt](focused-tests.xml): **5/5 passed**, including the subsequently
  registered runner validation, index, grass view, trails and package contract.
  The runner's four internal evidence-validation cases also pass independently.
- Index functional proof covers empty/small/large terrain, 20,000 randomized
  queries with a varying acceptance predicate, inclusive edge/point overlap,
  nonfinite fallback, rebuild, unload and slot reuse. These compare exact booleans,
  not approximate rendering metrics.
- Native Hyrule Field smoke run: `F_SP121,6,2,4`, D3D12, 1216x896, original
  textures, isolated user/mod directories. The game discovered 3195, 254 and 235
  grass surfaces in three loaded room parts; the settled receipt logged **38,809
  visible grass roots** (16,825 near, 18,600 middle, 3384 canopy). Trails, airborne
  particles, water, shared lighting, bloom, LUT and DOF ran. CloseMainWindow
  requested normal close; the process exited within the ten-second deadline and
  logged `all mods unloaded`. [Metadata](runtime.json),
  [stdout](runtime-stdout.log), [stderr](runtime-stderr.log).
- The native final-grading diagnostic reported CPU callback p50 **33.10 us** and
  p95 **54.80 us**, over 256 active samples. That timer excludes the grass stage
  and is not a before/after comparison, GPU timing or whole-frame result.

### Existing failed tests and runtime warnings

The full run is not green. `surface_map_contract` raises `StopIteration` because
the current optional Aurora patch lacks the expected new-file diff sections for
`surface_map_cache.hpp` / `surface_map_shader.hpp`. `surface_maps_pixels` invokes
`aa_pixels --surface-maps-only`, but the current executable implements only the
`--exposure-only` special argument; it consequently runs the full fixture and
hits its 60-second timeout. The ordinary `aa_pixels` run passes in 128.65 seconds
under its 240-second limit. The relevant patch and pixel-test source both match
their initial hashes. These failures belong to the preexisting unfinished
surface-map experiment; changing its host payload is outside this commission.
Neither failure is omitted, renamed as a skip, or represented as successful.

The native smoke also logged the existing missing `gczelda2` card-file error.
After mod unload it logged aborted buffer mappings and `Device was destroyed`
warnings. No WGSL/pipeline validation error was observed. A clean-looking frame
or clean error log is not claimed from that run.

## Limits and stop decision

Observed CPU benefits concern the isolated terrain query and vector-allocation
model. GPU improvements, native total-frame p50/p95/p99, frame-time variance,
physical memory traffic, low-end GPU scaling and total-memory changes remain
unmeasured. Pixel fixtures passed, but there is no frozen matched baseline versus
candidate grass image comparison. The native smoke does not prove camera-pan,
traversal, resize, reload, transition, replacement-pack or whole-game visual
equivalence. Automated index lifecycle proof and one graceful native shutdown are
the lifecycle evidence obtained here. macOS/Metal execution is unavailable on this
Windows host; best-effort packaging paths remain unchanged.

Additional optimization now needs native CPU traces or GPU captures to rank the
remaining stages. The highest-value next work is measuring the per-frame grass
reach scan and cold tile generation, then airborne candidate selection and eager
LUT/pipeline startup cost. Host resolve/synchronization and native shader/streaming
internals remain host-boundary blocked. No unsupported hooks or expanded host
patch dependency were introduced. All code remains local and unpublished.
