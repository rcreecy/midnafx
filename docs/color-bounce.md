# Screen-space color bounce

MidnaFX 1.11 adds **Screen-space color bounce (experimental)** across visible
opaque scenery and characters. Nearby surfaces can contribute restrained color
to one another, using the native scene image and depth rather than an asset or
stage list. Select **Profile / Remaster** or enable the independent Basic toggle.
**Vanilla** disables it. Existing saved settings remain intact until a profile
is selected. Advanced lighting strength defaults to 12%, with a 0–30% range.

## Rendering contract

The effect gathers 16 nearby source samples within 180 native world units at
half resolution. Both source and receiver must face the connecting segment.
Invalid depth, missing geometry, off-screen sources and interrupted short depth
segments do not contribute. Fixed sample normalization avoids amplifying the
remaining sources near the screen border.

The existing contact-shading field supplies geometric normals. A separate
half-resolution RGBA16F color field receives the same surface-aware horizontal
and vertical filters. Full-resolution reconstruction follows the original depth
planes. The effect shares the opaque color/depth snapshot and final composition
with AO, contact shadows and atmosphere. Relative to active contact shading, it
adds one gather dispatch and two half-resolution color textures. Running bounce
alone also requires geometry generation, both filters and the shared composite.
When other contact effects run
with bounce disabled, two 1×1 binding placeholders replace those color targets
and the gather dispatch is skipped.

Source colors are converted from the display-encoded LDR scene into linear light.
Composition adds bounded energy after contact darkening and before independent
depth atmosphere. Both receiver color and available highlight headroom bound the
addition. Pure black and white endpoints are preserved without other effects.
Alpha stays unchanged. Water capture follows this pass; native translucent
effects and HUD render downstream. Pause and underwater cameras bypass bounce.
No temporal history, GPU readback or additional host patch is used.
From 1.17, optional [local light response](local-lighting.md) shares these color
fields and filters. When both effects run, their normalized contributions share
one maximum gain rather than adding two independent energy budgets. Bounce-only
behavior remains unchanged when local lighting is off.
The energy bound applies at this opaque composite. Native downstream motion blur,
bloom and other game rendering can change the final displayed difference.

## Scope and limitations

This is a screen-space approximation, not a physically based global illumination
system. The source image already contains native lighting and fog. It does not
provide material albedo, roughness, emissive classification or hidden surfaces.
The lit receiver color is only a conservative reflectance proxy. Geometric
normals reconstructed from visible depth can miss thin surfaces and mesh detail;
two visibility samples cannot resolve every intervening surface. Off-screen
sources cannot contribute, and camera movement can change visible contributions.

The feature supplements native lighting without changing geometry, material
identities, gameplay or source textures. Existing user-supplied high-resolution
replacements participate through their rendered colors. No game or third-party
texture payloads ship. Broad motion/art quality and GPU duration remain open.
macOS native gameplay remains unverified and support is best effort through
native builds and portable checks.

See the [artifact and evidence ledger](evidence/color-bounce-2026-10-28/README.md)
for focused geometric, composition and native runtime results.
The current native screenshots retain uncontrolled image variation even with
unchanged effect settings; they do not establish an isolated color-bounce quality
comparison. The deterministic GPU fixtures establish the bounded color transfer.
