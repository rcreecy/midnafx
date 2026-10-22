# MidnaFX remaster program

The user authorizes continued implementation and iteration toward a coherent Twilight Princess remaster capability, with visual quality and completeness approaching a studio remaster. This is a product program rather than another diagnostic-only milestone. Each completed slice must produce a working capability, preserve the original game's behavior, and carry explicit evidence and limitations. Build success and asset identification alone do not establish remaster quality.

On 2026-10-22 the user confirmed [Nintendo's Ocarina of Time for Switch 2](https://www.nintendo.com/us/store/products/the-legend-of-zelda-ocarina-of-time-switch-2/) as the reference. Nintendo describes a ground-up rebuild and publishes an [overview trailer](https://www.nintendo.com/us/whatsnew/you-have-been-summoned-to-watch-the-new-legend-of-zelda-ocarina-of-time-overview-trailer/). Its page lists November 5, 2026, so the current comparison is published promotional material, not hands-on evaluation. The page advertises revised visuals, camera/controls, voiced cutscenes and orchestral music; it does not disclose the rendering implementation. Do not infer a particular PBR, lighting or shadow technique from that description.

Use that reference as a quality and completeness target while preserving Twilight Princess's art direction. Evaluate consistent materials, silhouettes, animation, lighting, image stability, camera, readable presentation and coherent scene coverage. Additional authored models, animation, sound/music and interface assets need content pipelines and suitable source assets alongside rendering code. Record those content dependencies instead of claiming that shader effects alone deliver the reference's complete remake scope. Ship of Harkinian and Zelda 64: Recompiled remain native-port engineering references, not the user's visual target.

## Platform and content contract

Windows/D3D12 is the available runtime qualification platform. macOS remains supported on a best-effort basis through native package builds, portable checks and compatibility-conscious implementation. Native macOS gameplay/Metal verification is unavailable and does not block development or release. Run Metal validation only if executable on the current working host; do not require remote hosted Metal checks. See the [working-host policy](development.md#validation-on-the-working-host).

Existing high-resolution replacements remain compatible. Original resource/material identity and replacement identity stay separate. Never infer a complete material model from an albedo filename/hash. Content profiles may refer to external user-supplied packs, but the MidnaFX package must not acquire or redistribute game/third-party texture payloads as part of this work. Ship code, owned configuration and authored metadata separately from external assets.

Preserve gameplay, authored material boundaries, animation/skinning, transparency ordering and HUD behavior. Keep a reliable disable/bypass path and restore owned state on disable, archive destruction and mod unload. Unsupported resources must retain their original rendering. Native host changes require versioning, ownership and compatibility design rather than an undocumented dependency on one local executable.

## Delivery sequence

| Slice | Observable completion | Current state |
|---|---|---|
| R0: reliable runtime and measurement | Explain/fix or explicitly isolate the baseline shutdown stall; measure frame timing and observer allocation behavior during steady rendering, active sampling and texture reload; retain valid baseline/off/on controls | Full-queue shutdown defect reproduced and repaired; bounded CPU timing collector passes native smoke; paired cost/allocation measurements and historical stall attribution remain open |
| R1: selective material control | Toggle a visible change on the fingerprinted pumpkin body while leaves and surrounding scene remain unchanged; disabling/restoring/reloading recovers original state; original and high-resolution replacements work | Visible reversible RGB-scale control passes original/pack comparisons; archive round trip and mod reload remain pending; see material-control.md |
| R2: authored material enhancement | Apply bounded profiles to several distinct material classes using original identities; support predictable color/specular/normal inputs only where rendering data and control are sufficient; document effects and fallback | Requires R1 control seam and material/shader capability audit |
| R3: geometry and character consistency | Improve selected silhouettes/normals without seam, skinning, animation or shared-instance corruption; broaden verified asset coverage beyond existing allowlists | Narrow geometry/smoothing foundations exist; broad coverage is incomplete |
| R4: coherent environment rendering | Integrate existing grading/LUT, atmosphere, depth, water and camera capabilities with material-aware exclusions and quality presets; add lighting/reflection/shadow capabilities only with suitable inputs and measured control | Several effects exist; generic material-aware integration remains incomplete |
| R5: remaster-quality product | Curated visible improvements across a representative scene/character corpus, stable motion, reliable reload/shutdown, clear quality presets, documented performance and packaged Windows/macOS builds | Not yet qualified |

R0 and R1 are the immediate implementation priorities. Avoid extending diagnostics indefinitely without crossing into a visible, reversible rendering capability. Conversely, do not claim normal mapping, PBR, dynamic shadows or reflections merely because asset metadata is available. Audit the required normals, tangents, lights, depth, shader ownership and GPU budget, then implement a complete supported path or record the specific missing capability.

## Immediate acceptance

1. Reproduce the shutdown stall with a source-matched host, capture its actual blocking stack or phase, and distinguish native host behavior from mod behavior. Preserve the original failed observation. Do not diagnose Discord or cache saving solely from the last buffered log line.
2. Add only the instrumentation needed to measure frame and allocation costs. Separate process counters, retained memory, actual allocation counts, CPU frame intervals, encoding time and GPU time. Label unsupported measurements; do not substitute one metric for another.
3. Implement an opt-in body-material control through the narrowest native/J3D seam with an explicit game-thread lifetime and state-restoration contract. It must operate independently of diagnostic logging being enabled.
4. Exercise baseline, changed and restored views with original and replacement textures. Confirm both positive effect and absence of modification to leaves/unrelated materials. Check archive round trip, mod reload and shutdown. A shared model/material control is not a claim of per-actor-instance isolation.

## Product quality and iteration

Maintain an acceptance ledger distinguishing implemented, automatically tested, visually observed, unavailable and failed. A diagnostic screenshot is not a motion-quality pass. A rendered draw is not proof of GPU completion or visible pixels. Record package/host revisions, backend, viewport, settings and pack identity for runtime evidence.

Grow coverage through outdoor foliage/architecture, interiors/dungeons, water, night/Twilight, characters/skinning, transparent effects, combat and cutscenes. Use matched routes or reproducible camera states where possible. Failures must remain recorded; do not select only passing scenes or reruns.

Performance targets must be tied to measured hardware and quality profiles. The available Intel UHD 630 host cannot attest high-end GPU or 4K performance. Retain low-cost bypass and sensible quality tiers; publish measured cost instead of a universal FPS promise. No unsupported GPU feature may prevent basic rendering.

Commit and validate coherent slices, continue to the next supported slice, and publish versions with accurate feature/platform limitations. The overall program is complete only when the accepted feature/quality ledger supports a remaster-quality experience; isolated pumpkin control or another successful CI run does not complete it.

## Checkpoint: 2026-10-22

Windows package build and all 21 CTest entries pass, including D3D12 pixels and
the 12-case frame analyzer suite. Original/pack material comparisons restore all
sampled rectangles exactly; their apply/restore totals balance and normal close
completes. The CPU timing collector emits 1,484 consistent frames with no drops
in its smoke trial. See [material control](material-control.md),
[shutdown repair](decomp-shutdown.md) and [frame timing](frame-timing.md).

The goal's usage limit stops this iteration before the full program is complete.
Resume with R1 archive departure/return and mod reload qualification, then paired
R0 settled/sampling/replacement-reload frame measurements and actual allocation
capture where available. Broader authored material profiles, geometry/character
coverage, coherent environment rendering and product qualification remain open.
Preserve the confirmed Nintendo Switch 2 reference and best-effort macOS policy.
