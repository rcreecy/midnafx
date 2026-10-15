# GPU process-memory probe

## Windows D3D12, 2026-10-15

The isolated `build/m13-aa-runtime/lut-memory-reload` run uses the unchanged
package `ABEE0EB4F0DE421A6AC8D0FA6CF6D0D497860800B7DBE724B331F03C14046AA6`
on Intel UHD Graphics 630. Fishing Pond (`F_SP127,0,0,0`) runs with AA,
Natural at 100%, enhanced water, DOF, and detail at 20%. The native Mods panel
remains open during baseline sampling, three reloads, and the idle control.
The game clock is running. No installed user settings are modified.

Windows `GPU Process Memory` counters are available. Samples filter the exact
host PID, retain each adapter instance and status, and sum **Total Committed**
across those instances per timestamp. Shared and local usage overlap on this
integrated adapter and are not added together. Dedicated usage is zero. These
are whole-host GPU process counters, not MidnaFX-only allocations or process RSS.

Each baseline/reload phase contains five one-second samples. The subsequent
idle control contains 30 one-second samples without another reload. All retained
counter statuses are zero (valid).

| Phase | Minimum committed bytes | Maximum committed bytes |
|---|---:|---:|
| Baseline | 760,918,016 | 761,499,648 |
| Reload 1 | 787,243,008 | 787,648,512 |
| Reload 2 | 789,348,352 | 789,348,352 |
| Reload 3 | 790,306,816 | 790,376,448 |
| Idle control | 790,892,544 | 792,252,416 |

The first reload accounts for most of the roughly 28 MiB increase. Growth also
continues during the idle control (about 1.3 MiB from its first to last sample).
This does not establish stable memory usage or isolate a reload leak. Scene
activity, host/driver caches, and allocation retention remain possible causes.
A matched no-reload run after a longer warmup and longer repeated cycles are
needed before attributing the growth or changing ownership code.

All three reloads are confirmed in the log. Each prior instance reports one
cached LUT layout and zero water-capture failures. The final instance reports
1,215 look draws, one cached layout, 1,216 water captures, zero capture failures,
and all mods unloaded after normal Alt+F4 exit. The controls still show the
selected effects after all reloads. This passes repeated reload functionality,
but the memory stability gate remains open.

Raw evidence: `counter-probe.json`, `baseline.json`, `reload-1.json` through
`reload-3.json`, `idle-control.json`, `summary.json`, the stdout/stderr logs,
and `after-three-reloads.jpg` in the run directory. No product code or release
assets changed.
