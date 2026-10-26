# Scene-wide adaptive exposure

The new effect has no resource, material, model or stage allowlist. One shared
meter/composite path changes scenery and characters together. It uses no new host
patches or asset metadata. See [the implementation contract](../../adaptive-exposure.md).

## Native observations

Windows D3D12, 960×720, qualified baseline host, stage `F_SP103,0,27,0`, autosave
off and isolated user data. Two externally supplied high-resolution DDS samples
are registered from `build/asset-hot-reload-pack/A-first`; this is a limited
replacement fixture, not the complete 4K pack. Its original lookup names are
`tex1_64x64_000e342106676e31_14.dds` and `tex1_64x64_630d5c7bc606585d_14.dds`.
No texture payload is included in this evidence directory or package.

`day-off.jpg` / `day-on.jpg` and `night-off.jpg` / `night-on.jpg` keep the same
camera and frozen simulation within each pair. Time is changed to native midnight
with `time 0`, then simulation runs before the night pair is frozen. Grading,
camera changes, AO and depth atmosphere are off for these pairs; AA and enhanced
water stay on. Only adaptive exposure changes. Render-time water/cursor/HUD
animations can still change, so these JPEG captures are not exact RGBA parity tests.

Both pairs show changes across rock, grass and Link rather than one supported
object. Mean RGB code values in fixed rectangles are:

| Region (x,y,width,height) | Day off / on | Night off / on |
|---|---|---|
| Rock (535,160,110,55) | 88.43 / 94.77 | 53.81 / 61.56 |
| Grass (590,360,150,130) | 57.59 / 62.81 | 50.47 / 57.90 |
| Link (434,321,94,220) | 81.19 / 86.80 | 58.91 / 67.07 |

These JPEG measurements demonstrate a broad visible change; they are not linear
light measurements or a full art-quality assessment. Night retains its darker,
cooler presentation. HUD remains downstream of the effect; translucent HUD
backgrounds can show the changed scene underneath.

The comparison candidate package SHA-256 is
`A9C000E2627803C16CC21579C163EA21538E3E155AC0CC51954BD7DF1AD62D34`.
`native-scenes.log` records 7,566 encoded metering dispatches and normal shutdown.
A reload request made at simulation rate zero remained queued, so it does not
establish reload success. The comparison host exited with code zero.

The final local v1.5.0 package SHA-256 is
`E8538E4B4E36EEAE558466EC300DA10C1F00479DA2C93A99A04A15CA7DB84D3D`.
It changes package metadata after the candidate's rendering checks. A new normal
30 Hz session selects the persisted Remaster settings, executes a real module
reload, and continues rendering. `native-final.log` records 1,439 dispatches before
reload and 886 after, followed by `all mods unloaded`; process exit code is zero.
`final-remaster.jpg` is the post-reload gameplay capture, not an A/B comparison.
The isolated empty memory card produces `Failed to open file: gczelda2`; no
WebGPU error is reported in the final run.

Host SHA-256:
`444D8EE33ED78BA4A427810E35A62731F4D845CBAC7343B37B16A5C20F3C34A4`.
These local artifact hashes differ from independently built CI packages.

An initial attempt used a stale `.dusk` because building the DLL target alone
does not run package creation. It exited on a reserved WGSL identifier already
fixed in the source. The full ALL build regenerated the package before the
successful comparisons; that stale attempt supplies no runtime qualification.

## Automated checks and limits

All 22 Windows CTest entries pass across the full run and the focused correction
rerun. The new GPU checks execute 316 ordered metering steps with a single final
readback: bounded convergence, darker night, asymmetric adaptation, reset,
Rec.709 colored-light metering, alpha independence, black bars, fades and recovery.
Sixteen active-exposure composite cases cover all eight ordinary/water/detail/AA
entry points, including LUT combinations. Neutral regression pixels remain exact.
The first new fixture expectations mishandled signed half values and water
composition order; correcting those test-only errors passes the focused rerun.
Package version checks pass again after the 1.5.0 metadata change.

Source review repaired reset tracking to use the current room rather than the
entry room, and added consumed WebGPU error scopes so optional metering can
retain neutral exposure on initialization failure.

Native dungeons, combat/cutscene transitions, room transitions and GPU duration
are not visually/performance qualified here. Fixed-grid highlight sampling and
global LDR correction have the limits described in the feature document. Intel
macOS gameplay/Metal remain unverified; only package and portable CI are required.
