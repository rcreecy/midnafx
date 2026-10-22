# Windows observer launch identity correction

The missing asset discovery is caused by launching the native Windows host under an executable alias. The SDK import library names `dusklight.exe`. The preserved mod DLL also lists that exact dependency. Running `dusklight-asset-traced.exe` while `dusklight.exe` is present loads both executable images into one process. Restoring the original filename restores asset discovery and 32 matching encoded draws, using the same executable bytes and preserved mod package.

This invalidates the CPU and retained-memory comparison in the [original nine-run study](asset-observer-overhead.md). Its raw counters and arithmetic remain available for audit, but cannot establish observer overhead. In particular, an aliased baseline can resolve native imports to the separate primary host, defeating the intended build isolation. Those historical trials did not record module inventories. The standalone object-layout probes remain valid because they load no host or native mod.

## Cause and proof

The pinned SDK's `cmake/WindowsExports.cmake` generates its import library with `-D dusklight.exe` or `/name:dusklight.exe`. Inspecting the native DLL extracted from preserved package SHA-256 `15ec60f960a1277f4441fd26358ef13196534018b8be29ab77e25bb6ab9dbcb3` confirms `dusklight.exe` in its PE dependency table.

The paired reproduction uses experimental host SHA-256 `482aa1fcc8c4789297ba3b4d8476582a8e1868c33be578a982628ca35e8f3e8b`, that preserved package, the same explicit configuration, isolated user/mod directories, the same replacement root, and Ordon `F_SP103,0,27,0`. Both runs confirm mod initialization. Only the launch filename changes within the common runtime directory.

| Launch name | Loaded host images | Discovery events | Draw labels / encoded draws |
|---|---:|---:|---:|
| `dusklight-asset-traced.exe` | 2, same hash at distinct bases | 0 | 0 / 0 |
| `dusklight.exe` | 1, expected hash | 128 resource records | 32 / 32 |

The normal-name labels are unique and cover both pumpkin materials. Each matches an encoded `AssetDraw` label. The resource registry reaches its existing 128-record cap, so the result remains a bounded observation rather than a scene census. The module inventory directly proves duplicate mappings. The filename comparison establishes the discovery failure mechanism without a game-code change; it does not audit every affected import or hook dispatch.

[Summary, module inventories and sanitized sessions](evidence/asset-host-name-2026-10-21/summary.json), [normal-name metadata](evidence/asset-host-name-2026-10-21/normal-events.jsonl), and [alias metadata](evidence/asset-host-name-2026-10-21/alias-events.jsonl) are committed. The alias records only the host's independent provider events, with no mod discovery or draw labels. Original session/log hashes identify the local captures. No game, mod binary, or texture payload is committed.

## Harness repair

[The Windows launcher](../tools/asset-overhead-launch.ps1) rejects any host filename other than `dusklight.exe` before creating trial directories or starting a process. It requires runtime assets, creates isolated configuration and mod directories, waits for mod initialization, and records loaded host names, bases and hashes. It rejects multiple host images or a hash mismatch before reporting a ready trial. A readiness or module-check failure leaves its owned process and logs available for inspection; it does not authorize counter collection.

Prepare separate complete runtime directories for baseline and traced builds, preserving the name `dusklight.exe` in each. Include the runtime DLLs and `res` tree. Do not rename the executable to select a variant. The launcher accepts explicit host, package, disc, replacement-root and output-root paths; it does not modify the installed runtime or user settings. All trials must use the same package, explicit settings and replacement files.

The counter analyzer adds `--require-host-identity` for future profiling. That option rejects aliases, missing module inventories, duplicate host images and mismatched hashes. The default remains available for reproducing the historical counter arithmetic; it is not a qualification decision. Always use the strict option for a new Windows benchmark and separately verify the intended asset events from its logs.

Regression tests check actual Windows launcher rejection before any filesystem mutation, analyzer rejection of incorrect module evidence, and reconstruction of the paired metadata result. The Windows-only launcher test is skipped on macOS; portable identity and evidence tests still run there.

The local Windows Python suite passes **12/12**, including a strict CLI check with Windows UTF-8 BOM session files. The remaining 18 CTest entries also pass, including the AA pixel test in 38.92 seconds. The corrected evidence assertion retains the host's independent provider records; it rejects mod discovery and draw events in the alias case. Whitespace and experimental patch checks pass. The primary host executable keeps the original traced SHA-256 listed above.

Hosted [CI run 39703544999](https://github.com/rcreecy/midnafx/actions/runs/39703544999) passes at fix commit `14b3f63`: Windows **19/19** in 43.56 seconds and macOS Intel **19/19** in 27.80 seconds. Three consecutive Metal AA checks pass in 11.76, 12.88 and 12.08 seconds on the Apple Paravirtual adapter. Both ordinary native package artifacts are produced; the release job is skipped. The Windows launcher rejection case runs on Windows; macOS skips that case and runs the portable checks. CI does not reproduce the live module experiment, compile the experimental host patch, or qualify native macOS gameplay.

## Limits and next measurement

These paired bootstrap runs omit runtime fonts. Both report font-load failures, so they are import/discovery diagnostics, not graphical acceptance or performance measurements. A subsequent run with the complete resource tree also restores discovery; its forcibly stopped logs retain 29 encoded records and are not substituted for the paired 32/32 evidence. The repaired launcher now checks the runtime font prerequisite.

Desktop capture did not show the intended game window during this investigation. No further UI input was issued. The owned diagnostic processes were stopped after metadata collection, so these runs provide no normal-shutdown or complete final-log proof. Encoded draws do not establish GPU completion or visibility.

The next valid overhead study requires working target-window capture, complete runtime assets, one verified host image per process, populated identity records, and 32/32 matched labels in every enabled trial. Repeat the balanced baseline/off/on measurements, then measure active bursts, allocation rates and frame timing separately. Experimental Metal-host compilation and native macOS gameplay remain open. Both asset diagnostic defaults stay off; no release is published by this correction.
