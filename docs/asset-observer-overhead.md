# Asset observer overhead: Windows process counters

Nine matched Windows D3D12 trials provide a first process-level CPU and retained-memory comparison. They do **not** close the full observer overhead gate. The enabled trials execute the shape callbacks but produce no resource identity events, draw labels or encoded `AssetDraw` records. The initial discovery shortfall needs a separate investigation before measuring active discovery and sampling. These results cannot stand in for that workload.

The experimental host patch remains separate from the ordinary package. Both asset diagnostic settings remain disabled by default. No visual behavior, production default or release version changes in this milestone.

## Observed results

Each entry below is the median of three trials, followed by their minimum and maximum. CPU units are milliseconds of accumulated process CPU time per wall second, across all process threads. A value of 1,000 represents one fully occupied logical processor; it is not a frame time or machine-wide CPU percentage. Memory entries are medians within each trial, then summarized across trials.

| Configuration | Process CPU, ms/s | Private committed memory, MiB | Working set, MiB |
|---|---:|---:|---:|
| Baseline host, diagnostics off | 382.780 [367.554, 384.533] | 1556.688 [1556.309, 1571.512] | 1341.453 [1340.906, 1363.684] |
| Patched host, diagnostics off | 384.584 [382.615, 392.885] | 1558.266 [1558.191, 1572.152] | 1342.137 [1341.820, 1364.297] |
| Patched host, diagnostics on, no discovered assets | 422.068 [393.484, 438.121] | 1558.090 [1556.461, 1558.379] | 1341.426 [1340.527, 1341.684] |

The patched-off CPU median is 1.804 ms/s higher than baseline, or 0.47%; the observed ranges overlap. Enabled callbacks add 37.483 ms/s over patched-off, or 9.75% of that process CPU median. This is an observed difference on one system, not a causal estimate with a confidence interval. Dynamic actors, host scheduling, compiler output and cache state remain possible contributors. No FPS cost or speedup follows from these counters.

Patched-off private memory is 1.578 MiB higher at the group median. Enabled private memory is 0.176 MiB lower than patched-off; the negative difference does not establish a saving. Private committed memory describes retained process memory, not total allocated bytes or allocation rate. The working set also includes shared resident pages. The higher first baseline/off memory observations remain in the evidence; no trial is discarded.

## Native object layout

The [layout probe](../tools/asset-layout.cpp) compiles against the same pinned Aurora headers and MSVC x64 `NDEBUG` configuration as the host. It initializes no GPU. Baseline and patched outputs are recorded in the summary.

| Object | Baseline bytes | Patched bytes | Difference |
|---|---:|---:|---:|
| Inline payload capacity | 128 | 160 | +32 |
| `gfx::detail::Command` | 152 | 208 | +56 |
| `gfx::detail::DrawCommand` | 136 | 168 | +32 |
| `gfx::detail::CustomDrawCommand` | 144 | 176 | +32 |
| `gfx::detail::EncoderTask` | 144 | 176 | +32 |
| `gfx::TextureRef` | 56 | 64 | +8 |
| `gx::GXBindGroups` | 8 | 24 | +16 |
| `gx::GXDrawData` | 128 | 144 | +16 |

The outer command grows by 56 bytes (36.84%), including the retained debug-group representation and layout effects. The payload-capacity increase alone understates this object-size change. These are per-object ABI measurements, not total allocations: command counts, vector capacities, copied strings and allocator bookkeeping are not measured. Do not sum overlapping container and payload sizes to estimate total memory.

## Controls and exercised paths

The host is built from Dusklight `edf42c6a7202647b56dd2fcdef02d17671bc814b` and Aurora `7f2801cd0133c9333eadb4e2e6b24100c328d328`, using MSVC RelWithDebInfo x64. The baseline removes only the experimental draw-trace patch. Existing camera, menu and game-clock host changes remain common. The synthetic registry-test source patch remains applied but contributes no runtime test code. Both executables use the same runtime DLL directory. The traced executable is preserved before the baseline rebuild and restored afterward; this comparison also includes any incidental compiler/linker output differences between the builds.

Host SHA-256 values:

- Baseline: `f1645430f7dbdbb9b1af6ade6b2e4b72cb017a7d934319349ef9a425139ab2de`.
- Traced: `482aa1fcc8c4789297ba3b4d8476582a8e1868c33be578a982628ca35e8f3e8b`.
- Identical mod package in all trials: `15ec60f960a1277f4441fd26358ef13196534018b8be29ab77e25bb6ab9dbcb3`.

The machine uses an Intel Core i7-10700 with 16 logical processors, Windows 11 Pro build 26200, and Intel UHD Graphics 630, driver 31.0.101.2140. Every host log confirms the Intel D3D12 adapter. A remote display adapter is also present. Results are limited to this desktop session and hardware.

