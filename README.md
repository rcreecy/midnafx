# MidnaFX

MidnaFX 1.16 improves scene contact lighting at steep surfaces and depth edges.
Surface normals follow projected plane continuity instead of crossing a nearby
depth step. Existing AO, contact shadows and color bounce share the improvement,
without new controls or rendering passes. See [contact lighting](docs/contact-lighting.md).

MidnaFX 1.15 upgrades **Enhanced model shading** with guarded automatic rigid-model
coverage and live activation. Safe regions can improve while uncertain seams and
shared normal indices retain their original shading, without new asset lists.
Enable the independent Basic control; it remains opt-in. See
[model shading](docs/model-shading.md) for ownership, coverage and limitations.

MidnaFX 1.14 adds experimental **Sunlight shafts** across supported outdoor
scenes. Native sun color and visibility drive depth-occluded rays without asset
lists or extra targets/passes. Select **Profile / Remaster**, or its independent
Basic toggle. Advanced lighting strength defaults to 10%. See
[sunlight shafts](docs/sunlight-shafts.md) for evidence and screen-space limits.

MidnaFX 1.13 adds experimental **Water scene reflections** on supported enhanced
water. Nearby visible opaque scenery contributes reflections without new asset
lists or rendering passes. Select **Profile / Remaster**, or enable Enhanced
water and its independent Basic reflection toggle. Off-screen scenery retains
the existing tint. See [water scene reflections](docs/water-scene-reflections.md)
for geometric evidence and screen-space limits.

MidnaFX 1.12 adds **Motion clarity** across gameplay contexts. It reduces native
previous-frame blending while preserving event/demo-camera blur, without asset
lists. Select **Profile / Remaster** or the independent Basic toggle. Advanced:
motion offers 0–100% retained native blend alpha, defaulting to 0%. See
[motion clarity](docs/motion-clarity.md) for ownership and evidence.

MidnaFX 1.11 adds **Screen-space color bounce (experimental)** across visible
opaque scenery and characters. Nearby facing surfaces contribute restrained color
using the native scene image and depth, without asset lists. Select **Profile /
Remaster** or the independent Basic toggle; Advanced lighting strength defaults
to 12%. See [color bounce](docs/color-bounce.md) for scope and evidence.

MidnaFX 1.10 adds **Movement camera framing** for exploration. The view widens
smoothly during brisk movement and settles at rest, with native aiming and
authored camera contexts preserved. Select **Profile / Remaster** or its
independent Basic control; Advanced offers a 0–12% boost, defaulting to 8%.
See [movement framing](docs/movement-camera.md) for compatibility and evidence.

MidnaFX 1.9 adds **Contact shadows** across visible opaque scenery and characters.
Short rays toward the native environment light add directional grounding alongside
ambient occlusion. Select **Profile / Remaster** or the independent Basic control;
default strength is 30%. No asset lists or extra texture targets are required.
See [contact shadows](docs/contact-shadows.md) for scope, evidence and limits.

MidnaFX 1.8 upgrades scene **anti-aliasing** with edge-directed coverage and
bounded endpoint searches. Stair-step edges receive smoothing while aligned
edges and tested fine lines retain their source detail. Existing AA controls
and defaults remain; no asset lists or extra render targets are needed. See
[edge-directed AA](docs/edge-antialiasing.md) for evidence and cost limits.

MidnaFX 1.7 adds **Scene bloom** across bright scenery, characters and replacement
textures. Select **Profile / Remaster** or its independent Basic control for
restrained glow at 20% default strength. Vanilla disables it. This LDR treatment
uses no asset lists and preserves the native HUD; it does not identify emissive
materials or add new light sources. See [scene bloom](docs/scene-bloom.md).

MidnaFX 1.6 upgrades **ambient contact shading** across opaque scenery and
characters. A denser half-resolution AO field receives depth/normal-aware
filtering and edge-aware reconstruction, improving contacts without temporal
history or per-asset lists. Existing AO controls and the Remaster profile use
the upgrade automatically. See [contact shading](docs/ambient-occlusion.md).

