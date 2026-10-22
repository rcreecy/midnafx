# Bounded CPU frame timing qualification

The opt-in `patches/aurora-frame-timing.patch` adds telemetry only to pinned
Aurora's `lib/gfx/frame.cpp`. It is a separate native host experiment, not part
of the MidnaFX package. Check/apply/reverse it with
`python tools/frame-timing-host-patch.py`, adding `--apply` or `--reverse`.
The helper checks Aurora revision `7f2801cd0133c9333eadb4e2e6b24100c328d328`
and preserves unrelated local changes. CI checks applicability on both package
platforms; native execution is qualified on this Windows host only.

## Measurement contract

Set `MIDNAFX_FRAME_TIMING_FILE` to an output file before launching the rebuilt
host. The isolated `tools/asset-overhead-launch.ps1` launcher supports
`-FrameTiming`, records that request in session metadata, scopes the environment
to its child host, and writes to the trial's `frame-timing.json` on normal
graphics shutdown. An ordinary trial explicitly clears the inherited timing
request for its child. Never claim a capture exists merely from that request;
check the emitted file and its integrity.

Two independent, preallocated arrays retain at most 16,384 records each:

- Producer: frame ID and timestamps for begin entry, acquisition complete,
  recording start, end entry and end enqueue complete.
- Render worker: frame ID, successful `Surface.Present` completion timestamp
  and interval since the previous successful completion.

Recording adds no per-frame logging, file I/O or dynamic allocation. Each stream
has one writer. The path is read once before worker initialization; output is
flushed after worker synchronization and join. Counts include retained/dropped
records, failed begins, incomplete begins and an explicit truncation flag.
`begin_attempts` covers `gfx::begin_frame`, excluding earlier outer-loop returns
for pause or an unpresentable window. A failed begin receives no fabricated row.

All timestamps are CPU monotonic **wall time**. Producer spans include waits
and scheduling. Present completion is neither physical scanout nor display
latency. Worker completion may precede the producer's trailing enqueue timestamp;
the analyzer must not impose a false cross-thread ordering. Encoding time, GPU
time and actual heap allocation counts remain unavailable/null.

## Analyzer and smoke evidence

```powershell
python tools/frame-timing.py --input build/frame-timing/TRIAL/frame-timing.json --output build/frame-timing/TRIAL/summary.json
```

The default selects 30 seconds after a 20-second warmup relative to the first
producer begin. The complete requested window must exist. The analyzer rejects
truncated sources, malformed count equations, reordered streams, inconsistent
present intervals and insufficient coverage. It computes count/min/max and
linearly interpolated p50/p95/p99 for acquisition wall time, recording wall
time, end/enqueue wall time, total producer span, consecutive begin intervals
and successful present-completion intervals. Both interval endpoints must lie
within the selected window. Failed/incomplete begins are capture-wide counters;
their phase cannot be inferred.

Twelve analyzer tests pass, including legitimate asynchronous ordering, missing
successful presents, failed/incomplete begins and malformed evidence rejection.
Code review confirms single-writer storage and flush-after-join ordering.

The 2026-10-22 Windows/D3D12 smoke trial launches the expected scene, emits
**1,484 producer / 1,484 present records**, and closes normally. Counts show
1,484 attempts, zero failed/incomplete begins and zero dropped records.
The analyzer accepts its selected 2–7 second window. This short smoke proves
collection/analysis, not settled performance or observer overhead. Retained
source data, summary, session identity and capture hashes are under
[evidence/frame-timing-smoke-2026-10-22](evidence/frame-timing-smoke-2026-10-22).

## Remaining qualification

Build source-matched baseline/off/on hosts with identical timing storage and
instrumentation. Collect repeated settled, active sampling and replacement
reload windows; keep scene/settings/pack/backend/hardware and host-image identity
explicit. Do not compare these metrics with old uninstrumented process counters
as if they were the same measurement. No paired timing result is established
by this smoke trial.

Actual allocations still require an appropriate allocation capture with stacks.
The current shell is not elevated and no heap trace was started. Retained
metadata estimates and process private bytes cannot substitute for allocation
counts. Current native builds have `TRACY_ENABLE=OFF` and do not activate GPU
timestamp collection. GPU timing is unmeasured. None of these unavailable
measurements changes the best-effort macOS policy or creates a remote Metal gate.
