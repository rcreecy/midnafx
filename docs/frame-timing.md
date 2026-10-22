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

## Settled comparison: 2026-10-22

Eight valid captures compare four conditions twice, reversing the order for the
second set. All use the same final mod package, D3D12, configured 960 × 720 client,
VSync, default 30 Hz simulation, Ordon point 27/layer 0 and the two external DDS
replacements. Hardware is Intel Core i7-10700 / UHD 630. Both hosts contain the
same CPU timing and decompressor repair patches; only the traced host includes
the draw-trace patch. Host hashes and settings are retained per trial.

Every selected window is 20–50 seconds after first producer begin. No build/test
or menu input runs during those windows. The live idle scene is matched, but it
is not a deterministic replay. All eight captures have zero failed/incomplete
begins, zero dropped records, no truncation and a normal close.

Successful present-call completion intervals, range across the two trials (ms):

| Condition | p50 | p95 | p99 |
|---|---:|---:|---:|
| Non-tracing host, diagnostics/body control off | 33.238–33.279 | 34.718–35.220 | 39.443–45.495 |
| Tracing host, diagnostics/body control off | 33.215–33.278 | 34.959–35.276 | 40.682–45.125 |
| Tracing host, diagnostics on, body control off | 33.268–33.296 | 34.704–34.762 | 36.313–44.646 |
| Non-tracing host, diagnostics off, body control on | 33.084–33.224 | 35.194–41.744 | 45.802–45.906 |

Total producer-span medians range from 33.167 to 33.205 ms across all trials.
These spans include native pacing and waits. Similar medians at the 30 Hz cap
do not establish zero execution/GPU overhead. Material control has a worse p95
in one repeat; tail spikes also increase in several later controls. Two trials
do not establish cause or bound worst-case cost. Retain the result, including
that tail, rather than claiming a performance pass beyond the measured scene.

The diagnostics-on windows are settled: their initial bounded 32 draw labels
finish before measurement. They are not active-resampling windows. Body-control
trials retain balanced apply/restore totals and zero rejected stages. Full source
captures, analysis, screenshots, hashes, control logs and comparison metadata:
[frame-timing-settled-2026-10-22](evidence/frame-timing-settled-2026-10-22).

## Remaining qualification

Collect active sampling and replacement-reload windows; broaden representative
scenes and investigate any repeatable material tail regression. Keep
scene/settings/pack/backend/hardware and host-image identity explicit. Do not
compare these metrics with old uninstrumented process counters as if they were
the same measurement. The settled comparison does not replace those phases.

Actual allocations still require an appropriate allocation capture with stacks.
The current shell is not elevated and no heap trace was started. Retained
metadata estimates and process private bytes cannot substitute for allocation
counts. Current native builds have `TRACY_ENABLE=OFF` and do not activate GPU
timestamp collection. GPU timing is unmeasured. None of these unavailable
measurements changes the best-effort macOS policy or creates a remote Metal gate.