MidnaFX 1.5 adds **Adaptive exposure** across the entire scene, including scenery,
characters and replacement textures. It gradually opens dark views and restrains
bright views, with bounded correction that preserves the game's night and dungeon
mood. Select **Profile / Remaster** or enable the independent Basic control.
**Vanilla** disables it. Existing settings are preserved until a profile is selected.
The effect shares the grading/water draw and leaves the HUD untouched. See
[adaptive exposure](docs/adaptive-exposure.md) for comparisons and limits.

MidnaFX 1.3 adds **Profile / Remaster**, the first control in the mod's
settings. It combines natural grading, detail, modern exploration camera,
anti-aliasing, enhanced water, depth atmosphere and ambient contact shading across opaque
scenery and characters. **Vanilla** disables these profile effects together.
Ambient occlusion can also be enabled separately and adjusted with strength and
radius controls. See the [feature and comparison notes](docs/ambient-occlusion.md).
Depth atmosphere uses the native fog palette and range, with a separate strength
control. It shares AO's draw and snapshots, bypassing underwater cameras and
paused game menus. See [atmosphere](docs/depth-atmosphere.md).
It uses scene depth and leaves the HUD untouched; it does not
add dynamic lights, material normal maps or off-screen shadows. Existing installs
retain their settings until a profile is selected.

MidnaFX 1.4 adds **Enhanced Link shading** to Basic controls and the
Remaster profile. It conservatively smooths supported regions of Link's original
Ordon Clothes and, since 1.4.1, Hero's Clothes, preserving uncertain normals and
authored boundaries. It activates
on the resident model; disabling it or selecting Vanilla restores the original
normal bytes. Armor outfits and replacement models are unchanged. This improves
lighting continuity without adding polygons or changing the silhouette.

MidnaFX is an experimental native visual processing mod for Dusklight. It provides a fused
pre-HUD grading shader with exposure, black point, contrast, gamma, saturation, highlight
rolloff, temperature and tint controls, plus optional five-tap detail enhancement.
Neutral grading with detail, AA and adaptive exposure off and Color Look set to Neutral bypasses the grading pass. Save,
load, and duplicate looks under Visual profile; Vanilla disables profile effects and
Custom retains its own live look across preset switches. A forced RGBA passthrough remains
available for validating the render path. Developer views provide A/B split, luminance,
highlight/shadow clipping, and amplified difference using the same scene snapshot.
The development build adds opt-in **Color Look** and **Look Intensity** controls.
Neutral preserves the existing image. MidnaFX Natural adds restrained tonal
separation; Twilight smoothly blends toward a cooler table during active Twilight.
Both creative looks remain experimental. Enable grading to use them. Selection
and intensity persist independently of visual profiles; changing a profile does
not replace the selected color look. Developer diagnostics include a bypass and
resource counters. See [the LUT contract and qualification record](docs/m13-continuation.md).

