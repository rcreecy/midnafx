# Adaptive exposure

Adaptive exposure adjusts the whole scene, without model fingerprints, material
lists or stage-specific tuning. It benefits scenery, characters, particles and
external texture replacements through the shared pre-HUD renderer. It adds no
lights, geometry or HDR output.

Select **Profile / Remaster** or enable **Adaptive exposure** in Basic. The control
works independently of grading. **Vanilla** disables it. New and existing standalone
configurations keep it off until enabled or Remaster is selected. Saved scene looks
include the switch; older grading-only looks preserve it. Existing scene looks with
the former four-bit mask load with adaptive exposure off.

Version 1.20 meters sixteen fixed stratified positions inside each cell of a
center-weighted 16×16 grid, for 4,096 scene samples. Different fixed offsets within
the strata reduce coherent aliasing from repeating textures. The pattern does
not change between frames, so a stationary image receives no sampling jitter.
Scene-code Rec.709 luminance excludes near-black samples and black bars.

A weighted 64-bin log-luminance histogram finds the 10th and 90th percentile
bins. Samples outside those bin edges are clamped before the geometric mean is
calculated. This limits isolated bright or dark features without discarding a
large bright region. Histogram edges are spaced by one eighth of a stop; using
edges instead of bin centers preserves uniform-scene luminance. This remains an
LDR meter, not emissive classification or material-aware exposure.

Correction is bounded to -0.5 through +0.75 stops around the original scene, with
1.2-second brightening and 0.4-second darkening time constants. Dark scenes remain
dark; this is restrained readability correction, not normalization to daylight.
The untrimmed geometric mean still controls black/fade rejection, so percentile
clamping cannot turn a near-black fade into a valid bright scene. Black fades
bypass correction, and near-black fades attenuate history. The exposure
curve preserves RGB ratios and avoids clipping bright endpoints when brightening.
Manual exposure remains an independent grading adjustment after this curve.

One compute dispatch still writes a single texel of history. Two fixed 1×1 RGBA16Float
textures alternate; a third zero-stop texture provides exact bypass. Existing
grading/water/AA/detail/LUT draws sample the result before their tone controls.
There is no CPU readback, extra scene resolve, full-frame intermediate or extra
fullscreen draw. Disabled metering submits no compute. A neutral texel is still
sampled by non-bypassed grading draws. The denser meter adds texture samples,
histogram atomics and workgroup synchronization. No performance equivalence to
the old 256-point meter is claimed; GPU duration has not been measured.

History restarts on stage, current room or render layout changes, a rendering gap
over 0.5 seconds, disable/re-enable, pause, diagnostic bypass and module reload.
Only frames with a fresh world render and live player are eligible. Unsupported
metering retains original exposure. Resource creation is checked with consumed
WebGPU error scopes, and host callback draining precedes resource release.

Windows D3D12 evidence includes matched-camera day/night switches with two external
high-resolution texture samples, plus automated temporal and composition checks. See the
[evidence record](evidence/adaptive-exposure-2026-10-26/README.md).
Native dungeons, combat flashes and cutscene transitions still need wider art
evaluation. The finite sample pattern can still miss tiny features or alias with
untested texture patterns. The percentile bins do not provide exact continuous
percentiles or remove all camera-motion variation. No local exposure or HDR
reconstruction is claimed. Intel macOS builds remain best effort; native
Metal/gameplay verification is unavailable and is not a release gate.

The [1.20 evidence ledger](evidence/robust-exposure-2026-11-01/README.md) retains
eight new spatial GPU groups plus the existing history/fade checks. Moving small
bright/dark patches, translating narrow stripes, large bright regions, fixed
sampling, alpha and tiny/odd inputs are covered. A native Ordon trial passes
toggle, fullscreen resize/return, live reload and normal close with exit zero.
These are focused checks, not whole-game camera-motion or performance qualification.
