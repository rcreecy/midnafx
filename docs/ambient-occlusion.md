# Ambient contact shading and Remaster profile

Since 1.9, [directional contact shadows](contact-shadows.md) can share this field
and its filters. AO retains its independent strength and ambient shading role.
The field's green channel now combines validity with shadow attenuation; its
depth/normal reconstruction and native translucent/HUD ordering remain intact.

Select **Profile / Remaster** at the top of MidnaFX settings. It enables natural
grading, detail, modern exploration FOV, AA, enhanced water and ambient occlusion,
plus [depth atmosphere](depth-atmosphere.md), [adaptive exposure](adaptive-exposure.md)
and supported Link shading. Select **Vanilla** to disable these profile effects.
Rigid model smoothing and depth of field remain separate opt-ins. Existing
installs keep their current settings.

Version 1.6 reconstructs surface positions and normals from scene depth, then
samples 32 neighbors per half-resolution pixel. Each direction retains its
strongest distance-bounded occluder. Horizontal and vertical five-tap filters
reject separated surfaces and incompatible normals; full-resolution reconstruction
uses four field neighbors with surface-plane rejection. This reduces spatial
variation without temporal history, jitter or a motion-vector dependency.
Strength defaults to 55%; the
radius defaults to 60 game units. The pass preserves scene alpha and depth and
runs after opaque geometry, before later translucent draws and the HUD. The
radius cap scales with viewport height. Native full-precision depth is reused
through filtering rather than storing distance in half precision.

The two RGBA16Float fields cost about 8 MiB at 1080p or 32 MiB at 4K, plus retired
resources still referenced by queued callbacks during a resize. Three compute
dispatches precede the existing combined AO/atmosphere draw; no additional scene
snapshot or CPU readback is introduced. Disabled AO submits no compute. This
reorganizes the sampling work but does not establish a measured GPU speedup.
Completed old targets retire using callback counts; the host drains callbacks
before module shutdown releases the remaining resources. Failed AO retains the
independent atmosphere path through an invalid neutral field.

Water snapshots explicitly request AO first. Submission is idempotent between
scene-begin callbacks, so native hook-slot reuse after reload cannot reverse
the effective AO/water order. Camera matrices come from the current stage view.
Unsupported partial viewports, multisampled targets, missing depth and scene
color formats other than RGBA8/BGRA8 bypass the effect.

This is screen-space ambient shading, not dynamic lighting, normal mapping or
off-screen shadowing. Screen edges and missing off-screen geometry limit it.
Motion quality across the entire game and high-resolution GPU cost remain open.
Half resolution can miss very thin contacts. Off-screen occluders remain absent;
spatial filtering does not eliminate every motion-dependent sampling change.
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

## Windows observation, 2026-10-26

The filtered path passes nine focused GPU cases covering flat/background identity,
odd/one-pixel dimensions, raised contacts, spatial smoothing, separated-depth
rejection, fog and alpha. An added assertion proves that an invalid neutral field
retains fog even with positive AO strength. Native observations and artifact
identities are recorded in the [1.6 evidence](evidence/contact-shading-2026-10-26/README.md).
The retained comparison toggles the new AO path; it is not a matched benchmark
against the former 16-tap shader.
