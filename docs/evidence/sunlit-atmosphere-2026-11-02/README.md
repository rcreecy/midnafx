# Sunlit depth atmosphere evidence, 2026-11-02

Version 1.21 tints the existing native-colored depth fog toward native sunlight
with a bounded directional lobe. See [the feature contract](../../depth-atmosphere.md)
for controls, source gates, composition and artistic limits.

## Source semantics

Source references use pinned Dusklight revision
`edf42c6a7202647b56dd2fcdef02d17671bc814b`.
`src/d/d_kankyo_rain.cpp:158–169` places the sun sprite 8,000 units from the
camera. The view transform therefore produces a full-sphere direction without
screen coordinates or a geometry far-plane gate.

`d_kankyo_rain.cpp:323–328` and `:394–395` ease native sun alpha with daytime and
rain availability. Projection/depth probes at `:334–360` instead feed sprite
visibility at `:411–420`. Directional haze deliberately uses daylight alpha, not
the lens visibility that vanishes when the sun leaves the screen or passes behind
local geometry. Native weather pattern and snow suppression at `:401–408` are
handled separately, alongside outdoor, skybox, Twilight, event, pause and water
gates. Native sun-core RGB remains the palette source; no authored sun schedule,
height plane, global memory mutation or asset-specific tuning is introduced.

## Automated proof

Five focused CTest entries pass in `focused-tests.log`: source preparation,
production shader compilation, preset persistence, ambient pixels and the
1.21.0 package contract. Source tests cover finite proper rigid transforms,
perspective guards, camera translation/rotation, full-sphere and off-screen
directions, alpha fading, color clamps and invalid-source fallback.

`gpu-final.log` retains 103 passing production D3D12 cases on Intel UHD Graphics
630. The previous 87 remain, with sixteen new checks at 65×49 and 33×25 targets.
The new cases cover the analytic forward cap, full-sphere response and symmetry,
continuous screen-edge crossing, daylight-alpha fading, exact original-fog
fallback, disabled/near/background bypass, reversed-Z and contact-darkening order.
Source alpha and original contact fields are preserved. Invalid non-unit or
non-finite direction/color data cannot introduce a new haze tint.

At the forward peak, the fog target is the midpoint of native fog and sun RGB
when daylight alpha is one. The existing 35% atmosphere setting still bounds the
final fog amount; its native distance curve does not change. An independent
analytic midpoint/composition bound checks that cap instead of reproducing the
whole directional lobe. Off-screen source crossings agree within two display
codes, and matched reversed-Z output agrees within one code in these fixtures.
These are synthetic bounds, not whole-game camera-motion or art qualification.

CPU, WGSL and fixture parameter blocks all occupy 496 bytes, including the two
appended vectors. The existing opaque snapshot, draw and target lifetime remain;
there is no additional texture, compute dispatch, fullscreen draw or history.
Additional per-fragment arithmetic is not a measured GPU cost.

## Native attempt and limits

The locally launched package SHA-256 is
`A95EF09DE16CA038BD4BBA73B7E61C9941728CD0F20CE0B8A65268EC70B2E0FD`.
The exact-name baseline host SHA-256 is
`444D8EE33ED78BA4A427810E35A62731F4D845CBAC7343B37B16A5C20F3C34A4`.
`session.json` retains launch identity, D3D12, Ordon F_SP103,0,27,0 and the
isolated initial configuration. Atmosphere alone is enabled at 35%; replacements
and autosave are off. No audit fixture is installed. This local binary is
distinct from release-CI packages.

Monitor capture fails with `IGraphicsCaptureItemInterop.CreateForMonitor failed:
Could not capture the given monitor. (0x80070057)`, including one fresh-window
recovery. Read-only session inventory reports RDP session 2 disconnected.
`capture-error.txt` retains the error. No subsequent UI input is attempted.
Native visual, toggle, reload and normal-close checks remain unverified.

`native.log` retains the attempted startup. The owned trial is forcibly stopped
for cleanup after its process path and start time are checked; `cleanup.json`
records that action and `exit.json` records -1. This is not an exit-zero result,
shader-execution proof or normal-unload qualification. Earlier native atmosphere
observations do not qualify this version's sunlit path.

The approximation provides no hidden/transparent atmospheric shadows or physical
transport. Weather/event gates may change discretely. Whole-game art/motion,
GPU duration, 4K cost and complete replacement-pack compatibility remain open.
macOS support stays best effort through native builds and portable checks;
gameplay and Metal execution are unavailable on this Windows host.
