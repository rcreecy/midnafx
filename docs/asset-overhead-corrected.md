# Corrected Windows observer process measurements

Nine new trials use complete runtime directories, the required `dusklight.exe` name, and exactly one matching host image per process. Each enabled trial discovers assets and records 32/32 matching pumpkin draw labels and encoded draws. This replaces the invalid process comparison described in the [launch-name correction](asset-host-name.md).

The result covers process CPU and retained memory after warm-up. It does **not** close the full observer overhead gate: allocation rates, frame/GPU timing, and active resample/reload costs remain unmeasured. The observed CPU ranges overlap substantially. No performance budget, FPS guarantee or default promotion follows from these results.

## Results

Entries are the median of three trials, followed by their minimum and maximum. CPU is accumulated process CPU milliseconds per wall second, across all threads; 1,000 ms/s represents one fully occupied logical processor. Memory values summarize the median within each trial. Ranges are observations, not statistical confidence intervals.

| Configuration | Process CPU, ms/s | Private committed memory, MiB | Working set, MiB |
|---|---:|---:|---:|
| Baseline, diagnostics off | 413.005 [391.331, 504.483] | 1583.832 [1581.582, 1595.313] | 1371.695 [1369.105, 1390.605] |
| Patched, diagnostics off | 380.499 [333.414, 436.195] | 1584.379 [1583.863, 1597.770] | 1370.047 [1368.633, 1390.555] |
| Patched, diagnostics on | 443.209 [348.619, 455.834] | 1585.098 [1584.348, 1585.395] | 1370.133 [1369.977, 1370.758] |

Enabled diagnostics have a 62.710 ms/s higher group CPU median than patched-off, or 16.48% of that process CPU median. Private memory is 0.719 MiB higher and working set is 0.086 MiB higher. These are observed differences on one system, with broad variation, not isolated causal estimates or frame costs.

Patched-off has a 32.506 ms/s lower CPU median than baseline and 0.547 MiB more private memory. The negative CPU difference does not establish a speedup or zero host-patch overhead. Scheduling, dynamic scene behavior, shared cache state and incidental compiler output remain uncontrolled. No trial is discarded. Private committed memory describes retained memory rather than allocation counts or total allocated bytes; working set includes shared resident pages.

