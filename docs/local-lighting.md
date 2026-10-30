# Native local-light response

MidnaFX 1.17 adds experimental **Local light response** to Basic. Remaster enables
it at 15% strength; Vanilla disables it. Advanced lighting strength ranges from
0 to 30%. Existing saved settings stay intact until the control or a profile is
selected. Earlier scene masks keep the new feature off, while grading-only looks
retain their existing scene-effect contract.

## Supported sources

The native ordinary point-light registry supplies world positions, signed color
channels and influence radii. This includes supported torch, campfire and stage
lights without new model identities or stage lists. MidnaFX copies current values
on the game thread; actor pointers never enter GPU payloads or persist between
frames. Native registration, deletion, movement, color and radius changes remain
owned by the game. No light or source texture is created by the product.

The bounded selector scans the 100 native slots and keeps at most four positive
ordinary sources, ranked by camera-relative radius coverage and brightest color
channel. Equal scores retain registry order. Source radius must be above 0.01 and
at most 6,000 world units. Non-finite/extreme positions, negative colors, zero or
negative indices and special index bit 0x8000 are excluded. Positive color channels
are clamped to the native display range. Base sunlight, effect-light and native
lantern/boss spotlight registries are not interpreted as ordinary point lights.
The lantern uses distinct attenuation and active-state semantics and is not
supported by this slice. Per-object native fluctuation/randomness and material
type exceptions are not reproduced.

## Rendering contract

The existing half-resolution contact-lighting field provides depth-derived
geometric normals. A source contributes Lambert-facing illumination with squared
radial falloff to zero at its native radius. Eight projected samples inspect the
biased receiver/light segment for visible opaque blockers. Samples outside the
screen or valid projection cannot establish new illumination. Reconstructed
blockers must lie before the light, within a finite depth/ray tolerance. A source
inside the normal-bias distance contributes nothing.

Native source colors are converted from display encoding to linear light. The
lit scene color is a conservative reflectance proxy, not material albedo. Local
and color-bounce illumination share one normalized field and a maximum gain equal
to the larger selected strength, capped at 30%. Receiver color and remaining
highlight headroom bound the added energy. Pure black/white endpoints and alpha
are preserved when independent effects are off. Native lighting is retained and
can already include some of the same source contribution.

The feature reuses the bounce color textures, gather dispatch, both surface-aware
filters and opaque composite. With bounce already running it adds source work
inside that gather, with no new target or dispatch. Local-only operation activates
the existing geometry/gather/two-filter pipeline and two half-resolution geometry
plus two color fields. At 1080p those fields total approximately 16 MiB; at 4K,
approximately 64 MiB, excluding temporary retirement during resizing. No temporal
history, CPU readback, extra fullscreen draw or host patch is added. With no
supported source and bounce off, no color-lighting work is requested. Disabled
local lighting performs no native registry scan.

HUD and native translucent/water ordering remain unchanged. Pause, absent player,
event sequences, underwater camera, unsupported viewport and unavailable compute
select bypass. Grading, AO and contact shadows can remain disabled independently.

## Limits and evidence

This is an LDR screen-space supplement, not physically based material relighting,
emissive detection, a shadow-map replacement or a new lantern renderer. Hidden or
off-screen blockers are unavailable; eight samples and finite thickness can miss
thin geometry or reject valid light. Four-source selection can change during
camera movement. Whole-game motion, native artistic quality, GPU duration and 4K
performance remain unqualified.

See [the evidence ledger](evidence/local-lighting-2026-10-29/README.md) for source
anchors, portable selection tests, GPU bounds/visibility and native integration.
The audit fixture creates a temporary registry source only for native validation;
it does not ship or establish complete naturally placed torch coverage.
macOS remains best effort through native package builds and portable checks.
Native gameplay and Metal execution are unverified on this Windows host.
