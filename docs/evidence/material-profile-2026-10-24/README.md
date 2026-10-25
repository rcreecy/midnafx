# Warm pumpkin material evidence, 2026-10-24

The candidate changes the selected pumpkin body's color in both original-texture
and external-replacement sessions. The sampled body returns exactly to its prior
decoded RGB values when disabled, or repeats exactly when re-enabled. Sampled
leaves, HUD hearts, rock, architecture and ground remain unchanged. These results
qualify this static material and these rectangles, not an entire scene or a
general remaster rendering system.

Both trials use Windows/D3D12, Intel UHD Graphics 630, stage `F_SP103,0,27,0`,
anti-aliasing enabled, both asset diagnostic switches disabled and the separate
body-dimming proof disabled. The images were captured on October 24 Pacific time;
the launch manifests record October 25 UTC. Their retained dimensions are
**1800 x 1022**. The sanitized manifests describe the initial 960 x 720 window
configuration, not the later captured dimensions.

## Package and scope

The package used for both candidate sequences is
`e0f62c1a95b3d3dfe35836c6ae821ba5fc871298b18c4e578a2c29064a51f80e`.
The host is
`444d8ee33ed78ba4a427810e35a62731f4d845cbac7343b37b16a5c20f3c34a4`.
The later package containing the logging fix has separate reload/departure and
archive-roundtrip evidence below. These candidate captures must not be relabeled
as that package's runtime observations.

The profile writes material color zero to `(255,240,224,255)` around a qualified
body draw and restores its known white definition afterward. It preserves the
existing register-sourced lighting response and texture lookup. This does not
establish arbitrary live-state restoration, other-mod compatibility, normal
mapping, specular rendering, PBR, per-actor-instance isolation or GPU performance.

| Retained trial | Sequence | Body mean absolute RGB change, 0-255 | Body maximum channel change | First/third body maximum change |
|---|---|---:|---:|---:|
| [candidate-original](candidate-original/summary.json) | Off, on, off | 2.437796771 | 16 | 0 |
| [candidate-pack](candidate-pack/summary.json) | On, off, on | 2.599754902 | 11 | 0 |

For every comparison above, all five exclusion rectangles have zero mean and
maximum channel difference. Their selection is explicit in each `roi-manifest.json`:
leaf `(620,480,30,20)`, HUD hearts `(112,80,120,40)`, rock `(1380,590,120,85)`,
building `(870,90,130,80)` and ground `(1120,440,100,50)`. The original body
rectangle is `(505,400,45,39)`; the replacement body rectangle is
`(510,405,40,34)`. Coordinates are image pixels, with origin at the upper left.

The six JPEG files are copied without modification. `captures.json` records their
SHA256 values, checked against the ignored originals. `pixel-regions.json` records
decoded RGB comparisons without resizing or image registration. Whole-frame
equality is not claimed: moving scene elements, interface/cursor state and other
dynamic pixels can differ. JPEG differences are observations of the decoded
captures, not a lossless framebuffer readback or a motion-quality test.

## Binding and close observations

The original-texture process starts with the profile disabled. Enabling it binds
the already loaded original model through the cached-source path. Disabling it
leaves **7,056 applies and 7,056 restores**, with zero rejections, through shutdown.

The replacement process starts with the profile enabled and binds during the
native resource load. Its on/off/on sequence reaches **17,334 applies and 17,334
restores**, with zero rejections, through shutdown. Its initial `patched-off`
launcher variant refers to asset diagnostics, not to the warm-profile switch.

Selected `control.log` files retain binding/toggle/shutdown events, `all mods
unloaded`, Discord shutdown and graphics-device destruction. Process exit codes
were not retained. The usual destruction-time mapping/device warnings and the
isolated session's missing `gczelda2` memory-card file are preserved; neither is
silently removed from the evidence.

The external replacement directory contains the two previously qualified DDS
files. Their relative filenames and SHA256 values are retained in the replacement
trial's summary; no texture payload is included. This trial does not independently
retrace resolved provider selection. It observes the existing replacement-enabled
path with the profile active.

## Bounded Frida command observation

The original trial retains the external-service `rea-evidence.json`. Its summary
observes **120 warm commands, 120 white-restore commands, zero other matching
commands and zero read faults** at the exported `GXCallDisplayList` boundary.
The probe source is `tools/qualification/material_color_probe.js`, SHA256
`8c57060374e28539b90b4df4a228ad7e95c3e9183534a7aa0f08b59e2c6b5aef`.

The filter counts only 32-byte display-list calls beginning with the selected
one-word XF `0x100C` transfer header. It does not attribute all host commands or
prove GPU completion. The external record declares unavailable artifact and
OS/architecture identity; host/package manifests provide separate session context.
Its retained state is `running`: the 1,200 ms summary timer does not itself detach
the interceptor. The parent observed a later absent session, but that follow-up
is not contained in this retained capture and is not used as detachment proof here.

## Final package reload and departure

[final-reload/summary.json](final-reload/summary.json) retains the later
`warm-final-r1` trial using package
`01582539f1f2a8dd1fec367e0680974de3a975521a4c1e17a44393e804120d2a`.
The observed loaded DLL has SHA256
`e48920f2aef5726d6f1bc3dbee7ebecffc3291e2ad59467e99daadfd565faeaf`,
matching the fixed Windows member in the separately retained
[static artifact manifest](../rea-artifacts-2026-10-24/manifest.json).
The host hash matches the candidate trials above. The sanitized
`runtime-identity.json` retains the observation timestamp and module hash; the
session manifest omits the disc path, arguments, PID and ASLR address.