Trials run in order `b1, o1, e1, e2, b2, o2, o3, e3, b3`, with baseline, patched-off and patched-on occupying each position within a three-run block once. Each uses a fresh isolated user/mod directory, Ordon `F_SP103,0,27,0`, a 960×720 client window, VSync on, AA on, and native texture replacements on. The same replacement root contains only verified copies of the two pumpkin DDS files under `A-first`; their source hashes and sizes are documented in the [reload experiment](asset-hot-reload.md). Other explicit launch settings match across all trials. Unspecified settings use the same package/host defaults. Native shader caches are shared across launches and are not reset.

After activating and visually checking each scene, collection waits 20 seconds, then reads process counters at approximately one-second intervals: 31 observations spanning 30.353–30.457 seconds. There is no UI input during collection. The scene is visually checked again and the owned host closes normally. Idle animation and NPC positions vary; this is not a deterministic frame replay. Screenshots remain local, outside the metadata evidence.

All enabled trials log a positive `draw-cost.callbacks` count, confirming the enabled shape path executes. **None logs identity loads or draw labels**, and no trial logs an encoded asset draw. The available logs do not establish why discovery is absent. Consequently, this comparison measures enabled callbacks without discovered identity records. It does not measure the populated registry, initial 32-label burst, manual resampling, archive discovery, or reload workload. Patched-off trials still include the host's bounded provider events, which are independent of the mod toggles.

All nine sessions log normal mod unload and host shutdown. Each reports the same missing `gczelda2` memory-card file in its fresh user directory. Shutdown reports buffer mapping aborted during destruction and device destroyed; no earlier device-loss event or stdout error is present. These common messages are retained in the evidence.

## Evidence, reproduction and remaining gates

[Raw counters and sanitized sessions](evidence/asset-overhead-2026-10-21/runs.json), [analysis and layout results](evidence/asset-overhead-2026-10-21/summary.json), and [hardware metadata](evidence/asset-overhead-2026-10-21/hardware.json) contain no game or texture payloads. Original capture, log and screenshot hashes identify the ignored local files under `build/asset-overhead`. Launch paths and process IDs are removed from the published sessions; the common local RVZ path is replaced with a consistent marker. Metadata events and log observations preserve the actual exercised scope.

The [analyzer](../tools/asset-overhead.py) accepts run directories containing `session.json` and `process-samples.json`. A session records its name, variant, host/package hashes and explicit configuration. A capture records the matching name/variant, warm-up duration, and 31 samples of elapsed seconds, cumulative CPU seconds, private bytes and working-set bytes. On Windows these counters come from `Get-Process`, `TotalProcessorTime.TotalSeconds`, `PrivateMemorySize64` and `WorkingSet64`, with a monotonic stopwatch for elapsed time. Divide CPU delta by wall delta and multiply by 1,000; divide byte counters by 1,048,576.

The analyzer rejects insufficient trials, invalid timing/counters, duplicate identities, unmatched configuration/package controls, and inconsistent host binaries. It validates local disc-path equality before omitting that path from its output. Eight Python tests cover independent arithmetic expectations, rejected evidence and exact reproduction of the recorded summary. CTest includes this suite as `asset_overhead` on both platforms.

The Windows Release package rebuild succeeds. Full local CTest passes **19/19** in 50.38 seconds, including the eight analyzer cases and the existing 16 asset-intelligence cases. Whitespace checks pass. The rebuild produces package SHA-256 `bc88117e99bc93c99b6880e9da20dfc634789ff1a423d660ad88a4d6dd935ef0`; profiling uses the preserved earlier package hash listed above. Both experimental patches pass their applicability checks, and the primary host executable matches the preserved traced binary after restoration.

Hosted [CI run 39604108904](https://github.com/rcreecy/midnafx/actions/runs/39604108904) passes at measurement commit `d588304`: Windows **19/19** in 33.55 seconds and macOS Intel **19/19** in 29.23 seconds. Three consecutive Metal AA checks pass in 18.45, 14.85 and 12.45 seconds on the Apple Paravirtual adapter. Both ordinary native package artifacts are produced; the release job is skipped. CI validates the analyzer and recorded metadata on both platforms, but does not repeat these Windows process measurements, compile the experimental host patch, or exercise native macOS gameplay.

For another experiment, reproduce the explicit settings and balanced order, verify each intended workload from its logs, and collect the counters as described above. Compile the layout probe twice using the native host's include paths, defines and compiler flags, once with the draw-trace patch absent and once present. Preserve both binaries and hashes; restore the experimental checkout and executable afterward.

Next: diagnose the missing initial discovery, then repeat with verified populated identity records and 32/32 encoded labels per enabled trial. Measure allocation counts/bytes and frame timing separately, including active resampling and reloads. Experimental Metal-host compilation and native macOS gameplay also remain open. The existing package CI does not compile this experimental host patch.
