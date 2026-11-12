# Native CPU continuation — November 12, 2026

This continues after the user's instruction to proceed. The earlier terrain
index/vector reuse and all unreleased grass, palette and material work remain.
[Starting hashes](workspace-baseline.json), [preservation verification](preservation.json)
and the [isolated implementation diff](implementation.diff) distinguish new work.
No host sources, host build settings, host patch payloads or remote state change.

## Baseline and method

The native pilot measured reach at 58.25 us p50, tile cache/generation at 24.60 us,
assembly at 2758.50 us and submission at 263.05 us. Assembly dominates this CPU
path; steady tile generation did not justify a new algorithm here.
[Pilot summary](pilot-summary.json), [raw capture](pilot/manifest.json).

Trials use the existing x64 host, D3D12, i7-10700/UHD 630, 1216x896 at scale 1,
original textures, Hyrule Field `F_SP121,6,2,4`, one starting config and Balanced
power policy. Manifests record host/package/config hashes, driver, order, PIDs,
wall duration and close/exit status. Each process receives isolated config and
mod copies, then a normal-close request. Forced close and incomplete captures
are failures. The game file is read from the existing installation.

The existing diagnostics toggle now arms 384 raw grass samples in fixed storage.
Five producer wall spans in microseconds cover reach, request selection, tile
work, assembly and submission. Rows record readiness, total instance count,
camera XZ and furthest reach. Serialization runs in `mod_update` after capture;
small geometry proofs run outside the timed spans. Diagnostics off takes no
capture clock readings. Disable diagnostics or clear cover to reset the capture.
Storage is about 30 KiB plus small state, with no frame-local capture allocation.

These spans exclude resource discovery, hash proofs, logging, simulation,
render-worker execution, GPU completion and presentation. They are not thread
CPU cycles, GPU timings, complete callback timings, whole-frame percentiles or
FPS evidence. Raw data and the analysis scripts preserve these boundaries.

## CPU-03 — exact reach reuse: retained

The old loop recalculates `hypot` for every triangle corner, including repeated
vertices and unchanged camera XZ. RoomCover now owns unique XZ float pairs,
deduplicated by IEEE bits. No hull approximation, squared-distance replacement
or changed distance formula is used: float subtraction, `hypot`, `+512` and max
reduction remain intact. A scalar cache reuses the result only for bit-identical
XZ and the same terrain revision. Lens/reach clamping and tile size still update
through the existing functions. Cover invalidation resets the cache.

Three reversed pairs, each with 256 ready samples after the initial 128-sample
warmup, measured reach p50 **54.25 / 54.20 / 53.70 us** before and
**0.40 / 0.40 / 0.40 us** after. Median paired ratio is approximately 0.00738:
**99.26% less measured reach wall time** in this fixed-XZ workload. Matching
native XZ inputs have bit-identical furthest values in all pairs.
[Summary](native-summary.json), [raw captures](reach-native/manifest.json).
Moving-camera numeric parity is tested; native traversal speed is not measured.

Three loaded parts retain 88,416 bytes of point-vector capacities, reducing
11,052 corners to 4109 unique pairs. A vector object adds 9 KiB across 384 fixed
room-part slots on x64, plus metadata for dynamic object-fall covers; the scalar
cache is small. This is a documented memory-for-CPU tradeoff. Process residency
and allocator bookkeeping are unmeasured. Owned point storage resets with cover
and releases at shutdown. Construction is gated on Ground cover being enabled.

## CPU-04 — exact-input geometry reuse

The original assembly rebuilds selected grass/detail instance shapes every frame.
The candidate retains the existing owned clump vector only when tiles are ready
and all 39 relevant projection/view/eye/lens/reach float bit patterns, terrain
revision and room priority match. Any change takes the original loop. A source
comparison verifies the cache-miss loop is exact after declaration lifting and
indentation. No selection, density, seed, quota, shader, alpha or blending changes.

Near grass and low plants refresh their CPU trails on every cached frame. Wind,
native actor contact, lighting, projection uniforms and vertex uploads stay live.
Distant grass and stone/litter preserve their existing trail semantics. The cache
stores scalar keys and ranges into the existing clump vector, with no retained
game-resource pointer, second geometry vector or persistent GPU allocation.
Cover clear, toggle changes and shutdown invalidate it. Unit tests cover all key
components, incomplete generation, reset, room/revision changes and trail refresh.

### Exploratory transition window

The first 128-sample warmup did not finish native FOV settling. Three complete
pairs showed assembly mean ratios 0.640, 0.547 and 0.605, while p50 changed little.
Two pairs also had worse p95/p99 spans. These are not hidden or called settled
wins. [Transition summary](assembly-summary.json) retains every percentile.

Two original reference processes exited normally before the 384-row buffer was
emitted. Their candidates captured successfully but had incomplete paired
references. [All original runs](assembly-original/manifest.json) are retained;
the [combined manifest](assembly-combined-manifest.json) names every exclusion
and reason. [Replacement trials](assembly-makeup/manifest.json) use 55-second
wall windows to guarantee completion of the same stage-index capture. The harness
now rejects incomplete captures explicitly.

