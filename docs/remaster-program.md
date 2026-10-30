# MidnaFX remaster program

The user authorizes continued implementation and iteration toward a coherent Twilight Princess remaster capability, with visual quality and completeness approaching a studio remaster. This is a product program rather than another diagnostic-only milestone. Each completed slice must produce a working capability, preserve the original game's behavior, and carry explicit evidence and limitations. Build success and asset identification alone do not establish remaster quality.

On 2026-10-22 the user confirmed [Nintendo's Ocarina of Time for Switch 2](https://www.nintendo.com/us/store/products/the-legend-of-zelda-ocarina-of-time-switch-2/) as the reference. Nintendo describes a ground-up rebuild and publishes an [overview trailer](https://www.nintendo.com/us/whatsnew/you-have-been-summoned-to-watch-the-new-legend-of-zelda-ocarina-of-time-overview-trailer/). Its page lists November 5, 2026, so the current comparison is published promotional material, not hands-on evaluation. The page advertises revised visuals, camera/controls, voiced cutscenes and orchestral music; it does not disclose the rendering implementation. Do not infer a particular PBR, lighting or shadow technique from that description.

Use that reference as a quality and completeness target while preserving Twilight Princess's art direction. Evaluate consistent materials, silhouettes, animation, lighting, image stability, camera, readable presentation and coherent scene coverage. Additional authored models, animation, sound/music and interface assets need content pipelines and suitable source assets alongside rendering code. Record those content dependencies instead of claiming that shader effects alone deliver the reference's complete remake scope. Ship of Harkinian and Zelda 64: Recompiled remain native-port engineering references, not the user's visual target.

## Platform and content contract

Windows/D3D12 is the available runtime qualification platform. macOS remains supported on a best-effort basis through native package builds, portable checks and compatibility-conscious implementation. Native macOS gameplay/Metal verification is unavailable and does not block development or release. Run Metal validation only if executable on the current working host; do not require remote hosted Metal checks. See the [working-host policy](development.md#validation-on-the-working-host).

Existing high-resolution replacements remain compatible. Original resource/material identity and replacement identity stay separate. Never infer a complete material model from an albedo filename/hash. Content profiles may refer to external user-supplied packs, but the MidnaFX package must not acquire or redistribute game/third-party texture payloads as part of this work. Ship code, owned configuration and authored metadata separately from external assets.

Preserve gameplay, authored material boundaries, animation/skinning, transparency ordering and HUD behavior. Keep a reliable disable/bypass path and restore owned state on disable, archive destruction and mod unload. Unsupported resources must retain their original rendering. Native host changes require versioning, ownership and compatibility design rather than an undocumented dependency on one local executable.

## Delivery sequence

The 2026-10-26 direction prioritizes broad improvements to the play experience.
Prefer scene-wide or cross-category rendering and camera features over extending
single-asset lists. Expand a specific identity only when required to remove a
demonstrated blocker. Adaptive exposure is the next delivered global slice;
broader environment lighting and image stability remain the next product areas.

Version 1.6 follows with filtered contact shading across opaque scene geometry.
It replaces the sparse full-resolution kernel with a denser half-resolution field
and surface-aware filtering, retaining native depth and existing controls. This
is another global rendering improvement, not an expansion of a single asset list.

Version 1.7 adds scene-wide LDR bloom using quarter-resolution highlight
extraction and separable filtering, fused with existing grading/water draws.
Remaster enables restrained 20% glow; Vanilla disables it. Focused GPU checks
and native Ordon lifecycle checks pass. Emissive classification, new dynamic
lighting and whole-game art/motion quality are not established. An unsuccessful
direct Goron Mines startup remains recorded in the [bloom evidence](evidence/bloom-2026-10-26/README.md).

Version 1.8 improves scene-wide spatial AA through bounded endpoint searches,
orientation symmetry and fine-detail rejection. It reuses existing ordinary/water
draws and controls. Supersampled coverage and preservation comparisons distinguish
the quality change from another layer of global softening. It does not provide
temporal reconstruction or establish whole-game motion quality. See
[edge-directed AA](edge-antialiasing.md).

Version 1.9 adds short directional contact shadows from the native environment
base light across visible opaque scenery and characters. It shares the existing
AO targets, filtering and reconstruction, without asset identities. Remaster
enables the effect at 30%; Vanilla disables it. The effect supplements native
shadows rather than replacing the game's full lighting system. See
[contact shadows](contact-shadows.md) for geometric evidence and scope limits.

Version 1.10 adds movement-aware exploration framing across native chase-camera
contexts. An independent toggle widens the view during brisk movement and
settles at rest, using native speed rather than asset identities or positional
deltas. Remaster enables the feature; Vanilla disables it. Existing aiming and
authored-camera safety gates remain in force. See [movement framing](movement-camera.md).

Version 1.11 adds bounded screen-space color bounce across visible opaque
scenery and characters. It gathers nearby facing surface colors from the
existing native snapshot and reuses contact-shading geometry and filters.
Remaster enables restrained 12% strength; Vanilla disables it. This is a visible
LDR approximation, not a material-aware global illumination system. See
[color bounce](color-bounce.md).

Version 1.12 adds gameplay motion clarity through a frame-local reduction of the
native previous-frame blend alpha. Remaster enables it with zero retained blend;
event/demo/pause/fade contexts preserve native blur. It adds no rendering pass or
host patch and does not replace motion-vector reconstruction. See
[motion clarity](motion-clarity.md).

Version 1.13 adds experimental visible-scenery reflections to supported enhanced
water. It reuses pre-water opaque depth/color and qualified water surfaces,
without expanding asset identities or adding targets/passes. Remaster enables
the feature; missed and off-screen sources retain the existing tint. This adds
environment response while retaining first-layer screen-space limits. See
[water scene reflections](water-scene-reflections.md).

Version 1.14 adds experimental outdoor sunlight shafts, driven by native sun
position, color and visibility. Visible opaque silhouettes interrupt the radial
field, with foreground suppression and bounded linear-light composition. It
shares the existing opaque-stage draw and adds no targets or asset identities.
Remaster enables it at 10% strength; screen-space and transparent-occlusion
limits remain explicit. See [sunlight shafts](sunlight-shafts.md).

Version 1.15 returns to geometry consistency by upgrading the existing opt-in
model shading control. Structurally qualified rigid models can retain uncertain
seam/index regions while improving safe normals, without new asset identities.
Resident activation, native deformation interception and per-normal restoration
make the path usable without a rendering effect or extra geometry. This does not
establish complete geometry coverage or justify default-on art quality. See
[model shading](model-shading.md).

Version 1.16 improves the shared contact-lighting surface reconstruction across
AO, contact shadows and color bounce. Projected-plane continuity prevents a
nearby unrelated depth step from reversing a steep surface normal. It reuses
existing controls, fields and passes, with focused analytic rendering comparisons.
Screen-space and whole-game quality limits remain; see [contact lighting](contact-lighting.md).

Version 1.17 adds experimental local-light response using the native ordinary
point-light registry. Supported torch, campfire and stage sources supplement
visible opaque scenery with bounded colored illumination and screen-space
occlusion. No new asset lists or renderer pass types are required. Native lantern
spotlights, hidden geometry and full material relighting remain outside scope.
See [local lighting](local-lighting.md).

Version 1.18 extends local-light response to the default native lantern channel,
including source movement, native steep attenuation and reference-power fading.
It reuses the existing control and four-source pipeline. Controlled native
source-channel integration is observed; equipped-lantern gameplay, unsupported
spotlight modes and whole-game quality remain open. See
[lantern coverage](local-lighting.md#player-lantern-source).

Version 1.19 upgrades scene-wide bloom with a tight highlight core and a broader
multi-scale halo. Normalized reconstruction retains the existing strength and
uniform-field response while redistributing light beyond the original Gaussian
footprint. This uses the existing controls and composite, with additional bounded
compute work and field memory. Synthetic rendering demonstrates visible spread
at the default strength; native desktop capture is unavailable in this iteration.
See [scene bloom](scene-bloom.md) for evidence and remaining quality limits.

| Slice | Observable completion | Current state |
|---|---|---|
| R0: reliable runtime and measurement | Explain/fix or explicitly isolate the baseline shutdown stall; measure frame timing and observer allocation behavior during steady rendering, active sampling and texture reload; retain valid baseline/off/on controls | Full-queue shutdown defect reproduced and repaired; eight settled CPU timing controls retained; active sampling/reload, allocation measurements and historical stall attribution remain open |
| R1: selective material control | Toggle a visible change on the fingerprinted pumpkin body while leaves and surrounding scene remain unchanged; disabling/restoring/reloading recovers original state; original and high-resolution replacements work | Visible reversible RGB-scale control passes original/pack comparisons; cached-binding reload defect repaired; final-package mod reload and Ordon–Fishing Pond–Ordon archive round trip pass their observed lifecycle checks; failed Hyrule Field transition and broader quality/compatibility remain open; see material-control.md |
| R2: authored material enhancement | Apply bounded profiles to several distinct material classes using original identities; support predictable color/specular/normal inputs only where rendering data and control are sufficient; document effects and fallback | First authored warm pumpkin profile implemented with lighting-preserving material color; original/pack rectangle comparisons, cached mod reload and archive round trip pass; additional classes and broader quality/performance remain open |
| R3: geometry and character consistency | Improve selected silhouettes/normals without seam, skinning, animation or shared-instance corruption; broaden verified asset coverage beyond existing allowlists | Narrow geometry/smoothing foundations exist; broad coverage is incomplete |
| R4: coherent environment rendering | Integrate existing grading/LUT, atmosphere, depth, water and camera capabilities with material-aware exclusions and quality presets; add lighting/reflection/shadow capabilities only with suitable inputs and measured control | Several effects exist; generic material-aware integration remains incomplete |
| R5: remaster-quality product | Curated visible improvements across a representative scene/character corpus, stable motion, reliable reload/shutdown, clear quality presets, documented performance and packaged Windows/macOS builds | Not yet qualified |

The 2026-10-25 user review changes the immediate priority to visible, scene-wide
capabilities. R0/R1 have enough foundations to support feature development;
additional observer qualification is not the default next deliverable. The first
slice is depth-based ambient contact shading across opaque scenery and characters,
with a single Remaster profile combining it with grading, detail, camera, AA and
water. These capabilities are implemented for 1.3.0, with native Ordon comparisons
and Windows checks; broad scene and motion quality remain open. Depth atmosphere
now shares the contact-shading pass and uses native fog color/range. Follow with
broader environment lighting and character/geometry coverage.
Each iteration must deliver an observable improvement or remove a demonstrated
blocker to that improvement. Do not claim normal mapping, PBR or dynamic shadows
from asset metadata alone.

## Immediate acceptance

1. Deliver a usable, visible feature across a whole scene, rather than another
   diagnostic or single-object tint. Put Remaster and Vanilla controls first.
2. Compare the new effect enabled and disabled at the same camera. Preserve HUD,
   scene depth and existing water integration. Unsupported layouts bypass it.
3. Run the relevant automated checks and a focused reload/close check. Broaden
   validation only for a demonstrated regression or unresolved feature question.
4. Record limitations briefly, package the working slice and continue product work.

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

The resumed iteration repairs a reproduced cached-archive binding gap. Scene
self-reload can retain the archive, so requiring another scene load was unreliable.
One-shot cached binding now qualifies the original resident model without fetches
or reference changes. Independent native endian simulation explains every loaded
source-byte change; only twelve unspecified padding bytes are normalized during
hashing. Final-package native mod reload passes with diagnostics off, replacement
textures on and a non-tracing host. Normal closes retain balanced draw restoration.

Eight repeated settled frame captures compare baseline, tracing host with
diagnostics off/on, and material control separately. Median presentation intervals
remain near the native 30 Hz cap. Material control has a larger tail in one repeat;
GPU and execution-time overhead are not established. Full raw timings and the
unresolved black Hyrule Field transition remain in the evidence ledger.

The final normalized package now also completes an Ordon–Fishing Pond–Ordon
archive departure/return with diagnostics off and external replacements enabled.
Deletion clears the target binding at 17,982 balanced applies/restores; the
return binds again and shutdown reaches 515,310 balanced applies/restores with
zero rejections and `all mods unloaded` logged. The retained lifecycle screenshots
are unchanged copies, not matched visual-quality comparisons. No process exit
code was retained. The failed black Hyrule Field transition remains unresolved;
this successful route does not establish its cause or erase that observation.

Continue with broader R1 scene/motion and compatibility coverage, active sampling
and replacement-reload measurement, and allocation capture where available.
Broader authored material profiles, geometry/character coverage, coherent
environment rendering and product qualification remain open. Preserve the
confirmed Nintendo Switch 2 reference and best-effort macOS policy.

## Checkpoint: 2026-10-25

The following character slice adds user-facing Enhanced Link shading to Basic
and Remaster. It supports the original casual outfit with conservative
preservation of conflicting indices and uncertain faces. Native D3D12 evidence
shows 297 changed normals, exact disable/reload/shutdown restoration and a subtle
localized pixel change. Other outfits and replacement models remain unchanged.
See [character shading](character-shading.md). The older standard-outfit rebuild
experiment remains outside this product path; no new mesh geometry is claimed.

The 2026-10-26 follow-up extends the supported identities to original Hero's
Clothes. It also repairs re-enable after native heap draw copies change, by
planning from fingerprinted original draws paired with original descriptors.
Native outfit switching, direct restoration, re-enable and module reload pass
with 194 changed standard-outfit normals and exact restoration of 10,398 bytes.
See the [artifact-specific record](evidence/standard-link-2026-10-26/README.md).

The first authored material profile adds a subtle warm RGB gain to the exact
original pumpkin body while preserving native lighting and alpha. It defaults
off and works with external replacements independently of diagnostics and the
separate dim proof. Original and replacement candidate image sequences reproduce
the sampled body on the third image and leave five sampled exclusions identical.
The final package's logging correction is recorded separately from those pixel
captures. Final-package native mod reload, cached binding and archive departure/
return pass; normal round-trip shutdown retains 21,480 balanced pairs, zero
rejections and exit code zero. See [the profile ledger](evidence/material-profile-2026-10-24/README.md).

REA now passes owned-fixture Ghidra and bounded Frida qualification. A final game
probe observes 114 warm/restore command pairs after reload. Loaded module and host
hashes match the retained Windows artifact baseline. This connects native source
assumptions to actual packaged and loaded code; it does not measure GPU overhead
or establish arbitrary live-state ownership. All 21 Windows CTest entries pass
after the final build, including AA pixels and package validation.

[CI run 40205847046](https://github.com/rcreecy/midnafx/actions/runs/40205847046)
also passes at code commit `682dacd`: Windows 21/21 tests and Intel macOS 20/20
tests, with native packages for both. Mac AA/Metal execution is excluded under
the working-host policy. The [artifact receipt](evidence/rea-artifacts-2026-10-24/ci-682dacd/README.md)
keeps CI and locally executed hashes separate; no new release is published by
that run. Windows REA providers cannot open its Mach-O module, so Mac gameplay
remains unverified and best effort.

Continue R2 with a distinct supported material class and its required rendering
interface. The inspected pot uses vertex material color and animated TEV inputs,
so applying the pumpkin's register gain would be ineffective or interfere with
game lighting. Additional identities, artistic quality, active replacement-reload
timing, allocation/GPU cost and the historical Hyrule Field failure remain open.