The development build enables experimental pre-HUD anti-aliasing by default.
Its Basic toggle operates independently of grading; Remaster and Vanilla include
it in their scene switches, while older saved looks preserve its state. Existing
saved AA-off values are preserved. The edge-directed filter shares source samples
with detail, and detail does not sharpen edges selected for AA. See the
[AA correctness review](docs/notes/m13-aa-correctness-review.md) for repairs,
headless GPU proof, and the remaining visual-quality limitations.
Optional CPU diagnostics report stage p50/p95, layout/resolve call time, snapshot
requests, and pipeline counts. An opt-in Twilight prototype can blend from the
general look toward a user-captured target when Dusklight reports active
Twilight. Conservative, default-off adaptive normal smoothing is available for a
structurally qualified rigid models, with exact known-good fixtures for previously
validated representations. Unknown skinning and ambiguous topology fail closed.
The development build adds Basic, Advanced, and Developer sections, plus Vanilla+
and Enhanced profiles. Both profiles leave model shading and DOF off pending
broader visual validation. See the [product-quality review](docs/product-quality-review.md)
for current coverage and remaining blockers; these changes are not in the published
v1.0.0 package. Grading, active/normal Twilight
detection, rigid geometry, and skinned geometry have been exercised in a
source-matched Windows D3D11 host. Twilight-spot behavior has deterministic test
coverage but no live capture. The development build also has a default-off
modern exploration camera for Dusklight builds carrying MidnaFX's CameraService
1.3 host patch. Independent controls widen vertical FOV and lower native
chase-controller latitude. Only active mode-0 chase output is modified;
authored events, detached cameras, and all other native camera algorithms keep
their original framing. A default-off depth-of-field prototype separates near
and far blur at half resolution, then composites against full-resolution depth
before the HUD. Manual focus works with the base camera service. Camera-target
focus requires MidnaFX's CameraService 1.4 host extension. The quality pass adds
an adjustable 2-12 pixel radius, smooth autofocus, and silhouette-gated near
compositing. Windows runtime checks cover focus orientation, HUD exclusion,
movement, resize, and clean shutdown. New Ordon on/off captures show an intact
Link silhouette and HUD, but are not frame-matched and do not close the full
art-quality matrix. Intel Mac/Metal runtime validation remains deferred.

Since v1.1, new configurations enable an enhanced-water path by default for a
small exact allowlist of validated TP water materials. It preserves gameplay
water, authored animation, splashes, particles, and geometry while adding
restrained depth absorption, animated surface detail, bounded refraction,
Fresnel/environment response, shoreline treatment, and authored-light
specular. Exact classification has live D3D12 evidence across outdoor, moving,
dungeon, boss, and submerged scenes; the optical path has additional
waterfall-adjacent and irregular-shore proof. Unknown water remains native;
screen-space reflections are deferred after an unsuccessful research prototype.
See [the M12 investigation](docs/water-investigation.md) for exact coverage and
limits.

Download `midnafx-windows-amd64.dusk` or `midnafx-macos-x86_64.dusk` from the
[latest release](https://github.com/rcreecy/midnafx/releases/latest), then put that file
in Dusklight's mods folder. Each package contains one platform's native library.
The macOS package is CI-built but has not received an Intel Mac runtime pass.

MidnaFX includes **Natural / Vivid Realism** under **Basic: visual profile**.
This parametric profile is separate from the new **MidnaFX Natural** Color Look.
Turn on **Enable grading** to use it. The look adds restrained color,
a small midtone lift, softer highlights, and subtle detail while preserving neutral
white balance and the black floor. Save or duplicate it to create an editable copy;
select Vanilla to disable profile effects. See [the preset notes](docs/vivid-realism.md)
for exact settings, costs, and visual validation limits.

Research and exact source revision: [docs/research.md](docs/research.md).
Realism preset settings and validation: [docs/vivid-realism.md](docs/vivid-realism.md).
Architecture and limits: [docs/architecture.md](docs/architecture.md).
Build and Intel Mac validation: [docs/development.md](docs/development.md).
Performance instrumentation and Mac capture plan: [docs/performance.md](docs/performance.md).
Detail design and sample costs: [docs/detail.md](docs/detail.md).
One-session Intel Mac checklist: [docs/runtime-validation.md](docs/runtime-validation.md).
Twilight source audit and M6 decision: [docs/m5-m6-investigation.md](docs/m5-m6-investigation.md).
Camera foundation, host patch, and runtime evidence: [docs/camera-investigation.md](docs/camera-investigation.md).
Geometry coverage checkpoint: [docs/m8-geometry-coverage.md](docs/m8-geometry-coverage.md).
Atmosphere depth/camera gate: [docs/atmosphere-investigation.md](docs/atmosphere-investigation.md).
Depth of field focus gate: [docs/depth-of-field-investigation.md](docs/depth-of-field-investigation.md).
Water modernization gates and runtime evidence: [docs/water-investigation.md](docs/water-investigation.md).
v1 milestone disposition and ship status: [docs/v1-readiness.md](docs/v1-readiness.md).
