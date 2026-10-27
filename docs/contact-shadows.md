# Directional contact shadows

MidnaFX 1.9 adds short directional contact shadows across opaque scenery and
characters, without model identities or stage lists. Select **Profile / Remaster**
or enable **Contact shadows** in Basic. Strength defaults to 30%, with a 0–50%
range. Vanilla disables it. Existing installations retain their saved settings;
selecting Remaster enables the new effect. Strength tuning stays unchanged across
profile switches. Saved profiles from earlier versions remain readable.

The effect complements ambient occlusion. AO darkens local enclosed contacts;
contact shadows trace toward the native environment base light, so nearby visible
geometry can cast a short directional shadow. The native base-light position
follows the game's sun/moon or environment selection. It is a shared scene light,
not every object's material-specific or local torch light. Native shadows and
lighting remain; this effect adds a bounded screen-space approximation.

## Rendering and controls

The existing half-resolution AO computation reconstructs opaque depth and surface
normals. Twelve deterministic projected samples trace toward the light, over at
most 120 native world units and never beyond the light. A normal offset and
depth-thickness tolerance reduce self-shadow artifacts. Low-resolution distant
contacts, back-facing light and missing depth contribute no new shadow. Samples
fade near the viewport boundary and the end of the contact range.

AO and shadows have separate strengths. They share the two existing half-resolution
RGBA16Float fields, two surface-aware filters and full-resolution reconstruction.
The field stores AO visibility in red, validity plus shadow amount in green
(zero invalid, otherwise one plus attenuation), and octahedral normal in blue/alpha.
Native full-precision depth stays separate. Combined AO/shadow darkening is capped
at 50%; independent depth atmosphere composes afterward. Scene alpha is unchanged.
Native translucent/water and HUD draws retain their downstream order.

Shadows work with AO and manual grading disabled. With AO disabled, its 32-tap
loop is skipped. With shadows disabled, no ray samples run. Unavailable compute
or targets preserve native shading and independent fog. Underwater cameras,
native pause, absent player and unsupported viewports bypass contact shadows.
No host patch, temporal history or CPU readback is added. Shadows use the existing
AO passes and targets. When AO was disabled, enabling shadows runs that pipeline
and allocates the existing AO fields
(about 8 MiB at 1080p or 32 MiB at 4K). Enabling shadows alongside AO adds up to
twelve projected depth samples per half-resolution pixel. GPU duration and 4K
performance remain unmeasured.

## Limits and evidence

Only visible opaque depth is available. Off-screen/hidden casters and transparent
surfaces cannot cast reliable new shadows. Half-resolution representatives and
twelve samples can miss thin contacts, while depth-thickness tolerance can create
false hits or leaks. Native baked or dynamic shadows can overlap the added effect.
This is not a shadow-map replacement, full dynamic lighting, material-aware
relighting or whole-game motion qualification.

[The evidence ledger](evidence/contact-shadows-2026-10-27/README.md) records focused
GPU geometry checks and native Windows observations. macOS support remains best
effort through native builds and portable checks. Gameplay and Metal execution
are unverified; no hosted Metal runtime check is required.
