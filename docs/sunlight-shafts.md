# Sunlight shafts

MidnaFX 1.14 adds experimental **Sunlight shafts** across supported outdoor
scenes, without asset or stage lists. Select **Profile / Remaster**, or the
independent Basic toggle. Advanced lighting strength ranges from 0–25%, defaulting
to 10%. Vanilla disables the effect. Existing saved settings remain intact until
a profile is selected; earlier scene masks leave the new feature off.

## Rendering contract

The renderer reads the native sun packet's world position, color, alpha and
visibility. It does not synthesize a sun from bright pixels or the contact-shadow
light. Native weather and time-of-day visibility flow through those inputs.
Only initialized, non-hidden outdoor sky contexts with a player, an unpaused
world and an above-water camera qualify. Twilight is excluded. Off-screen,
behind-camera, invalid-matrix, missing-packet and zero-strength sources bypass
the effect. Native visibility fades smoothly after occlusion; this is not an
instantaneous visibility switch. The sun sprite may lie beyond the geometry
far plane, so projection uses its positive perspective w and on-screen direction.

Sixteen weighted samples gather opaque-depth sky visibility along the screen
segment toward the projected sun. Opaque silhouettes interrupt the radial field.
The field fades outside a radius of 0.65 viewport heights and as the sun approaches
the screen edge. Nearby opaque receivers receive less veiling than distant
surfaces or sky, using a 2,000-unit axial-distance falloff.

The composition adds native sun color in linear light, bounded by the remaining
color headroom and the 25% maximum control. Source alpha is exact. Sunlight follows
contact shading, color bounce and depth atmosphere in the existing opaque-stage
draw, preceding downstream water, grading, exposure, bloom, LUT and HUD processing.
It adds no targets, passes, asset identities or distributed textures. Shafts-only
operation uses existing neutral field bindings and does not dispatch AO/bounce
compute work. When no qualified source or other scene effect remains, the shared
draw and snapshots are skipped.

## Limits

This is a first-layer screen-space shaft approximation, not volumetric geometry
or light transport through a participating medium. Off-screen, hidden and
translucent occluders are unavailable from the opaque snapshot. Clouds and later
transparent effects cannot produce their own shaft occlusion; the native sun
visibility still modulates the whole effect. Camera motion, sampling and foliage
silhouettes can change the field. No temporal reconstruction is added.

The feature adds depth reads within the affected screen region. No universal GPU
cost or frame-rate improvement is claimed. Whole-game art and motion quality
remain unqualified. macOS gameplay and Metal execution remain unavailable;
support stays best effort through native package builds and portable checks.

See [the evidence ledger](evidence/sunlight-shafts-2026-10-29/README.md) for
projection, GPU and native integration proof.