Native mod reload shuts down the old module with **8,112 applies and 8,112
restores**, zero rejections, then binds the already loaded pumpkin through the
cached-source path. Departing Ordon (`F_SP103`) for Fishing Pond (`F_SP127`)
invalidates the pumpkin binding through `archive-delete` at **19,602 applies
and 19,602 restores**, zero rejections. Final shutdown remains unbound at those
same counts. This trial does **not** observe a return to Ordon or a subsequent
archive-load rebind. Selected log events, source line numbers and the complete
source-log hash are retained.

`before-reload.jpg` and `after-reload.jpg` are unchanged copies with hashes and
actual **1800 x 1022** dimensions in `captures.json`. Scene motion prevents a
matched-pixel comparison. These images are reload context, not another exact
restoration measurement.

The unchanged `rea-evidence.json` contains **two distinct records**. The first,
`ev_0b1a0e13cc3668bb8a3d43407e87e65c7e504f276a7601213255ceff5fe38597`,
is the prior candidate-original observation of 120 warm/120 restore commands.
The second,
`ev_9e8fe6cbbf18e7bcbad92ee5d0ef78510151d610a4bde82c6318fd44e7f5c93d`,
belongs to the final package after native reload: **114 warm, 114 white-restore,
zero other matching commands and zero read faults**. Both use the probe and
bounded command filter described above. The second record's state is `running`;
the separate unchanged `rea-cleanup.json` later reports no open session
`f0737f0a-eefa-427a-88ff-b211b3791d4d`. That receipt confirms the session is absent
at the check, without adding rendering or GPU-completion evidence.

The user closed the trial. The log records orderly mod unload, Discord shutdown
and graphics-device destruction; the process exit code is unknown. Retained
stderr includes the isolated session's missing `gczelda2` memory-card file.
No full-scene pixel, general other-mod restoration, GPU timing or macOS runtime
claim follows from this trial.

## Final package archive roundtrip, 2026-10-25

[final-roundtrip/summary.json](final-roundtrip/summary.json) retains the separate
`warm-roundtrip-r1` trial performed on **October 25, 2026, Pacific time**. This
ledger keeps its original October 24 directory date. The trial uses the same
final package
`01582539f1f2a8dd1fec367e0680974de3a975521a4c1e17a44393e804120d2a`
and host
`444d8ee33ed78ba4a427810e35a62731f4d845cbac7343b37b16a5c20f3c34a4`.
Both asset diagnostics and the dimming proof are disabled; the warm profile,
anti-aliasing and external texture replacements are enabled. The launcher's
`patched-off` variant again refers to asset diagnostics, not the warm profile.

The native route is Ordon `F_SP103,0,27,0`, Fishing Pond `F_SP127,0,0,0`, then
Ordon `F_SP103,0,27,0`. The source log records `archive-delete` at line 1,956,
clearing the binding with **15,216 applies and 15,216 restores**, zero rejections.
On return, another `pumpkin.arc`/`pumpkin.bmd` resource load precedes the fresh
`bound-pumpkin-body` event at line 3,070. This is a native-load rebind, distinct
from the previous trial's cached rebind after mod reload. Shutdown at line 4,239
remains bound with **21,480 applies and 21,480 restores**, zero rejections:
**6,264 additional balanced pairs after return**. `all mods unloaded` follows at
line 4,240. The selected `control.log` and source-log hash retain this sequence.

The process handle was acquired before closing with Alt+F4. The unchanged
`exit.json` records **exit code 0** and the owned process PID, with an observation
timestamp of `2026-10-25T15:07:39.9465305Z`. The log also records orderly graphics
destruction. The sanitized session removes disc paths, arguments and ASLR
addresses; the exit receipt deliberately retains its PID for process attribution.
The usual destruction warnings and missing `gczelda2` memory-card file remain
visible in the selected log and retained stderr.

`ordon-before.jpg`, `pond-away.jpg` and `ordon-return-bound.jpg` are unchanged
**1800 x 1022** copies. Their hashes are in `captures.json`. The return occurs
under different natural lighting, and scene elements move. These captures
document route context; no matched-image or exact pixel-restoration claim is
made. This trial uses **no Frida instrumentation** and adds no GPU timing,
general material coverage or macOS gameplay claim.

## Reproduction

Use Python with Pillow. Recompute metadata from the unchanged retained images:

```text
python tools/qualification/material-profile-regions.py --manifest docs/evidence/material-profile-2026-10-24/candidate-original/roi-manifest.json --output build/material-profile/original-regions-reproduced.json
python tools/qualification/material-profile-regions.py --manifest docs/evidence/material-profile-2026-10-24/candidate-pack/roi-manifest.json --output build/material-profile/pack-regions-reproduced.json
```

The tool writes metadata only and refuses to overwrite an input image or manifest.
Input file hashes and rectangle statistics must match the retained reports.
No native macOS/Metal gameplay, broad material corpus, allocator cost, frame
timing or GPU timing result follows from this candidate evidence.
