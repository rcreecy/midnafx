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
nine-tap Gaussian filters operate on quarter-resolution RGBA16Float targets.
The blur radius scales with scene height. No temporal history or CPU readback is
used, so a previous frame cannot leave a bloom trail.

The existing grading/water draw samples this field once. Composition adds glow
in proportion to remaining highlight headroom, preserving the white endpoint
and input alpha. Adaptive exposure, manual grading and creative LUTs follow.
Enhanced water composes bloom after its absorption/refraction calculation; bloom
extraction uses the earlier scene snapshot, not a second enhanced-water draw.
Native HUD draws remain downstream. Diagnostic and forced passthrough modes
bypass bloom. Missing source, absent player, native pause or unavailable bloom
resources select a zero field. Other grading and water effects remain usable.

Bloom requires three compute dispatches and two quarter-resolution eight-byte
fields: approximately 2 MiB at 1080p and 8 MiB at 4K, excluding temporarily
retained targets during resize. Resource retirement follows pending compute and
draw callbacks. Disabled bloom queues no bloom compute; an active grading draw
still samples the neutral field. GPU duration and 4K performance are not measured.

## Evidence and platform scope

Eight focused Windows GPU cases cover extraction, visible spread at 20%, color
retention, dim/black identity, white and alpha preservation, zero strength,
odd/small targets and absence of history. Sixteen additional production shader
cases cover ordinary/water, detail, AA and LUT composition. Native Ordon toggle,
resize, live reload and normal shutdown observations are retained in
[the evidence ledger](evidence/bloom-2026-10-26/README.md). An unsuccessful Goron
Mines startup is also retained; its cause is unresolved.

macOS remains best effort through native package builds and portable checks.
Gameplay and Metal execution are unverified. No remote Metal execution is
required. The package contains no game or third-party texture payloads.
