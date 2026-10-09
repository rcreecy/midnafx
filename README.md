# MidnaFX

MidnaFX is an experimental native visual processing mod for Dusklight. It provides a fused
pre-HUD grading shader with exposure, black point, contrast, gamma, saturation, highlight
rolloff, temperature and tint controls, plus optional five-tap detail enhancement.
Neutral grading with detail and AA off bypasses the grading pass. Save,
load, and duplicate looks under Basic: visual profile; Vanilla restores neutral settings and
Custom retains its own live look across preset switches. A forced RGBA passthrough remains
available for validating the render path. Developer views provide A/B split, luminance,
highlight/shadow clipping, and amplified difference using the same scene snapshot.
The development build enables experimental pre-HUD anti-aliasing by default.
Its Basic toggle operates independently of grading and saved looks; existing
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

Post-v1 development also includes a default-off enhanced-water path for a
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

MidnaFX includes **Natural / Vivid Realism** under **Basic: visual profile** in
the development build (the released build uses **Presets > Current preset**).
Turn on **Enable grading** to use it. The look adds restrained color,
a small midtone lift, softer highlights, and subtle detail while preserving neutral
white balance and the black floor. Save or duplicate it to create an editable copy;
select Vanilla to restore neutral grading. See [the preset notes](docs/vivid-realism.md)
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
