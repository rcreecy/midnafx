# Ambient contact shading and Remaster profile

Select **Profile / Remaster** at the top of MidnaFX settings. It enables natural
grading, detail, modern exploration FOV, AA, enhanced water and ambient occlusion.
Version 1.3.0 also includes [depth atmosphere](depth-atmosphere.md) in Remaster.
Select **Vanilla** to disable these profile effects. Geometry smoothing and depth
of field remain separate opt-ins. Existing installs keep their current settings.

Ambient occlusion reconstructs surface positions and normals from scene depth,
then samples a deterministic 16-point neighborhood. It adds restrained shading
at contacts and creases without temporal history. Strength defaults to 55%; the
radius defaults to 60 game units. The pass preserves scene alpha and depth and
runs after opaque geometry, before later translucent draws and the HUD.

Water snapshots explicitly request AO first. Submission is idempotent between
scene-begin callbacks, so native hook-slot reuse after reload cannot reverse
the effective AO/water order. Camera matrices come from the current stage view.
Unsupported partial viewports, multisampled targets, missing depth and scene
color formats other than RGBA8/BGRA8 bypass the effect.

This is screen-space ambient shading, not dynamic lighting, normal mapping or
off-screen shadowing. Screen edges and missing off-screen geometry limit it.
Motion quality across the entire game and high-resolution GPU cost remain open.
macOS support is best effort; native Metal gameplay is unavailable on this host.

## Windows observation, 2026-10-25

The native D3D12 preview uses the qualified local host, a 960x720 scene target,
Ordon `F_SP103,0,27,0`, and the existing external replacement pack. Matched paused
captures change only the AO switch; grading, camera, AA and water stay enabled.
The images show additional shading on character creases, rock/ground contacts
and scene geometry. They are JPEG captures, not lossless per-pixel HUD proof:
[AO off](evidence/ambient-occlusion-2026-10-25/off.jpg),
[AO on](evidence/ambient-occlusion-2026-10-25/on.jpg).

The first viewport guard used TV/HUD dimensions and wrongly bypassed ordinary
gameplay. The unchanged comparison caught this; the repaired guard uses the
native camera's framebuffer dimensions. All 21 Windows CTest entries pass,
including shader compilation and legacy/new preset persistence checks.
Native module reload restores the selected Remaster profile, submits AO again
and continues rendering with enhanced water. Normal window close exits with code 0.

New saved profiles use MFX4 to retain AO/AA/water switches. MFX1-MFX3 remain
readable and preserve scene switches when applied. AO strength/radius remain
advanced tuning rather than fields in the saved profile format.
