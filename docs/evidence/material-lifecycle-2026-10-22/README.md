# Material lifecycle ledger, 2026-10-22

All trials use Windows/D3D12 and the two previously identified external DDS
replacements. Native fullscreen was used for the 1024 × 768 lifecycle screenshots.
These are lifecycle/static observations, not a representative gameplay-quality pass.
Each directory retains a sanitized initial session, control events and screenshot hashes.

| Trial | Observation | Qualification |
|---|---|---|
| `lifecycle-r1` | Original material binds in Ordon. Warp to `F_SP121,0,0,-1` clears the binding with 12,216 balanced applies/restores. Destination stays black; queued return is not consumed. Normal close succeeds. | Failed transition; no successful archive round trip. |
| `lifecycle-r1-pond` | `F_SP127,0,0,0` renders. Warp to `F_SP103,0,27,0` loads and binds the target. Original module reload loses binding; self-warp keeps the archive cached and does not repair it. | Reproduced cached-binding usability defect. |
| Same process, first repair candidate | Cached binding succeeds without another resource load. Paused `rate 0` off/on/off views show body-only change. Both diagnostic settings remain off. Normal close succeeds with 39,570 balanced applies/restores and zero rejected stages. | Draw/restoration proof for candidate package `c057c7a85213ebca797e3697bf20e0c46a1e406cf4ec2a92d2d265b618ffb752`. |
| `cached-normalized-r1` | Final package cold-loads and binds, then rebinds immediately after native mod reload. Non-tracing host, diagnostics off, replacements on. Normal close succeeds. Counts balance before reload (11,394) and afterward (5,802), with zero rejections. | Final normalized cached-source identity and mod-reload lifecycle proof. |
| `archive-roundtrip-r1` | Final package renders Ordon, leaves for `F_SP127,0,0,0`, and returns to `F_SP103,0,27,0`. Archive deletion clears the binding at 17,982 balanced applies/restores; return binds again at the same count. Shutdown reaches 515,310 balanced applies/restores, zero rejections and `all mods unloaded`. | Successful archive departure/return for this route. Three unchanged screenshots and selected stage/control logs are retained. The process exit code was not retained. |

The first candidate used a raw post-load fingerprint. Review found twelve
compiler-padding bytes whose value is unspecified. The final package uses a
normalized fingerprint; it is `cd656465b9a98e86191495d557fdd1cd908c921a6d6bf0c19b41ca1de199791b`.
Replacing the file and requesting another reload during `rate 0` did not run that
queued reload before closing the candidate process. Its screenshots and counts
must therefore not be relabeled as final-package evidence. The independent final
process supplies the final-package reload observation.

Candidate rectangles: left/center body mean absolute RGB change 25.007 / 13.917
on the 0–255 scale. Leaves, ground, architecture and player rectangles have zero
off/on difference. Every rectangle has zero off/restored difference. See
`lifecycle-r1-pond/pixel-regions.json` and the three `cached-*` screenshots.

The black `F_SP121` destination is unresolved. A matching noninvasive stack shows
the main thread rendering through the normal frame limiter and workers idle;
this is not the reproduced decompressor shutdown stall. Destination progression,
room/player creation and native overlap/fader state were not fully captured.
`mDoGph` fade fields alone do not exclude the separate `JUTFader`. No baseline
comparison establishes a cause, so this ledger attributes neither a native bug
nor a MidnaFX regression. The separate `archive-roundtrip-r1` trial completes
the known Fishing Pond route; it does not resolve the failed Hyrule Field route.

The [archive round-trip evidence](archive-roundtrip-r1/summary.json) records
package `cd656465b9a98e86191495d557fdd1cd908c921a6d6bf0c19b41ca1de199791b`
and host `444d8ee33ed78ba4a427810e35a62731f4d845cbac7343b37b16a5c20f3c34a4`.
Both diagnostic settings remain off, body control remains enabled and the existing
external replacements remain enabled. `control.log` retains the stage order
`F_SP103`, `F_SP127`, `F_SP103` and clear/rebind/shutdown events; only session
addresses are removed from the three selected stage lines. `session.json` is the
sanitized initial configuration, not a claim that later native fullscreen settings
match its initial 960 × 720 window. The three copied screenshots are 1024 × 768.
`captures.json` records their SHA256 values, verified against the ignored originals.
They show each scene rendering but do not provide a matched before/after pixel
comparison: time of day and other scene state change during the round trip.
Shutdown logs reach mod unload, Discord shutdown and graphics-device destruction;
the missing process exit code remains explicitly unknown.

`post-load-source-proof.json` records the noninvasive read metadata and
`post-load-source-reproduced.json` records the independent endian simulation.
Reproduce locally without distributing the game asset:

```powershell
python tools/qualification/material-source-normalization.py --archive build/runtime-assets/files/res/Object/pumpkin.arc --post-load-source build/material-control/lifecycle-r1-pond/post-load-source.bin
```

The output is metadata only. The raw source snapshot and original assets are not retained here.

The final Windows package build and all 21 CTest entries pass, including D3D12
pixels, the exhaustive fingerprint-padding test and package validation. This does
not substitute for broader gameplay, motion-quality or performance coverage.
The separate archive round trip supplies the previously missing route-specific
lifecycle observation; it does not complete the full R1 or remaster-quality goal.
