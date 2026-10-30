# Scene bloom

MidnaFX 1.7 adds restrained glow around bright scene pixels across scenery,
characters and external texture replacements. Select **Profile / Remaster** or
enable **Scene bloom** in Basic. **Bloom strength (%)** defaults to 20 and ranges
from 0 to 50. Vanilla disables bloom; existing installations keep their settings
until a profile is selected. Bloom works independently of manual grading.

The effect uses scene brightness, without asset hashes, material lists or stage
exceptions. It is an LDR highlight treatment, not emissive-material detection,
HDR rendering or a new light source. Bright clothing and other reflective-looking
surfaces can glow too. Tiny one-pixel highlights can disappear after filtering
and output quantization. Whole-game art and motion qualification remain open.

## Rendering contract

The existing scene snapshot feeds a soft highlight extraction before exposure
and manual grading. Extraction thresholds each source pixel before averaging
its 4×4 tile, including only valid pixels at odd-size edges. Two separable
nine-tap Gaussian filters build a quarter-resolution highlight core. The core
blur radius scales with scene height.

Version 1.19 adds a broader halo. Valid 2×2 averages build progressively smaller
RGBA16Float fields. Starting with the smallest level, a normalized nine-tap tent
filter reconstructs each larger field and blends equally with that level's
original field. Every merge is convex: a uniform highlight retains the same
response rather than gaining energy for every level. The original core remains
part of the final result. No temporal history or CPU readback is used, so a
previous frame cannot leave a bloom trail.

The pyramid stops when its height reaches 34 texels or five total levels have
been allocated. At 1080p the levels are 480×270, 240×135, 120×68 and 60×34.
4K adds a 960×540 base. Targets at or below 136 pixels high keep the original
single-level core. Odd dimensions use valid-texel averages, without repeated edge
texels. Clamp sampling prevents wraparound, but border highlights can retain
different integrated energy from interior highlights. Resolution changes can
change the level count and halo shape; cross-resolution parity is not claimed.

The existing grading/water draw samples this field once. Composition adds glow
in proportion to remaining highlight headroom, preserving the white endpoint
and input alpha. Adaptive exposure, manual grading and creative LUTs follow.
Enhanced water composes bloom after its absorption/refraction calculation; bloom
extraction uses the earlier scene snapshot, not a second enhanced-water draw.
Native HUD draws remain downstream. Diagnostic and forced passthrough modes
bypass bloom. Missing source, absent player, native pause or unavailable bloom
resources select a zero field. Other grading and water effects remain usable.

Bloom requires three core dispatches and two additional dispatches per coarser
level. Two eight-byte fields per level total approximately 2.63 MiB and nine
dispatches at 1080p, or 10.54 MiB and eleven dispatches at 4K. These figures exclude
temporarily retained targets during resize. All levels share the existing
compute callback and draw lease; resource retirement waits for their pending
callbacks. No extra fullscreen draw is added. Disabled bloom queues no bloom
compute; an active grading draw still samples the neutral field. Additional
filtering costs GPU work; GPU duration and 4K performance are not measured.

## Evidence and platform scope

Fourteen focused Windows GPU cases cover the original eight extraction,
composition and preservation checks, plus multi-scale strength bounds, a wider
visible halo at 20%, small-highlight motion through extraction phases, odd border
behavior, maximum-level narrow targets and linear strength scaling. Portable
planning checks cover boundaries, overflow-safe dimensions and memory bounds.
The [1.19 evidence ledger](evidence/multiscale-bloom-2026-10-30/README.md) retains
production rendering outputs and measured limits. Native desktop capture is
unavailable in this iteration; 1.19 native visual, reload and normal-close
qualification remain open.

Earlier native Ordon toggle, resize, reload and normal-close observations cover
the 1.7 implementation only. Sixteen earlier ordinary/water/detail/AA/LUT
combinations remain in `aa_pixels`. The [1.7 evidence ledger](evidence/bloom-2026-10-26/README.md)
also retains an unsuccessful Goron Mines startup whose cause is unresolved.
Those observations do not qualify the new multi-scale native path.

macOS remains best effort through native package builds and portable checks.
Gameplay and Metal execution are unverified. No remote Metal execution is
required. The package contains no game or third-party texture payloads.
