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
nor a MidnaFX regression. A complete successful archive departure/return is pending.

`post-load-source-proof.json` records the noninvasive read metadata and
`post-load-source-reproduced.json` records the independent endian simulation.
Reproduce locally without distributing the game asset:

```powershell
python tools/qualification/material-source-normalization.py --archive build/runtime-assets/files/res/Object/pumpkin.arc --post-load-source build/material-control/lifecycle-r1-pond/post-load-source.bin
```

The output is metadata only. The raw source snapshot and original assets are not retained here.

The final Windows package build and all 21 CTest entries pass, including D3D12
pixels, the exhaustive fingerprint-padding test and package validation. This does
not substitute for the pending successful archive round trip or broader gameplay coverage.
