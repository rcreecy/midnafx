# Water scene reflections

MidnaFX 1.13 adds experimental **Water scene reflections** to supported enhanced
water. Nearby visible opaque scenery can replace the existing reflection tint
where a short reflected ray finds a qualified source. Select **Profile /
Remaster**, or enable both **Enhanced water** and **Water scene reflections** in
Basic. **Vanilla** disables them. Existing saved settings remain intact until
a profile is selected. The existing advanced reflection strength controls both
the scenery reflection and its tint fallback.

## Rendering contract

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
back to the original tint. Visible water cannot become a recursive reflection
source. A water-mask fragment hidden behind an opaque bank does not disqualify
that bank.

Source colors come from the pre-water snapshot, avoiding reflection of this
effect or the downstream HUD. Source-facing, edge, distance and thickness
confidence fade uncertain hits toward the original tint. The existing Fresnel
weight remains bounded at 45%; source alpha is preserved by the existing water
composition. Reflections retain their position after shoreline treatment and
before specular light, grading, exposure, bloom and LUT processing.

## Scope and limitations

This is a first-layer screen-space reflection, not a complete planar reflection
or ray-traced scene. Hidden, off-screen, sky and translucent geometry cannot be
reconstructed from the available opaque snapshot. Fine geometry can fall between
steps, depth-derived normals can fail at silhouettes and visible sources change
with camera motion. Misses deliberately retain the existing tint. No temporal
history or motion reconstruction is provided.

The effect covers the already supported, qualified horizontal-water surfaces.
It does not make arbitrary translucent materials reflective or establish support
for additional water identities. Existing high-resolution packs contribute their
rendered source colors without new texture metadata or redistributed payloads.
Ray sampling adds shader work on affected water pixels; no universal GPU cost or
frame-rate promise is made. macOS native gameplay remains unverified and support
is best effort through native builds and portable checks.

See [the evidence ledger](evidence/water-reflections-2026-10-28/README.md) for
geometric/color-transfer checks, native integration and remaining quality limits.