The standalone [native layout probe](asset-observer-overhead.md#native-object-layout) remains valid: `gfx::detail::Command` grows from 152 to 208 bytes, independently of the mod diagnostic switches. These object sizes do not quantify total frame allocations.

## Controls and workload

The trial order is `b1, o1, e1, e2, b2, o2, o3, e3, b3`, where each variant occupies each position within a three-run block once. Baseline and traced executables live in separate complete runtime directories, both named `dusklight.exe`. Runtime DLLs and resources are copied from the same runtime. The existing launcher records module identity after mod initialization; the new [sampler](../tools/asset-overhead-sample.ps1) verifies it again before warm-up and after measurement.

All trials use preserved package SHA-256 `15ec60f960a1277f4441fd26358ef13196534018b8be29ab77e25bb6ab9dbcb3`, stage `F_SP103,0,27,0`, the same 960×720 client window, VSync on, AA on, texture replacements on, and the same two verified pumpkin DDS files under `A-first`. Only the two asset diagnostic flags vary between off/on trials. Other explicit configuration matches; unspecified settings use the same package/host defaults. The baseline/traced hashes and SDK pins match the earlier documented builds. Shared native caches are not reset between trials.

The machine uses an Intel Core i7-10700 with 16 logical processors, Windows 11 Pro build 26200, and Intel UHD Graphics 630, driver 31.0.101.2140. Every host log confirms the Intel D3D12 adapter. Collection occurs in an active RDP desktop session. Hardware metadata is captured again for this experiment.

The intended game window is activated and visually checked before each collection. After 20 seconds of warm-up, the sampler records 31 observations across 30 approximately one-second intervals. No UI input occurs during sampling. The game is visually checked again afterward. Startup pipeline-building notices disappear by the final captures. Idle animations and NPC positions vary; this is not a deterministic frame replay. Screenshots stay local.

Every enabled run records 128 resource identities, including the pumpkin, and both pumpkin materials have 32 total unique labels matching 32 native encoded draws. The existing discovery cap prevents a whole-scene census. Off runs have no mod asset events or encoded asset labels. The enabled measurement retains a populated registry after the initial bounded sample burst; it does not isolate discovery, startup sampling, manual resampling, or reload costs. Encoded draws do not prove GPU completion or visibility.

## Shutdown observation

Eight trials close normally. Baseline trial `b2` completes its measurement and confirms mod unload after a normal close request, but the process does not exit after sequential five-second and fifteen-second waits. The agent subsequently terminates only that owned baseline process. Its counters remain in the comparison because sampling finishes before shutdown begins. The termination note is preserved and asserted by the evidence test.

The shutdown cause is unresolved; an earlier suggestion that cache saving explained the delay was not established. This failure occurs with the unpatched baseline and diagnostics off, but that alone does not diagnose it or establish stability for the other configurations. It must remain separate from any claim that all runtime acceptance checks pass.

All runs report the common missing `gczelda2` memory-card file in their fresh user directories. No stdout error or fatal record is present. Normal shutdowns report the previously observed buffer-mapping abort and device destruction messages. There are no missing-font startup failures in this corrected experiment.

## Evidence and validation

[Raw counters and sanitized sessions](evidence/asset-overhead-corrected-2026-10-21/runs.json), [summary](evidence/asset-overhead-corrected-2026-10-21/summary.json), [metadata events](evidence/asset-overhead-corrected-2026-10-21/metadata-events.jsonl), and [hardware](evidence/asset-overhead-corrected-2026-10-21/hardware.json) contain metadata only. Original log/session/capture/screenshot hashes identify local files under `build/asset-overhead-v2`. Published sessions redact the common disc path and omit process IDs and launch arguments.

The analyzer's `--require-host-identity` option now checks the session inventory, both sampler inventories and explicit visual-check acknowledgement. It rejects missing or duplicate host images and hash mismatches. The sampler refuses collection without visual acknowledgement, verifies the live process, and writes UTF-8 counters. Fourteen Python cases cover arithmetic, evidence rejection, Windows launch/sampling guards, BOM input, and exact reconstruction of both historical and corrected results. The two Windows script checks are skipped on macOS.

Local Windows CTest passes **19/19** in 53.95 seconds, including all 14 analyzer cases and the existing asset-intelligence tests. The strict CLI reproduces the analysis from all nine original run directories. Both experimental patch applicability checks and whitespace checks pass. No game or renderer source changes in this milestone.

Hosted [CI run 39706492998](https://github.com/rcreecy/midnafx/actions/runs/39706492998) passes at measurement commit `192ecd1`: Windows **19/19** in 31.55 seconds and macOS Intel **19/19** in 41.73 seconds. Three consecutive Metal AA checks pass in 16.81, 17.29 and 18.02 seconds on the Apple Paravirtual adapter. Both ordinary native package artifacts are produced; the release job is skipped. The two Windows script guard cases run on Windows and are skipped on macOS. CI validates recorded counters and metadata; it does not repeat the live process experiment, diagnose the shutdown stall, compile the experimental host patch, or perform the user's macOS gameplay check.

To reproduce, prepare complete baseline and traced runtimes in separate directories with the original executable name. Use [the launcher](../tools/asset-overhead-launch.ps1) for fresh user/mod directories and the settings above. Visually activate/check the game, then invoke the sampler with the run directory and `-VisualCheckConfirmed`. Check the final scene, close the owned host, preserve any termination failure, and run the analyzer with `--require-host-identity` on all nine directories. Inspect companion logs for the expected populated workload; module checks alone do not establish it.

## Remaining work and macOS acceptance

Next measurements must capture allocation counts/bytes, frame timing, active sampling and reload costs. The baseline shutdown stall also needs diagnosis. Experimental Metal-host compilation remains a development check separate from ordinary package CI.

The user owns native macOS gameplay testing separately and chooses successful launch/gameplay without a crash as the minimum acceptance criterion. That result remains pending; it will not be represented as pixel, performance or broad material-control qualification. Hosted package/Metal checks continue. Both asset diagnostics remain disabled by default; no release or visual behavior changes in this measurement milestone.
