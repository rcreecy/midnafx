# Water scene reflections

MidnaFX 1.13 adds experimental **Water scene reflections** to supported enhanced
water. Nearby visible opaque scenery can replace the existing reflection tint
where a short reflected ray finds a qualified source. Select **Profile /
Remaster**, or enable both **Enhanced water** and **Water scene reflections** in
Basic. **Vanilla** disables them. Existing saved settings remain intact until
a profile is selected. The existing advanced reflection strength controls both
scenery and sky reflections.

## Rendering contract

### November 10 water replay correction

The horizontal gray band seen in Faron dialogue was a water-mask projection
error, rather than a mist boundary. Surface packets were retained during world
rendering and replayed before the HUD, after native 2D setup had changed the
projection to orthographic. The replay now retains each packet's original
perspective projection, applies it immediately before drawing that packet, and
restores the incoming projection afterward. Invalid perspective data or more
than 64 retained packets bypass the enhanced capture for that frame.

The mask also respects the world viewport's normalized scissor rectangle.
Without that restriction, valid water geometry behind native cutscene trim
could recolor black letterbox pixels. Capture uses target-pixel scissor bounds
and restores the native scissor afterward. Unsupported or empty viewport bounds
bypass capture. This adds no shader bindings, render targets, or draw passes.
Waterfall sheets, mist, and contact effects retain their existing behavior.

Isolation previews reproduced the band with both waterfall overlay passes
disabled and removed it with enhanced water disabled. A temporary projection
trace confirmed an orthographic incoming projection and a perspective captured
projection. Final Windows D3D12 previews remove the band in Faron dialogue and
preserve black bars in the Lake Hylia cave dialogue while retaining water and
waterfall effects. Both close with exit code zero; Faron records 548 successful
captures and the cave 699, with zero capture failures or logged GPU validation
errors. Water identity, preset round-trip, and package-contract checks pass.

The final package SHA-256 is
`ACFAD36560D8A42562779EA00779831E1181F849331E1FE075176547614B0C60`.
Local final screenshots and logs are in
`build/material-relighting/faron-seam-complete/` (`faron-repaired.png`) and
`build/material-relighting/seam-cave-clipped/` (`cave-repaired.png`). Earlier
`faron-seam-*` isolation/projection trials and `seam-regression-cave` are
intermediate builds. Temporary overlay disables and trace logging are removed.
Whole-game cutscene transitions, alternate host recording/pillarbox settings,
and native macOS remain unverified.

The feature reuses the existing water identity classification, exact surface
mask/depth replay, pre-water opaque color/depth snapshot and animated horizontal
water normals. It adds no asset identities, texture captures, render targets or
passes. A forward projection matrix is appended to the mod's private water
uniform block. Existing grading offsets and all prior controls retain their
meaning.

Sixteen quadratic-distance ray steps trace up to 1,500 native world units in
view space. Four binary refinements locate a candidate depth crossing. The
candidate must remain within the ray range, close to the ray and sampled depth,
above the local water plane and on a camera-visible surface facing the reflected
ray. Background, invalid projection, off-screen and near-plane crossings fall
back to the environment. Visible water cannot become a recursive reflection
source. A water-mask fragment hidden behind an opaque bank does not disqualify
that bank.

Source colors come from the pre-water snapshot, avoiding reflection of this
effect or the downstream HUD. Source-facing, edge, distance and thickness
confidence fade uncertain hits toward a directional gradient built from the
scene sky and horizon. The native sun adds a soft highlight when visible.
Interiors and scenes without an active sky use the tint. The existing Fresnel
weight remains bounded at 45%; source alpha is preserved by the existing water
composition. Reflections retain their position after shoreline treatment and
before specular light, grading, exposure, bloom and LUT processing.

## Scope and limitations

This is a first-layer screen-space reflection, not a complete planar reflection
or ray-traced scene. Hidden, off-screen, sky and translucent geometry cannot be
reconstructed from the available opaque snapshot. Fine geometry can fall between
steps, depth-derived normals can fail at silhouettes and visible sources change
with camera motion. No temporal
history or motion reconstruction is provided.

Two slow swells and finer wind ripples drive the shared surface normal. The
fine detail fades with distance. Fishing Pond uses lower wave exposure than
Lake Hylia. Windows D3D12 runs confirmed sky fallback active in both scenes,
including the Lake Hylia Twilight palette. GPU timing and macOS gameplay
remain unverified.

The effect covers the already supported, qualified horizontal-water surfaces.
It does not make arbitrary translucent materials reflective or establish support
for additional water identities. Existing high-resolution packs contribute their
rendered source colors without new texture metadata or redistributed payloads.
Ray sampling adds shader work on affected water pixels; no universal GPU cost or
frame-rate promise is made. macOS native gameplay remains unverified and support
is best effort through native builds and portable checks.

See [the evidence ledger](evidence/water-reflections-2026-10-28/README.md) for
geometric/color-transfer checks, native integration and remaining quality limits.
## Wet rock and sunlit shallows

Enhanced water uses the shoreline strength control for two contact effects.
The opaque pass darkens and adds a restrained sheen around sampled banks
and mapped waterfall spray. Its receiver mask uses position, depth, and
subdued color as a stone proxy. The water composite adds moving caustics
where the water capture has a visible shallow floor and native sunlight.
The pattern fades with depth, distance, and weak sunlight.

Waterfall sheets are discovered by material and geometry in loaded room
backgrounds and background objects. Water-surface capture also scans every
loaded stage, preserving verified special-case identities and admitting known
surface families when the native blend and depth state match. This gives
ripples and caustics the same coverage as the resulting water capture.
