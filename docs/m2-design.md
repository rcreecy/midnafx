# M2 design checkpoint

M2 extends the verified `GFX_STAGE_FRAME_BEFORE_HUD` snapshot/draw path with one streamed
uniform block and one WGSL fragment invocation per pixel. It adds exposure, black point,
contrast, gamma, saturation, highlight rolloff and temperature/tint. A master-disabled or
fully neutral configuration returns before `resolve_pass`. A separate diagnostic option
can force M1 passthrough to validate the render integration after the M2 upgrade.

The game thread owns a validated scalar snapshot. Each UI or ConfigService change clamps
the affected value to a conservative documented range, recomputes derived constants and
publishes a complete snapshot. The stage callback calls `push_uniform` before `push_draw`;
the payload retains the returned uniform range and frame-only scene view. The render
worker creates its frame-local bind group using only its `GfxDrawContext` handles. A slider
change never rebuilds the shader or pipeline. Individual effect toggles feed neutral
constants into this snapshot, so the fragment shader has no per-effect runtime branch.

The color domain is the host's queried UNORM scene representation; no unverified sRGB
conversion is added. Preserve the source alpha. Operations are ordered: exposure and
temperature/tint balance, black point, contrast, highlight shoulder, saturation, gamma.
Exposure is precomputed as `exp2(EV)`, gamma as reciprocal, and color balance as channel
gains. Black point zero, contrast one, saturation one, rolloff zero, exposure zero,
gamma one and balance zero are the neutral values. The neutral configuration bypasses
the pass, avoiding both arithmetic precision changes and the snapshot cost.

M2 does not add presets, sharpening, automatic Twilight detection, game parameter hooks,
or a LUT. These remain later milestones. The initial GPU target is still unmeasured; use
the Intel Mac method in `performance.md` after a successful host run.

Implemented integer UI ranges (values are divided by 100 when preparing uniforms):

| Control | Range | Neutral |
|---|---:|---:|
| Exposure | -200 to +200 centi-EV | 0 |
| Black point | 0–20% | 0 |
| Contrast | 50–150% | 100 |
| Gamma | 70–150% | 100 |
| Saturation | 0–200% | 100 |
| Highlight rolloff | 0–100% | 0 |
| Temperature, tint | -100 to +100% | 0 |

The current temperature/tint controls apply modest RGB gains rather than a calibrated
Kelvin/chromatic-adaptation model. Highlight rolloff uses a soft shoulder beginning at
0.65 in the host's UNORM scene domain. These are visually experimental until Intel Mac
captures establish color-domain behavior and tune the curves. Alpha remains unchanged.
