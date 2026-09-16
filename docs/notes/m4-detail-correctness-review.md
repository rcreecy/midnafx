# M4.1–M4.2 independent correctness review

Reviewed the uncommitted detail/diagnostic shader, CPU parameter preparation, preset migration, renderer pipeline variants, UI, tests, packaging, and the pinned Dusklight GfxService 1.2 contract (`edf42c6`).

## Actionable findings

None identified in the reviewed source. The review does not establish in-game visual quality, Intel Metal runtime behavior, or the 4K frame-time target.

## Evidence and limits

- The WGSL uniform contains eight grading floats, two additional floats, and two `u32` values. The C++ `alignas(16)` struct is 48 bytes with detail at offset 32 and debug mode at offset 40. The game-thread `push_uniform` range is copied into the draw payload and bound by the render worker; passthrough omits that binding.
- The ordinary grading entry point reads one center texel. The selected detail entry point reads the center plus four clamped cardinal neighbors. Explicit clamping handles border pixels. All entry points return center alpha. The detail-off selection uses the one-read grading pipeline; neutral Final mode bypasses the snapshot and draw.
- Scene source is an observed RGBA8/BGRA8 UNORM snapshot. User control ranges keep grading divisors positive, gains and power operands finite for finite scene input, and detail strength within 0–0.5. The detail result is clamped before grading. There is no supported negative or HDR scene RGB in this path. These source checks do not prove visual absence of halos or noise amplification.
- A/B selection compares integer pixel `x` with `floor(width × percent / 100)`; the left side returns source before detail or grading. Luminance, clipping overlays, and absolute difference operate on processed RGB and preserve source alpha. The diagnostic thresholds are display-domain indicators, not physical luminance or HDR clipping measurements.
- Five pipelines share two shader modules per layout. Each payload holds an immutable pipeline-pair pointer. The pinned SDK states that resolved views are valid for the current frame; host shutdown unregisters and synchronizes draw callbacks before owned pipelines are released. The mod does not release borrowed scene/device handles.
- MFX2 encodes detail enabled/strength with bounds and retains MFX1 decoding, which defaults detail off. The smoke look is built in, cannot collide with persisted user preset names (slash is disallowed), and fits the 32-character persisted selection limit. Debug mode and split position are deliberately independent of the preset snapshot.
- The source shader is embedded through CMake configure dependencies; `src/config/visual.cpp` is included in the mod and test targets. The package test expects the updated manifest version. The workflow runs the package and parameter tests on Windows and Intel macOS. `shader_compile` uses Dawn's Null adapter and can skip if that adapter is unavailable; it is not an Intel Metal validation.

Runtime validation remains as specified in `docs/runtime-validation.md`, especially snapshots A–H, frame resize/transition, reload, Dawnlight ordering, high-resolution textures, and a target Intel Mac GPU capture.

## Follow-up correctness pass

During the M5 source pass, `register_presets()` was found to decode the saved
Custom snapshot and then overwrite it from current control variables whenever
Custom was selected. That could lose the separate stored Custom look on reload
if the variables and snapshot diverged. The overwrite was removed; the decoded
snapshot remains authoritative for the Custom selection. The M4 shader,
pipeline, uniform, and diagnostic paths were reread; no further actionable
M4 source defect was found. Windows build/tests, shader pipeline compilation,
formatting, and static analysis were rerun as part of the follow-up. This is
still not visual or target-Mac runtime validation.
