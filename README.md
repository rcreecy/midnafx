# MidnaFX

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
Neutral grading with detail and AA off and Color Look set to Neutral bypasses the grading pass. Save,
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
saved AA-off values are preserved. The five-tap filter shares source samples
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
