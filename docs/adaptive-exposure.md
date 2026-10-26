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

The GPU meters a center-weighted 16×16 grid from the renderer's existing scene
snapshot. A log-average uses scene-code Rec.709 luminance and excludes black bars.
Correction is bounded to -0.5 through +0.75 stops around the original scene, with
1.2-second brightening and 0.4-second darkening time constants. Dark scenes remain
dark; this is restrained readability correction, not normalization to daylight.
Black fades bypass correction, and near-black fades attenuate history. The exposure
curve preserves RGB ratios and avoids clipping bright endpoints when brightening.
Manual exposure remains an independent grading adjustment after this curve.

One compute dispatch writes a single texel of history. Two fixed 1×1 RGBA16Float
textures alternate; a third zero-stop texture provides exact bypass. Existing
grading/water/AA/detail/LUT draws sample the result before their tone controls.
There is no CPU readback, extra scene resolve, full-frame intermediate or extra
fullscreen draw. Disabled metering submits no compute. A neutral texel is still
sampled by non-bypassed grading draws. GPU duration has not been measured.

History restarts on stage, current room or render layout changes, a rendering gap
over 0.5 seconds, disable/re-enable, pause, diagnostic bypass and module reload.
Only frames with a fresh world render and live player are eligible. Unsupported
metering retains original exposure. Resource creation is checked with consumed
WebGPU error scopes, and host callback draining precedes resource release.

Windows D3D12 evidence includes matched-camera day/night switches with two external
high-resolution texture samples, plus automated temporal and composition checks. See the
[evidence record](evidence/adaptive-exposure-2026-10-26/README.md).
Native dungeons, combat flashes and cutscene transitions still need wider art
evaluation. The fixed grid can miss very small highlights; no local exposure or
HDR reconstruction is claimed. Intel macOS builds remain best effort; native
Metal/gameplay verification is unavailable and is not a release gate.
