# MidnaFX

MidnaFX is an experimental native visual processing mod for Dusklight. M3 provides a fused
pre-HUD grading shader with exposure, black point, contrast, gamma, saturation, highlight
rolloff, temperature and tint controls. Neutral grading bypasses the render pass. Save,
load, and duplicate looks in the Presets section; Vanilla restores neutral settings and
Custom tracks edits. A forced RGBA passthrough remains available for validating the render
path. Automatic Twilight profiles are later milestones. Windows builds have not been tested in
the game; Intel Mac/Metal validation is still required.

Download platform-specific packages from the latest successful
[Build MidnaFX workflow](https://github.com/rcreecy/midnafx/actions/workflows/build.yml):
`midnafx-windows-amd64` or `midnafx-macos-x86_64`. Extract the artifact ZIP to obtain
`midnafx.dusk`, then put that `.dusk` in Dusklight's mods folder. Each CI package contains
one platform's native library. A CI build does not establish in-game visual behavior.

Research and exact source revision: [docs/research.md](docs/research.md).
Architecture and limits: [docs/architecture.md](docs/architecture.md).
Build and Intel Mac validation: [docs/development.md](docs/development.md).
