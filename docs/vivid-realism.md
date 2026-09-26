# Natural / Vivid Realism

This built-in gameplay look aims for vivid, believable color while retaining the
scene's original lighting. It uses the existing pre-HUD grading and detail shader;
it does not add lighting, materials, geometry, or a new render pass.

Enable grading and select **Natural / Vivid Realism** under **Current preset**.
Use the Final debug view and disable Force passthrough comparison to see the look.
For an unblended comparison, turn off the optional automatic Twilight profile.
The master grading switch remains under user control when selecting any preset.

| Control | Value | Purpose |
| --- | --- | --- |
| Exposure | 0 EV | Keep the scene's exposure |
| Black point | 0% | Preserve shadow information |
| Contrast | 100% | Avoid clipping shadows with the shader's linear contrast pivot |
| Gamma | 102% | Gently lift midtones |
| Saturation | 108% | Add restrained color separation |
| Highlight rolloff | 25% | Soften the shoulder above the existing 0.65 knee |
| Temperature / Tint | 0% / 0% | Keep neutral grays neutral |
| Detail | On, 12% | Add subtle texture definition with the existing edge gate |

All eight grading controls are active. Detail uses five scene samples per pixel
instead of the grading-only path's one sample. Disable detail if its cost or texture
emphasis is undesirable. The preset adds no shader variant or uniform layout change.

The built-in does not consume a saved preset slot. Save and Duplicate create a copy;
editing a grading control selects Custom. Vanilla and the separately retained Custom
look remain available. Stored MFX1 and MFX2 presets remain compatible.

## Validation scope

Automated checks cover serialization and a neutral ramp through the existing grading
equations: black stays black, grays stay neutral, shadow steps remain distinct, and
highlights remain ordered with headroom. These checks do not establish perceptual
realism. Saturation can still clip extreme source colors, and no post-process can
recover detail already clipped in the source.

A Windows D3D11 smoke run on the source-matched development host reached frame 300 at
1216x896 with the candidate values active, one loaded MidnaFX 0.7.0 package, and no
MidnaFX or WebGPU validation error. An offline CPU replay of the grading equations on
two existing outdoor captures measured a 0.83-0.84% mean absolute channel change and
reduced pixels at or above 0.98 from 0.06%/0.58% to zero. This establishes restrained
tonal behavior and highlight headroom; it is not a matched live visual comparison.

After the 0.7.1 release, the same replay was expanded to seven existing real-game
captures covering bright foliage, a dark forest interior, Link, a hard-surface dungeon
object, and active Twilight. Mean absolute channel change remained 0.61-0.83% for the
general-look captures; optional detail contributed 0.019-0.173%. The highlight shoulder
reduced pixels at or above 0.98 from 0.07-3.82% to at most 0.01%. Saturation produced no
upper-gamut excursion, while 0.009-0.834% of sampled channels crossed below zero before
the shader's existing nonnegative gamma guard. The largest value occurred in the dark
Link capture and remains a specific item for matched visual inspection; these data do
not justify changing the shipped 108% saturation without live A/B evidence.

The source-matched Windows D3D11 host also loaded the 0.7.1 package in normal
`F_SP103` and active-Twilight `D_MN08` command-line runs at 1216x896. Both processes
remained responsive through the observation interval with the preset values forced.
They were force-stopped after the smoke interval, so these runs provide no graceful
shutdown or perceptual evidence. Windows screenshot automation was unavailable for
this pass.

Before calling this look visually validated, compare Vanilla and this preset at the
same camera position in daylight, foliage, interiors, night, and active Twilight.
Check skin tones, sky gradients, dark texture detail, bright effects, and the HUD.
Use A/B Split plus highlight/shadow clipping views, then return to Final. Record
host revision, backend, resolution, bloom, and texture packs. Live scene evaluation
and Intel Mac/Metal validation remain outstanding for this preset.
