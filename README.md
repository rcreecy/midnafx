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
Twilight. It is disabled until configured and has not been observed in-game.
Windows builds have not been tested in
the game; Intel Mac/Metal validation is still required.

Download platform-specific packages from the latest successful
[Build MidnaFX workflow](https://github.com/rcreecy/midnafx/actions/workflows/build.yml):
`midnafx-windows-amd64` or `midnafx-macos-x86_64`. Extract the artifact ZIP to obtain
`midnafx.dusk`, then put that `.dusk` in Dusklight's mods folder. Each CI package contains
one platform's native library. A CI build does not establish in-game visual behavior.

Research and exact source revision: [docs/research.md](docs/research.md).
Architecture and limits: [docs/architecture.md](docs/architecture.md).
Build and Intel Mac validation: [docs/development.md](docs/development.md).
Performance instrumentation and Mac capture plan: [docs/performance.md](docs/performance.md).
Detail design and sample costs: [docs/detail.md](docs/detail.md).
One-session Intel Mac checklist: [docs/runtime-validation.md](docs/runtime-validation.md).
Twilight source audit and M6 decision: [docs/m5-m6-investigation.md](docs/m5-m6-investigation.md).
CameraService source audit and safe-override gate: [docs/camera-investigation.md](docs/camera-investigation.md).
