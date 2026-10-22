# Decompressor shutdown repair

Qualification date: 2026-10-21. Pinned Dusklight revision:
`edf42c6a7202647b56dd2fcdef02d17671bc814b`.

`JKRDecomp::destroy()` originally sent its null shutdown sentinel with
`OS_MESSAGE_NOBLOCK`, ignored the return value, and immediately joined the
worker. When all eight queue slots were occupied, the sentinel was rejected.
The worker drained the commands and waited for another message forever; the
joining thread could not finish. The repair waits for queue space with
`OS_MESSAGE_BLOCK`. The worker remains live until it receives that sentinel.

`tools/qualification/jkr_decomp_shutdown.cpp` compiles the actual, complete
upstream `JKRDecomp.cpp` translation unit. Controlled OS queue/thread functions
hold the real worker before its first receive, fill the eight-slot queue, then
release it when shutdown starts. A bounded recovery sentinel prevents the
unrepaired test from leaving a hanging process. The test also exercises an
empty queue. It does not replace the decompressor's create, destroy, run or
decode implementation.

| Source | Empty queue | Full queue | Recovery required |
|---|---|---|---|
| Original | Pass, 20/20 repeats | Fail, 20/20 repeats | Full case only |
| Repaired | Pass, 20/20 repeats | Pass, 20/20 repeats | No |

The full case processes all eight commands in both versions. The repaired
version accepts the sentinel and exits normally. Evidence contains source and
harness SHA256 values, case results, repetition results and patch pin checks
under [evidence/decomp-shutdown-2026-10-21](evidence/decomp-shutdown-2026-10-21).
The original full-queue repeat count was observed in the terminal (20 exit-1
results); its individual repeat output was not saved. Durable original evidence
contains one full-queue failure and 20 empty-queue controls. The repaired
evidence contains all 40 repeat results. Do not represent the original full
repeats as a retained per-run artifact.

Run on this Windows development host, after configuring the native host:

```powershell
python tools/decomp-shutdown-host-patch.py --apply
python tools/jkr-decomp-shutdown-test.py --output build/remaster-r0/decomp-shutdown-fixed
```

The driver reuses the native host project's include paths and definitions and
the configured Visual Studio 18 Community toolchain. An unrepaired full queue
returns failure after bounded recovery. Compilation and case logs remain in
the requested output directory. The patch helper checks the exact source pin,
supports idempotent apply/reverse, and preserves unrelated local patches.

This is a demonstrated shutdown defect under controlled scheduling. It does
**not** identify the cause of the historical corrected-overhead `b2` stall:
that process was terminated before a matching blocked-thread stack was
captured. The last buffered `all mods unloaded` line cannot distinguish
decompression, worker shutdown, queue cleanup or Discord IPC. Preserve that
uncertainty. If another normal-close stall occurs, capture matching native
thread stacks before terminating the owned test process.

The patch is not part of the MidnaFX `.dusk` file. A native host rebuild and
separate runtime qualification are required. Portable patch applicability on
macOS does not establish macOS runtime shutdown verification.
