# MidnaFX

MidnaFX is an experimental native visual processing mod for Dusklight. It provides a fused
pre-HUD grading shader with exposure, black point, contrast, gamma, saturation, highlight
rolloff, temperature and tint controls, plus optional five-tap detail enhancement.
Neutral grading with detail off bypasses the render pass. Save,
load, and duplicate looks in the Presets section; Vanilla restores neutral settings and
Custom retains its own live look across preset switches. A forced RGBA passthrough remains
available for validating the render path. Developer views provide A/B split, luminance,
highlight/shadow clipping, and amplified difference using the same scene snapshot.
Optional CPU diagnostics report stage p50/p95, layout/resolve call time, snapshot
requests, and pipeline counts. An opt-in Twilight prototype can blend from the
general look toward a user-captured target when Dusklight reports active
Twilight. Conservative, default-off adaptive normal smoothing is available for a
small exact allowlist of validated game models. Grading, active/normal Twilight
detection, rigid geometry, and skinned geometry have been exercised in a
source-matched Windows D3D11 host. Twilight-spot behavior has deterministic test
coverage but no live capture. The development build also has a default-off
modern exploration camera for Dusklight builds carrying MidnaFX's CameraService
1.3 host patch. Independent controls widen vertical FOV and lower native
chase-controller latitude. Only active mode-0 chase output is modified;
authored events, detached cameras, and all other native camera algorithms keep
their original framing. Intel Mac/Metal runtime validation is still required.

Download `midnafx-windows-amd64.dusk` or `midnafx-macos-x86_64.dusk` from the
[latest release](https://github.com/rcreecy/midnafx/releases/latest), then put that file
in Dusklight's mods folder. Each package contains one platform's native library.
The macOS package is CI-built but has not received an Intel Mac runtime pass.

The development build includes **Natural / Vivid Realism** in **Presets > Current
preset**. Turn on **Enable grading** to use it. The look adds restrained color,
a small midtone lift, softer highlights, and subtle detail while preserving neutral
white balance and the black floor. Save or duplicate it to create an editable copy;
select Vanilla to restore neutral grading. See [the preset notes](docs/vivid-realism.md)
for exact settings, costs, and visual validation limits.

Research and exact source revision: [docs/research.md](docs/research.md).
Architecture and limits: [docs/architecture.md](docs/architecture.md).
Build and Intel Mac validation: [docs/development.md](docs/development.md).
Performance instrumentation and Mac capture plan: [docs/performance.md](docs/performance.md).
Detail design and sample costs: [docs/detail.md](docs/detail.md).
One-session Intel Mac checklist: [docs/runtime-validation.md](docs/runtime-validation.md).
Twilight source audit and M6 decision: [docs/m5-m6-investigation.md](docs/m5-m6-investigation.md).
Camera foundation, host patch, and runtime evidence: [docs/camera-investigation.md](docs/camera-investigation.md).
Geometry coverage checkpoint: [docs/m8-geometry-coverage.md](docs/m8-geometry-coverage.md).