An extra [reuse-flag validation](proof/manifest.json) showed zero reuse at samples
128–223, partial reuse at 224–287 and reuse on every frame at 288–383. A post-hoc
look at the last 96 rows measured assembly p50 2478.70 → 133.95 us and p95
2624.78 → 164.75 us. That observation selected the window for a fresh declared
validation; it is not used alone as acceptance. Mixed transition tails remain
an open native pacing question, and no traversal-tail improvement is claimed.

### Fresh settled-camera validation

The final validation declares a 288-sample warmup before three reversed paired
runs, with 55-second windows and 96 ready samples per run. Geometry hashes cover
immutable instance points, shape, base and tip attributes at transition samples
and the final settled sample. The candidate explicitly marks reused frames, so
a matching hash can verify a cached frame rather than only a cache miss.

**Disposition: retained for exact stationary inputs.** All six fresh runs
completed normally with 96/96 ready samples. Every candidate settled sample
explicitly reports geometry reuse; references report none. Each pair has three
matching input/geometry proofs, including one explicitly reused candidate frame,
with zero geometry mismatches. Recorded furthest values match at shared XZ inputs.
[Final summary](settled-summary.json), [raw runs](settled/manifest.json),
[delivered source/package hashes](delivery.json).

| Pair/order | Assembly p50 before → after (ms) | Assembly p95 before → after (ms) | Assembly p99 before → after (ms) |
|---|---:|---:|---:|
| 1 / AB | 2.513 → 0.139 | 3.133 → 0.180 | 3.163 → 0.232 |
| 2 / BA | 2.506 → 0.134 | 2.635 → 0.158 | 2.904 → 0.164 |
| 3 / AB | 2.488 → 0.133 | 2.610 → 0.155 | 2.639 → 0.163 |

Median paired assembly p50 reduction is approximately **94.7%**. Summing the five
captured spans, p50 falls **2.71–2.73 ms → 0.342–0.386 ms** (median paired
reduction **86.9%**). Summed-span p95 falls **2.83–3.38 ms → 0.418–0.467 ms**;
p99 falls **2.90–3.43 ms → 0.450–0.568 ms**. These upper-percentile improvements
apply to this fresh settled-camera workload. The CPU-04 references already
include CPU-03; separate percentage savings must not be added together.

The original loop handles camera movement or new geometry, so no traversal
speedup is claimed. Earlier mixed-transition tail regressions are not explained
away by these settled results: they remain an open broader pacing question.
No extra clump vector or GPU memory is allocated for geometry reuse; the scalar
key/ranges add less than 256 bytes on this x64 build. The unchanged instance
upload remains a significant part of the measured remainder.

## Regression infrastructure and limits

`grass_reach` tests bitwise numeric parity, shared vertices, arbitrary IEEE
payloads, canopy thresholds, moving inputs, revision changes, unload and rebuild.
`grass_assembly` tests all key components and live trail refresh without changing
other geometry. `grass_cpu` uses analytic fixtures to validate units, means,
warmup, corrupt/missing records, bad spans, camera matching and geometry proofs.
These functional checks are separate from the opt-in native performance runs.

Final focused checks pass **8/8**, including the existing terrain index, vector
benchmark evidence checks, grass view, trails and package contract, plus the new
reach, assembly and native-capture checks. [CTest receipt](tests.xml). The final
Windows package builds, matches the package exercised in the fresh candidate
runs, and stays local. No product optimization was reverted; the early mixed
windows were retained and followed by fresh declared-window validation. The
initial full-suite 40/42 receipt remains historical, including its two known
surface-map failures; unchanged shaders were not redundantly retested here.

Replay retained data with:

```powershell
python tools/grass-cpu.py --manifest docs/evidence/performance-2026-11-12/round2/reach-native/manifest.json --output build/reach-replay.json
```

For new comparisons, give `tools/grass-native-bench.ps1` explicit reference and
candidate packages, host/game paths, starting config and a new output directory.
Use `-Seconds 55 -PairCount 3 -WarmupSamples 288` for the settled scenario. Retain
all trials and source identities. Choose the window before examining candidates.

Existing optional material integrations remain unchanged. Logs retain the missing
`gczelda2` card-file error and post-shutdown mapping/device-destroyed warnings;
clean logs are not claimed. The prior surface-map contract failure and unsupported
pixel-test argument remain preexisting. No shader was changed in this continuation.

Camera movement/traversal, reload/resize, other scenes, replacement packs and
whole-game visual coverage remain broader qualification work. Geometry hashes
are focused instance-data evidence, not complete image parity. GPU time, native
presentation tails and GPU-limited FPS remain unmeasured. Local Aurora requests
TimestampQuery only with `TRACY_ENABLE`; that flag is off in the working host,
whose enabled-feature log omits TimestampQuery. No host changes are made to
enable it. External GPU capture remains the next route. Metal is unavailable here.
